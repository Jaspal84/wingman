#!/usr/bin/env python3
"""Offline, local-checkpoint HTDemucs worker. No automatic model downloads."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import sys
import tempfile
import time

MODEL_FILE = '955717e8-8726e21a.th'
MODEL_PREFIX = '8726e21a'
MODEL_DIRECTORY = 'htdemucs-v4.0.1-955717e8'
MODEL_URL = 'https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/' + MODEL_FILE


def checksum(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''): digest.update(block)
    return digest.hexdigest()


def verify_model(directory):
    directory = Path(directory)
    if not (directory/'wingman-model.json').is_file(): directory = directory/MODEL_DIRECTORY
    manifest = json.loads((directory/'wingman-model.json').read_text())
    if manifest.get('model') != 'htdemucs' or manifest.get('demucs') != '4.0.1':
        raise ValueError('Unsupported model manifest/version')
    expected = manifest.get('sha256', '')
    if len(expected) != 64 or not expected.startswith(MODEL_PREFIX):
        raise ValueError('Manifest lacks the full SHA-256 for the official checkpoint')
    model = directory/MODEL_FILE
    if checksum(model) != expected: raise ValueError('Model checksum mismatch')
    return model


def write_status(path, **data):
    if path:
        target = Path(path)
        target.parent.mkdir(parents=True,exist_ok=True)
        temp = target.with_suffix('.writing')
        temp.write_text(json.dumps(data, allow_nan=False))
        temp.replace(target)
    print(json.dumps(data, allow_nan=False), flush=True)


def separate(args):
    began = time.monotonic()
    def cancelled():
        if args.cancel and Path(args.cancel).exists(): raise InterruptedError('Separation cancelled')
    cancelled()
    model_path = verify_model(args.models)  # Validate before importing torch or loading pickle weights.
    import numpy as np
    import soundfile as sf
    import torch
    import torchaudio
    import demucs
    from demucs.pretrained import get_model
    from demucs.apply import apply_model
    if demucs.__version__ != '4.0.1' or torch.__version__.split('+')[0] != '2.5.1' or torchaudio.__version__.split('+')[0] != '2.5.1':
        raise ValueError('Worker requires Demucs 4.0.1 / torch and torchaudio 2.5.1')
    torch.set_num_threads(2)
    info = sf.info(args.input)
    if info.channels not in (1,2) or not 8000 <= info.samplerate <= 192000 or not 0 < info.frames <= info.samplerate*120:
        raise ValueError('Input must be mono/stereo, 8-192 kHz and 0-120 seconds')
    samples, rate = sf.read(args.input, dtype='float32', always_2d=True)
    if not np.isfinite(samples).all(): raise ValueError('Nonfinite input audio')
    wave = torch.from_numpy(samples.T.copy())
    if wave.shape[0] == 1: wave = wave.repeat(2,1)
    # Verified native PyTorch checkpoint. Do not claim CoreML/ONNX compatibility.
    model = get_model('955717e8', repo=model_path.parent)
    if set(model.sources) != {'drums','bass','other','vocals'} or model.samplerate != 44100:
        raise ValueError('Unexpected model sources/sample rate')
    model.cpu().eval()
    if rate != model.samplerate: wave = torchaudio.functional.resample(wave,rate,model.samplerate)
    mean = wave.mean(); std = wave.std().clamp_min(1e-6)
    wave = (wave-mean)/std
    length = wave.shape[-1]
    segment = min(float(model.segment),args.segment)
    # HTDemucs retains its training-window requirement internally. Chunking bounds
    # the input/output queue; peak activation RAM still needs a hardware benchmark.
    size = int(segment*model.samplerate)
    stride = int(size*0.75)
    estimates = torch.zeros(4,2,length)
    weights = torch.zeros(length)
    triangle = torch.minimum(torch.arange(1,size+1),torch.arange(size,0,-1)).float()
    triangle /= triangle.max()
    inference_began = time.monotonic()
    for offset in range(0,length,stride):
        cancelled()
        end = min(length,offset+size)
        chunk = wave[:,offset:end]
        with torch.inference_mode():
            prediction = apply_model(model,chunk[None],shifts=0,split=False,device='cpu',segment=segment)[0]
        weight = triangle[:end-offset]
        estimates[:,:,offset:end] += prediction[:,:,:end-offset]*weight
        weights[offset:end] += weight
        write_status(args.status,state='running',progress=min(0.95,end/length*0.95))
    cancelled()
    estimates /= weights.clamp_min(1e-8)
    estimates = estimates*std+mean/4
    output = Path(args.output)
    output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()): raise ValueError('Output directory must be empty')
    try:
        statistics = {}
        for i, source in enumerate(model.sources):
            cancelled()
            stem = estimates[i].T.numpy()
            if not np.isfinite(stem).all(): raise ValueError('Model returned nonfinite audio')
            # PCM conversion saturates rather than wraps. All files have identical length/rate.
            stem = np.clip(stem,-1,1)
            statistics[source] = {'rms':float(np.sqrt(np.mean(stem.astype(np.float64)**2))),'peak':float(np.max(np.abs(stem)))}
            sf.write(output/(source+'.wav'),stem,model.samplerate,subtype='PCM_24')
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        peak_bytes = rss if sys.platform == 'darwin' else rss*1024
        result = dict(state='done',progress=1.0,seconds=time.monotonic()-began,peak_rss_bytes=peak_bytes,
                      sample_rate=model.samplerate,frames=length,sources=model.sources,backend='PyTorch CPU',
                      inference_and_export_seconds=time.monotonic()-inference_began,stem_statistics=statistics)
        (output/'benchmark.json').write_text(json.dumps(result,indent=2))
        write_status(args.status,**result)
    except BaseException:
        for name in ('vocals.wav','drums.wav','bass.wav','other.wav','benchmark.json'):
            (output/name).unlink(missing_ok=True)
        raise


def install(args):
    if not args.allow_download or not args.accept_mit_license:
        raise ValueError('Explicit --allow-download and --accept-mit-license are required')
    from urllib.request import urlopen
    directory = Path(args.models); directory.mkdir(parents=True,exist_ok=True)
    destination = directory/MODEL_DIRECTORY
    if destination.exists() or (directory/MODEL_FILE).exists() or (directory/'wingman-model.json').exists():
        raise ValueError('Refusing to overwrite an existing model installation')
    staging = getattr(args,'staging',None)
    if staging:
        staging = Path(staging)
        if staging.parent.resolve() != directory.resolve(): raise ValueError('Staging must be a private child of the model directory')
        staging.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.wingman-model-',dir=staging or directory) as temp:
        payload = Path(temp)/'payload'; payload.mkdir()
        checkpoint = payload/MODEL_FILE
        with urlopen(MODEL_URL,timeout=60) as response, checkpoint.open('wb') as out:
            size = 0
            expected = int(response.headers.get('Content-Length',0))
            while block := response.read(1024*1024):
                size += len(block)
                if getattr(args,'cancel',None) and Path(args.cancel).exists(): raise InterruptedError('Model installation cancelled')
                if size > 200*1024*1024: raise ValueError('Model exceeds 200 MB download limit')
                out.write(block)
                write_status(getattr(args,'status',None),state='running',progress=min(0.95,size/expected*0.95) if expected else 0,message=f'Downloaded {size//(1024*1024)} MB')
        digest = checksum(checkpoint)
        if not digest.startswith(MODEL_PREFIX): raise ValueError('Official model checksum prefix mismatch')
        if args.sha256 and digest != args.sha256: raise ValueError('Pinned full SHA-256 mismatch')
        manifest = dict(model='htdemucs',demucs='4.0.1',sha256=digest,url=MODEL_URL,bytes=size,
                        license='MIT',runtime='torch 2.5.1 / torchaudio 2.5.1 CPU')
        (payload/'wingman-model.json').write_text(json.dumps(manifest,indent=2))
        payload.rename(destination)  # Publish checkpoint and manifest together on the same filesystem.
    print(json.dumps(manifest,indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('check','install','separate'))
    parser.add_argument('--models',type=Path,required=True)
    parser.add_argument('--input',type=Path); parser.add_argument('--output',type=Path)
    parser.add_argument('--staging',type=Path)
    parser.add_argument('--status',type=Path); parser.add_argument('--cancel',type=Path)
    parser.add_argument('--segment',type=float,default=4)
    parser.add_argument('--sha256',default='')
    parser.add_argument('--allow-download',action='store_true')
    parser.add_argument('--accept-mit-license',action='store_true')
    args = parser.parse_args()
    try:
        if args.action == 'check':
            verify_model(args.models); write_status(args.status,state='done',progress=1,message='Local checksum verified')
        elif args.action == 'install':
            install(args); write_status(args.status,state='done',progress=1,message='Model installation complete')
        else:
            if not args.input or not args.output or not math.isfinite(args.segment) or not 1 <= args.segment <= 7.8:
                raise ValueError('Separation requires input, output and a segment of 1-7.8 seconds')
            separate(args)
        return 0
    except Exception as error:
        write_status(args.status,state='cancelled' if isinstance(error,InterruptedError) else 'error',message=str(error),progress=0)
        return 2 if isinstance(error,InterruptedError) else 1

if __name__ == '__main__': sys.exit(main())

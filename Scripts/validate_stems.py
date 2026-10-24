#!/usr/bin/env python3
"""Validate exported stems; numerical mixture consistency is not source-level SDR."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import numpy as np
import soundfile as sf

SOURCES=('vocals','drums','bass','other')

def sha256(path):
    result=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):result.update(block)
    return result.hexdigest()

def validate(input_path,output_directory):
    reference,rate=sf.read(input_path,dtype='float32',always_2d=True)
    if reference.shape[1] not in (1,2) or not np.isfinite(reference).all() or len(reference)==0:
        raise ValueError('Invalid reference audio')
    if reference.shape[1]==1:reference=np.repeat(reference,2,axis=1)
    expected=math.ceil(len(reference)*44100/rate)
    if rate!=44100:
        import torch
        import torchaudio
        reference=torchaudio.functional.resample(torch.from_numpy(reference.T.copy()),rate,44100).numpy().T
    mixture_rms=float(np.sqrt(np.mean(reference.astype(np.float64)**2)))
    if mixture_rms<1e-12:raise ValueError('Reference music is silent')
    report={'input_sha256':sha256(input_path),'input_sample_rate':rate,'expected_output_frames':expected,
            'output_sample_rate':44100,'duration_seconds':expected/44100,'input_rms':mixture_rms,
            'stems':{},'warnings':[], 'metric_scope':'No isolated reference stems: no source SDR/SI-SDR or perceptual accuracy is claimed.'}
    outputs={}
    for source in SOURCES:
        path=Path(output_directory)/(source+'.wav');info=sf.info(path)
        if (info.samplerate,info.channels,info.frames,info.format,info.subtype)!=(44100,2,expected,'WAV','PCM_24'):
            raise ValueError(source+': expected synchronized stereo 44.1-kHz PCM_24 WAV')
        data,_=sf.read(path,dtype='float32',always_2d=True)
        if not np.isfinite(data).all():raise ValueError(source+': nonfinite samples')
        rms=float(np.sqrt(np.mean(data.astype(np.float64)**2)))
        if rms<1e-12:raise ValueError(source+': no nonzero signal in this acceptance fixture')
        clipped=float(np.mean(np.abs(data)>=1-1/8388608))
        if clipped>0:report['warnings'].append(source+': saturated PCM samples present')
        if rms<mixture_rms*0.01:report['warnings'].append(source+': very quiet; this fixture does not establish semantic quality for this source')
        report['stems'][source]={'sha256':sha256(path),'frames':info.frames,'channels':info.channels,'subtype':info.subtype,
                                 'rms':rms,'peak':float(np.max(np.abs(data))),'clipped_sample_fraction':clipped}
        outputs[source]=data
    correlations={}
    for i,a in enumerate(SOURCES):
        for b in SOURCES[i+1:]:
            if np.array_equal(outputs[a],outputs[b]):raise ValueError('Duplicate stem signals: '+a+'/'+b)
            x=outputs[a].ravel().astype(np.float64);y=outputs[b].ravel().astype(np.float64)
            x-=x.mean();y-=y.mean();denominator=np.linalg.norm(x)*np.linalg.norm(y)
            correlations[a+'/'+b]=float(np.dot(x,y)/denominator) if denominator else 0.0
    total=sum(outputs.values());residual=reference-total
    error_rms=float(np.sqrt(np.mean(residual.astype(np.float64)**2)))
    report['pairwise_correlations']=correlations
    report['mixture_residual_rms']=error_rms
    report['mixture_reconstruction_snr_db']=20*math.log10(mixture_rms/max(error_rms,1e-15))
    report['structural_checks_passed']=True
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input',type=Path);parser.add_argument('output_directory',type=Path)
    parser.add_argument('--report',type=Path,required=True);args=parser.parse_args()
    try:report=validate(args.input,args.output_directory)
    except (ValueError,RuntimeError,OSError) as error:
        print('Validation failed: '+str(error));return 1
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())

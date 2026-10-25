import argparse
import hashlib
import importlib.util
from pathlib import Path
import json
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('stem_worker',Path(__file__).resolve().parents[1]/'Scripts/stem_worker.py')
worker = importlib.util.module_from_spec(spec); spec.loader.exec_module(worker)
class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup); self.root = Path(self.temp.name)
    def test_missing_installation(self):
        with self.assertRaises(FileNotFoundError): worker.verify_model(self.root)
    def test_invalid_manifest(self):
        (self.root/'wingman-model.json').write_text(json.dumps({'model':'other'}))
        with self.assertRaises(ValueError): worker.verify_model(self.root)
    def test_corrupted_checkpoint(self):
        (self.root/'wingman-model.json').write_text(json.dumps({'model':'htdemucs','demucs':'4.0.1','license':'MIT','sha256':worker.MODEL_SHA256}))
        (self.root/worker.MODEL_FILE).write_bytes(b'not an actual checkpoint')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'): worker.verify_model(self.root)
    def test_tampered_manifest_cannot_repin_checkpoint(self):
        (self.root/'wingman-model.json').write_text(json.dumps({'model':'htdemucs','demucs':'4.0.1','license':'MIT','sha256':worker.MODEL_PREFIX+'0'*56}))
        with self.assertRaisesRegex(ValueError,'pinned official'): worker.verify_model(self.root)
    def test_unexpected_license_rejected(self):
        (self.root/'wingman-model.json').write_text(json.dumps({'model':'htdemucs','demucs':'4.0.1','license':'unknown','sha256':worker.MODEL_SHA256}))
        with self.assertRaisesRegex(ValueError,'license metadata'): worker.verify_model(self.root)
    def test_license_text_matches_pinned_notice(self):
        self.assertEqual(hashlib.sha256(worker.MODEL_LICENSE_TEXT.encode()).hexdigest(),worker.MODEL_LICENSE_SHA256)
    def test_download_requires_explicit_consent(self):
        with self.assertRaisesRegex(ValueError,'Explicit'): worker.install(argparse.Namespace(allow_download=False,accept_mit_license=True))
    def test_checksum(self):
        path=self.root/'data'; path.write_bytes(b'known data')
        self.assertEqual(worker.checksum(path),hashlib.sha256(b'known data').hexdigest())
    def test_cancel_before_loading_model(self):
        flag=self.root/'cancel'; flag.touch()
        with self.assertRaises(InterruptedError): worker.separate(argparse.Namespace(cancel=flag))
    def test_existing_output_preserved_before_model_load(self):
        source=self.root/'input.wav';source.write_bytes(b'fixture')
        output=self.root/'output';output.mkdir();sentinel=output/'vocals.wav';sentinel.write_bytes(b'keep')
        with self.assertRaisesRegex(ValueError,'existing files are preserved'):
            worker.separate(argparse.Namespace(cancel=None,input=source,output=output))
        self.assertEqual(sentinel.read_bytes(),b'keep')
    def test_missing_input_before_model_load(self):
        with self.assertRaisesRegex(FileNotFoundError,'Input audio'):
            worker.separate(argparse.Namespace(cancel=None,input=self.root/'absent.wav'))
    def test_status_replaces_previous_state(self):
        path=self.root/'status.json'; worker.write_status(path,state='running',progress=0.5)
        worker.write_status(path,state='cancelled',progress=0)
        self.assertEqual(json.loads(path.read_text())['state'],'cancelled')
        self.assertFalse(path.with_suffix('.writing').exists())
if __name__ == '__main__': unittest.main()

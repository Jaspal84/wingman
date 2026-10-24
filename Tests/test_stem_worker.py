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
        (self.root/'wingman-model.json').write_text(json.dumps({'model':'htdemucs','demucs':'4.0.1','sha256':worker.MODEL_PREFIX+'0'*56}))
        (self.root/worker.MODEL_FILE).write_bytes(b'not an actual checkpoint')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'): worker.verify_model(self.root)
    def test_download_requires_explicit_consent(self):
        with self.assertRaisesRegex(ValueError,'Explicit'): worker.install(argparse.Namespace(allow_download=False,accept_mit_license=True))
    def test_checksum(self):
        path=self.root/'data'; path.write_bytes(b'known data')
        self.assertEqual(worker.checksum(path),hashlib.sha256(b'known data').hexdigest())
    def test_cancel_before_loading_model(self):
        flag=self.root/'cancel'; flag.touch()
        with self.assertRaises(InterruptedError): worker.separate(argparse.Namespace(cancel=flag))
    def test_status_replaces_previous_state(self):
        path=self.root/'status.json'; worker.write_status(path,state='running',progress=0.5)
        worker.write_status(path,state='cancelled',progress=0)
        self.assertEqual(json.loads(path.read_text())['state'],'cancelled')
        self.assertFalse(path.with_suffix('.writing').exists())
if __name__ == '__main__': unittest.main()

import importlib.util
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('manifest',Path(__file__).resolve().parents[1]/'Scripts/guitar-library-manifest.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
class ManifestTests(unittest.TestCase):
    def test_hash_and_license_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'instrument.sfz').write_text('<region> sample=note.wav')
            (root/'note.wav').write_bytes(b'synthetic fixture, not a production recording')
            license=root/'LICENSE';license.write_text('Original test fixture')
            data=module.inventory(root,license,'Original fixture')
            self.assertEqual(len(data['files']),2)
            self.assertEqual(data['files'][1]['sha256'],module.digest(root/'note.wav'))
            self.assertIn('not automatic legal approval',data['license_review'])
    def test_missing_license_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(OSError):module.inventory(folder,Path(folder)/'missing','Unknown')
if __name__=='__main__':unittest.main()

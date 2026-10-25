import importlib.util
from pathlib import Path
import plistlib
import tempfile
import unittest
spec = importlib.util.spec_from_file_location('installer', Path(__file__).resolve().parents[1] / 'Scripts/install-standalone-macos.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)

class AppMetadata(unittest.TestCase):
    def test_bundle_checks(self):
        with tempfile.TemporaryDirectory() as tmp:
            app = Path(tmp) / 'JASWINGMAN PRO.app'
            (app / 'Contents/MacOS').mkdir(parents=True)
            plist = app / 'Contents/Info.plist'
            plist.write_bytes(plistlib.dumps({'CFBundleExecutable':'JASWINGMAN PRO'}))
            with self.assertRaises(ValueError): installer.inspect(app)
            binary = app / 'Contents/MacOS/JASWINGMAN PRO'
            binary.write_text('fixture, not a Mach-O executable')
            binary.chmod(0o755)
            self.assertEqual(installer.inspect(app)[1],binary)
            plist.write_bytes(plistlib.dumps({'CFBundleExecutable':'../escape'}))
            with self.assertRaises(ValueError): installer.inspect(app)
    def test_platform_guard(self):
        if installer.sys.platform != 'darwin':
            with self.assertRaises(ValueError): installer.install('/missing.app')

if __name__ == '__main__': unittest.main()

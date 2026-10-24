import importlib.util
from pathlib import Path
import plistlib
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('macos_au', Path(__file__).resolve().parents[1] / 'Scripts/macos_au.py')
au = importlib.util.module_from_spec(spec)
spec.loader.exec_module(au)

class MetadataTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.bundle = Path(self.temp.name) / 'Wingman Pro.component'
        (self.bundle / 'Contents/MacOS').mkdir(parents=True)
        (self.bundle / 'Contents/MacOS/Wingman Pro').write_bytes(b'fixture, not a Mach-O')
        self.plist = {'CFBundleIdentifier': 'com.wingmanaudio.wingman', 'CFBundleExecutable': 'Wingman Pro',
                      'AudioComponents': [{'type': 'aumf', 'subtype': 'Wgm1', 'manufacturer': 'Wgma', 'factoryFunction': 'WingmanAUFactory'}]}
        self.write()
    def write(self):
        with (self.bundle / 'Contents/Info.plist').open('wb') as f: plistlib.dump(self.plist, f)
    def test_actual_ids(self):
        info = au.metadata(self.bundle)
        self.assertEqual((info['type'],info['subtype'],info['manufacturer']),('aumf','Wgm1','Wgma'))
    def test_not_guessed(self):
        self.plist['AudioComponents'][0]['type'] = 'aumu'; self.write()
        self.assertEqual(au.metadata(self.bundle)['type'],'aumu')
    def test_reject_invalid_fourcc(self):
        self.plist['AudioComponents'][0]['subtype'] = 'WRONG'; self.write()
        with self.assertRaises(ValueError): au.metadata(self.bundle)
    def test_missing_binary(self):
        (self.bundle/'Contents/MacOS/Wingman Pro').unlink()
        with self.assertRaises(ValueError): au.metadata(self.bundle)
    def test_ambiguous_components(self):
        self.plist['AudioComponents'].append(self.plist['AudioComponents'][0]); self.write()
        with self.assertRaises(ValueError): au.metadata(self.bundle)
    def test_reject_executable_escape(self):
        self.plist['CFBundleExecutable'] = '../Info.plist'; self.write()
        with self.assertRaises(ValueError): au.metadata(self.bundle)
if __name__ == '__main__': unittest.main()

import importlib.util
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('migration',Path(__file__).resolve().parents[1]/'Scripts/migrate-standalone-settings.py')
migration=importlib.util.module_from_spec(spec);spec.loader.exec_module(migration)
class MigrationTests(unittest.TestCase):
    def test_copy_preserves_bytes_and_legacy(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);data=b'JUCE fixture\x00legacy';old=root/'Wingman Pro.settings';old.write_bytes(data)
            migration.migrate(root)
            self.assertEqual((root/'JASWINGMAN PRO.settings').read_bytes(),data);self.assertEqual(old.read_bytes(),data)
    def test_existing_new_settings_not_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'Wingman Pro.settings').write_bytes(b'old');new=root/'JASWINGMAN PRO.settings';new.write_bytes(b'new')
            migration.migrate(root);self.assertEqual(new.read_bytes(),b'new')
    def test_initial_name_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'Wingman.settings').write_bytes(b'first');migration.migrate(root)
            self.assertEqual((root/'JASWINGMAN PRO.settings').read_bytes(),b'first')
    def test_missing_legacy_does_not_create_empty_state(self):
        with tempfile.TemporaryDirectory() as directory:
            migration.migrate(directory);self.assertFalse((Path(directory)/'JASWINGMAN PRO.settings').exists())
if __name__=='__main__':unittest.main()

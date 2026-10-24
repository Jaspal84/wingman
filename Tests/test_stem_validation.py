import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
import numpy as np
import soundfile as sf
spec=importlib.util.spec_from_file_location('validate_stems',Path(__file__).resolve().parents[1]/'Scripts/validate_stems.py')
validator=importlib.util.module_from_spec(spec);spec.loader.exec_module(validator)
class StemValidationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        t=np.arange(4096)/44100;mixture=np.zeros((4096,2))
        for i,name in enumerate(validator.SOURCES):
            audio=np.column_stack([0.04*np.sin(2*np.pi*(170+i*120)*t),0.035*np.sin(2*np.pi*(180+i*120)*t)])
            sf.write(self.root/(name+'.wav'),audio,44100,subtype='PCM_24');mixture+=audio
        self.input=self.root/'input.wav';sf.write(self.input,mixture,44100,subtype='PCM_24')
    def test_valid_set(self):
        result=validator.validate(self.input,self.root)
        self.assertTrue(result['structural_checks_passed']);self.assertGreater(result['mixture_reconstruction_snr_db'],95)
        self.assertEqual(len(result['pairwise_correlations']),6)
    def test_missing_stem(self):
        (self.root/'bass.wav').unlink()
        with self.assertRaises((OSError,RuntimeError)):validator.validate(self.input,self.root)
    def test_misaligned_stem(self):
        sf.write(self.root/'bass.wav',np.ones((4095,2))*0.1,44100,subtype='PCM_24')
        with self.assertRaisesRegex(ValueError,'synchronized'):validator.validate(self.input,self.root)
    def test_wrong_encoding(self):
        sf.write(self.root/'bass.wav',np.ones((4096,2))*0.1,44100,subtype='PCM_16')
        with self.assertRaisesRegex(ValueError,'PCM_24'):validator.validate(self.input,self.root)
    def test_duplicate_stems(self):
        shutil.copyfile(self.root/'drums.wav',self.root/'bass.wav')
        with self.assertRaisesRegex(ValueError,'Duplicate'):validator.validate(self.input,self.root)
    def test_silent_stem(self):
        sf.write(self.root/'bass.wav',np.zeros((4096,2)),44100,subtype='PCM_24')
        with self.assertRaisesRegex(ValueError,'nonzero'):validator.validate(self.input,self.root)
    def test_clipping_reported(self):
        data,_=sf.read(self.root/'bass.wav');data[0,0]=1
        sf.write(self.root/'bass.wav',data,44100,subtype='PCM_24')
        result=validator.validate(self.input,self.root)
        self.assertGreater(result['stems']['bass']['clipped_sample_fraction'],0)
        self.assertTrue(any('saturated' in w for w in result['warnings']))
if __name__=='__main__':unittest.main()

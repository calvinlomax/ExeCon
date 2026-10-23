#!/usr/bin/env python3
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('fetch',root/'scripts/fetch-test-binaries.py')
fetch=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)
class IntegrityTest(unittest.TestCase):
    def test_reject_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            file=Path(directory)/'archive'
            file.write_bytes(b'expected artifact')
            digest=hashlib.sha256(file.read_bytes()).hexdigest()
            fetch.verified(file,digest)
            file.write_bytes(b'tampered artifact')
            with self.assertRaisesRegex(ValueError,'SHA-256 mismatch'): fetch.verified(file,digest)
if __name__=='__main__': unittest.main()

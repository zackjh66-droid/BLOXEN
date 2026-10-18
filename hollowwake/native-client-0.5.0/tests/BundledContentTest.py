"""Owned byte fixtures only: actual staging/hash checks, no Fortnite execution."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('bundle', Path(__file__).parents[1] / 'private-package/bundle_content.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)

class BundledContentTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.payload = b'Original Hollowwake bundled-data fixture'
        self.path = '6.21/FortniteGame/Content/Paks/owned.pak'
        self.file = {'path': self.path, 'bytes': len(self.payload), 'sha256': hashlib.sha256(self.payload).hexdigest()}
        self.plan = {'schema': 1, 'build': b.BUILD, 'files': [self.file], 'archive': {'bytes': b.SIZE, 'sha256': b.PIN}}
        self.raw = json.dumps(self.plan).encode()
        self.buffer = io.BytesIO()
        with zipfile.ZipFile(self.buffer, 'w') as z:
            z.writestr(self.path, self.payload)
    def tearDown(self):
        self.temp.cleanup()
    def test_stage_real_bytes_and_marker(self):
        target = self.root / 'staged'
        with zipfile.ZipFile(self.buffer) as z:
            b.inspect(z, b.records(self.plan))
            b.stage(z, self.plan['files'], target, self.raw)
        self.assertEqual((target / self.path).read_bytes(), self.payload)
        marker = json.loads((target / 'bundled-content.json').read_text())
        self.assertEqual(marker['contentPlanSha256'], hashlib.sha256(self.raw).hexdigest())
    def test_corrupt_hash_never_ready(self):
        self.file['sha256'] = '0' * 64
        target = self.root / 'bad'
        with zipfile.ZipFile(self.buffer) as z, self.assertRaises(ValueError):
            b.stage(z, self.plan['files'], target, self.raw)
        self.assertFalse((target / 'bundled-content.json').exists())
    def test_low_disk_does_not_start(self):
        target = self.root / 'low'
        with zipfile.ZipFile(self.buffer) as z, patch.object(b.shutil, 'disk_usage', return_value=type('Space', (), {'free': 1})()), self.assertRaises(ValueError):
            b.stage(z, self.plan['files'], target, self.raw)
        self.assertFalse(target.exists())
    def test_preserve_existing_destination(self):
        with zipfile.ZipFile(self.buffer) as z, self.assertRaises(ValueError):
            b.stage(z, self.plan['files'], self.root, self.raw)
    def test_duplicate_case(self):
        self.plan['files'].append(dict(self.file, path=self.path.upper()))
        with self.assertRaises(ValueError):
            b.records(self.plan)
    def test_unsafe_paths_and_code(self):
        for name in ['../escape.pak', '6.21/FortniteGame/Content/../escape.pak', '6.21/FortniteGame/Content/x.dll', '6.21/FortniteGame/Content/CON.txt']:
            self.assertFalse(b.safe(name))
    def test_archive_links_rejected(self):
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, 'w') as z:
            entry = zipfile.ZipInfo('6.21/link')
            entry.external_attr = 0o120777 << 16
            z.writestr(entry, '../outside')
        with zipfile.ZipFile(buf) as z, self.assertRaises(ValueError):
            b.inspect(z, [])
    def test_prepared_copy_rejects_unlisted_then_copies_verified_bytes(self):
        prepared = self.root / 'prepared'
        app = self.root / 'app'
        app.mkdir()
        manifest = self.root / 'plan.json'
        manifest.write_bytes(self.raw)
        with zipfile.ZipFile(self.buffer) as z:
            b.stage(z, self.plan['files'], prepared, self.raw)
        extra = prepared / 'unselected.txt'
        extra.write_text('must not enter package')
        argv = ['bundle', '--manifest', str(manifest), '--prepared', str(prepared), '--copy-to', str(app)]
        with patch.object(sys, 'argv', argv), self.assertRaises(ValueError):
            b.main()
        self.assertEqual(list(app.iterdir()), [])
        extra.unlink()
        with patch.object(sys, 'argv', argv):
            b.main()
        self.assertEqual((app / self.path).read_bytes(), self.payload)
        self.assertTrue((app / 'bundled-content.json').is_file())

if __name__ == '__main__':
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))

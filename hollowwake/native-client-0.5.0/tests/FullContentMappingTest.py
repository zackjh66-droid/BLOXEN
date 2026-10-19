"""Small original file fixtures exercise real mapping/audit paths, not game code."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

private = Path(__file__).parents[1] / 'private-package'
sys.path.insert(0, str(private))
import full_mapping as mapping

class FullContentMappingTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.layout = self.root / 'layout'
        self.content = self.root / 'content'
        self.layout.mkdir()
        self.content.mkdir()
        self.name = '6.21/FortniteGame/Content/owned.txt'
        payload = b'Original Hollowwake content fixture'
        self.record = {'path': self.name, 'bytes': len(payload), 'sha256': hashlib.sha256(payload).hexdigest()}
        self.manifest = self.root / 'plan.json'
        self.manifest.write_text(json.dumps({'schema': 1, 'build': '6.21-CL-4526925', 'files': [self.record]}))
        marker = {'schema': 1, 'mode': 'installed-package-only', 'build': '6.21-CL-4526925', 'files': 1,
                  'expandedBytes': len(payload), 'contentPlanSha256': hashlib.sha256(self.manifest.read_bytes()).hexdigest()}
        (self.content / self.name).parent.mkdir(parents=True)
        (self.content / self.name).write_bytes(payload)
        (self.content / 'bundled-content.json').write_text(json.dumps(marker))
        (self.layout / 'AppxManifest.xml').write_text('<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10" IgnorableNamespaces="uap"><Identity Name="Hollowwake.NativeClientExperiment" ProcessorArchitecture="x64" Version="0.5.1.0"/></Package>')
        data = b'Original owned native-image metadata fixture; not executable'
        (self.layout / 'owned.dll').write_bytes(data)
        self.native = {'build': '6.21-CL-4526925', 'files': [{'packageRelativePath': 'owned.dll', 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}], 'proxyDlls': []}
        (self.layout / 'native-package-manifest.json').write_text(json.dumps(self.native))
        self.prepared = self.root / 'native.json'
        self.prepared.write_text(json.dumps(self.native))
    def tearDown(self):
        self.temp.cleanup()
    def test_mapping_preserves_namespaces_and_points_to_content_without_copy(self):
        path = self.root / 'files.map'
        mapping.mapping(self.layout, self.content, self.manifest, path)
        xml = (self.layout / 'AppxManifest.xml').read_text()
        self.assertIn('xmlns:uap=', xml)
        self.assertIn('Version="0.5.2.0"', xml)
        self.assertIn(str((self.content / self.name).resolve()), path.read_text())
        self.assertFalse((self.layout / self.name).exists())
    def test_hash_audit_reads_real_bytes_and_rejects_tampering(self):
        archive = self.root / 'owned.msix'
        def package(payload):
            with zipfile.ZipFile(archive, 'w') as z:
                z.write(self.layout / 'owned.dll', 'owned.dll')
                z.write(self.content / 'bundled-content.json', 'bundled-content.json')
                z.writestr(self.name, payload)
                z.writestr('AppxSignature.p7x', b'Fixture presence only: NOT a signature')
                z.writestr('AppxBlockMap.xml', b'Fixture presence only')
        package((self.content / self.name).read_bytes())
        mapping.audit(archive, self.content, self.manifest, self.prepared, self.root / 'audit.json')
        result = json.loads((self.root / 'audit.json').read_text())
        self.assertFalse(result['fortniteExecuted'])
        self.assertIn('not performed', result['signatureTrustVerification'])
        package(b'Tampered bytes')
        with self.assertRaises(ValueError):
            mapping.audit(archive, self.content, self.manifest, self.prepared, self.root / 'bad-audit.json')

if __name__ == '__main__':
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))

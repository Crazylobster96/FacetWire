"""Pure-Python per-renderer installation archive checks."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts' / 'package-renderer.py'
spec = importlib.util.spec_from_file_location('facetwire_package_renderer', SCRIPT)
package_renderer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package_renderer)


class PackageRendererTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.source = self.root / 'source.json'
        self.library = self.root / 'renderer.dll'
        self.license = self.root / 'LICENSE'
        self.output = self.root / 'renderer.zip'
        self.manifest = dict(format='facetwire.plugin-manifest', formatVersion='0.1',
            plugin=dict(id='org.facetwire.test.renderer',version='0.1.0',name='test',vendor='test',licenseSpdx='MPL-2.0'),
            abi=dict(major=1,minimumMinor=0,maximumMinor=0),
            capabilities=[dict(id='facetwire.renderer.test',kind='facetwire.capability.renderer',
                interfaces=[dict(id='facetwire.renderer.test.v1',version=1)])],
            artifacts=[dict(target='any',profile='static',registration='test_query')],
            permissions=[],dependencies=[],extensions={})
        self.save()
        self.library.write_bytes(b'MZ synthetic renderer')
        self.license.write_bytes(b'Synthetic MPL-2.0 notice')

    def save(self):
        self.source.write_text(json.dumps(self.manifest),encoding='utf-8')

    def run_package(self, **changes):
        values=dict(manifest_path=self.source,library_path=self.library,license_path=self.license,
            target='windows-x86_64',output_path=self.output)
        values.update(changes)
        return package_renderer.package(**values)

    def test_exact_independent_manifest_and_bytes(self):
        produced=self.run_package()
        self.assertEqual(self.manifest['plugin'],produced['plugin'])
        self.assertEqual('native-dynamic',produced['artifacts'][0]['profile'])
        self.assertEqual(hashlib.sha256(self.library.read_bytes()).hexdigest(),produced['artifacts'][0]['sha256'])
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(['LICENSE','facetwire.plugin.json','lib/renderer.dll'],archive.namelist())
            self.assertEqual(self.library.read_bytes(),archive.read('lib/renderer.dll'))
            self.assertEqual(produced,json.loads(archive.read('facetwire.plugin.json')))
            self.assertTrue(all(member.date_time==(1980,1,1,0,0,0) for member in archive.infolist()))
        with self.assertRaises(package_renderer.PackageError):self.run_package()

    def test_invalid_input_and_target_do_not_create_output(self):
        cases=(dict(manifest_path=Path('relative')),
               dict(library_path=self.root/'missing.dll'),
               dict(license_path=self.root/'missing-license'),
               dict(output_path=Path('relative.zip')),
               dict(target='ios-arm64'),dict(target='windows-x86'),
               dict(target='linux-x86_64'),dict(library_path=self.source))
        for change in cases:
            with self.subTest(change=change),self.assertRaises(package_renderer.PackageError):
                self.run_package(**change)
            self.assertFalse(self.output.exists())
        self.library.write_bytes(b'')
        with self.assertRaises(package_renderer.PackageError):self.run_package()

    def test_malformed_or_unapproved_manifest_rejected(self):
        self.source.write_bytes(b'{')
        with self.assertRaises(package_renderer.PackageError):self.run_package()
        self.save()
        for change in (dict(permissions=['network']),dict(dependencies=[{}]),dict(artifacts=[]),
                       dict(artifacts=[None]),dict(format='other'),dict(capabilities='bad')):
            changed=self.manifest.copy();changed.update(change)
            self.source.write_text(json.dumps(changed),encoding='utf-8')
            with self.subTest(change=change),self.assertRaises(package_renderer.PackageError):
                self.run_package()
            self.assertFalse(self.output.exists())


if __name__=='__main__':
    unittest.main()

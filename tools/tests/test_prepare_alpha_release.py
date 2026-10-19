"""Offline synthetic fixtures; no compiler, native program, Git, or network."""
import copy
import gzip
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("alpha_release", Path(__file__).resolve().parents[1] / "prepare_alpha_release.py")
A = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(A)


class ReleaseArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        for name in A.REQUIRED_FILES:
            self.write(name, b"// synthetic fixture\n")
        self.write("VERSION", (A.VERSION + "\n").encode())
        self.write("tests/CMakeLists.txt", b"# fixture\n")
        self.write("examples/CMakeLists.txt", b"# fixture\n")
        trees = {}
        self.tree_members = {}
        for tree in A.DEPENDENCY_TREES:
            component = "typelayout" if "typelayout" in tree else tree.split("/")[3]
            relative = "boost/" + component + "/fixture.hpp"
            data = ("// pinned " + component + "\n").encode()
            self.write(tree + "/" + relative, data)
            self.tree_members[tree] = {relative: data}
            trees[tree] = A.sha((relative + " " + A.sha(data) + "\n").encode())
        licenses = {}
        for name in A.DEPENDENCY_LICENSES:
            self.write(name, b"synthetic license\n")
            licenses[name] = A.sha(b"synthetic license\n")
        lock = "set(XOFFSET_DEPENDENCY_TREES\n" + "".join('  "' + name + "|" + digest + '"\n' for name, digest in trees.items()) + ")\n"
        lock += "set(XOFFSET_DEPENDENCY_LICENSES\n" + "".join('  "' + name + "|" + digest + '"\n' for name, digest in licenses.items()) + ")\n"
        lock += "".join("set(XOFFSET_" + name.upper() + "_REVISION " + revision + ")\n" for name, revision in A.EXPECTED_REVISIONS.items())
        self.write("cmake/XOffsetDependencyLock.cmake", lock.encode())

    def write(self, name, data):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def create(self, name="release"):
        out = self.root / name
        record = A.create(self.source, out)
        return out, Path(record["archive"])

    def payload(self):
        _, payload, lock = A.collect_source(self.source)
        return payload, A.json_bytes(A.source_manifest(payload, lock))

    def archive_from_members(self, rows, filename="hostile.tar.gz"):
        output = io.BytesIO()
        with tarfile.open(fileobj=output, mode="w", format=tarfile.GNU_FORMAT) as bundle:
            for name, data, kind in rows:
                member = tarfile.TarInfo(name)
                member.uid = member.gid = member.mtime = 0
                member.mode = A.normalized_mode(name.split("/", 1)[-1])
                member.type = kind
                member.size = len(data) if kind == tarfile.REGTYPE else 0
                if kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
                    member.linkname = "../../escape"
                bundle.addfile(member, io.BytesIO(data))
        archive = self.root / filename
        archive.write_bytes(A.canonical_gzip(output.getvalue()))
        return archive

    def make_prefix(self, component):
        prefix = self.root / ("prefix-" + component)
        payload = {name: b"// synthetic fixture\n" for name in A.install_required(component)}
        payload["share/XOffsetDatastructure/VERSION"] = (A.VERSION + "\n").encode()
        config = {
            "XOffsetDatastructure_VERSION": '"' + A.VERSION + '"',
            "XOffsetDatastructure_VERSION_NUMERIC": '"0.1.0"',
            "XOffsetDatastructure_VERSION_PRERELEASE": '"alpha.1"',
            "XOffsetDatastructure_IS_PRERELEASE": "TRUE",
            "XOffsetDatastructure_VERSION_COMPATIBILITY": '"exact-full-version-only"',
            "XOffsetDatastructure_full_FOUND": "ON" if component == "full" else "OFF",
            "XOffsetDatastructure_storage_FOUND": "TRUE",
        }
        payload["lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfig.cmake"] = "".join("set(" + key + " " + value + ")\n" for key, value in config.items()).encode()
        payload["lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfigVersion.cmake"] = ('set(PACKAGE_VERSION "' + A.VERSION + '")\n').encode()
        targets = ["XOffset::storage"] + (["XOffset::xoffset"] if component == "full" else [])
        payload["lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake"] = "".join("add_library(" + target + " INTERFACE IMPORTED)\n" for target in targets).encode()
        if component == "full":
            lines = ["Boost " + A.EXPECTED_REVISIONS["boost"], "TypeLayout " + A.EXPECTED_REVISIONS["typelayout"], ""]
            for tree, members in self.tree_members.items():
                records = "".join(name + " " + A.sha(data) + "\n" for name, data in sorted(members.items()))
                lines.append("[" + tree + "] " + A.sha(records.encode()))
                for name, data in sorted(members.items()):
                    lines.append(name + " " + A.sha(data))
                    payload["include/xoffset-deps/" + name] = data
                lines.append("")
            for name in A.DEPENDENCY_LICENSES:
                data = (self.source / name).read_bytes()
                payload["share/XOffsetDatastructure/licenses/" + name.removeprefix("external/")] = data
                lines.append(name + " " + A.sha(data))
            payload["share/XOffsetDatastructure/xoffset-third-party-manifest.txt"] = ("\n".join(lines) + "\n").encode()
        for name, data in payload.items():
            path = prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return prefix

    def test_deterministic_working_files_and_normalized_metadata(self):
        self.write("xoffsetdatastructure.hpp", b"// current uncommitted working bytes\n")
        self.write(".git/HEAD", b"obsolete-unrelated-commit")
        self.write("build/private-binary", b"not distributed")
        self.write("docs/validation/history.tar.gz", b"not distributed")
        first, archive = self.create("first")
        for path in self.source.rglob("*"):
            if path.is_file():
                os.utime(path, (123456789, 123456789))
        second, other = self.create("second")
        self.assertEqual(archive.read_bytes(), other.read_bytes())
        self.assertEqual((first / "SHA256SUMS").read_bytes(), (second / "SHA256SUMS").read_bytes())
        _, payload, manifest_data = A.read_archive(archive)
        self.assertEqual(payload["xoffsetdatastructure.hpp"], b"// current uncommitted working bytes\n")
        self.assertFalse(any(name.startswith((".git/", "build/", "docs/validation/")) for name in payload))
        self.assertEqual(A.parse_json(manifest_data)["version"], A.VERSION)

    def test_real_extract_without_original_source_or_git_and_repackage(self):
        out, archive = self.create()
        moved = self.root / "original-unavailable"
        self.source.rename(moved)
        result = A.extract(archive, self.root / "extracted", out / (A.PACKAGE_NAME + ".manifest.json"), out / "SHA256SUMS")
        extracted = Path(result["source_dir"])
        self.assertTrue((extracted / "external/typelayout/include/boost/typelayout/fixture.hpp").is_file())
        self.assertFalse((extracted / ".git").exists())
        reproduced = A.create(extracted, self.root / "reproduced")
        self.assertEqual(Path(reproduced["archive"]).read_bytes(), archive.read_bytes())

    def test_extract_pins_identity_and_payload_to_one_archive_read(self):
        out_a, archive_a = self.create("release-a")
        readme_a = (self.source / "README.md").read_bytes()
        self.write("README.md", b"different but independently valid source B\n")
        _, archive_b = self.create("release-b")
        loaded_a, loaded_b = A.read_archive(archive_a), A.read_archive(archive_b)
        self.assertNotEqual(A.sha(loaded_a[0]), A.sha(loaded_b[0]))
        # An archive can be replaced between reads. Old extract audited A,
        # reread B, and returned A's hash while extracting B's content.
        with mock.patch.object(A, "read_archive", side_effect=[loaded_a, loaded_b]) as read:
            result = A.extract(archive_a, self.root / "single-read",
                               out_a / (A.PACKAGE_NAME + ".manifest.json"), out_a / "SHA256SUMS")
        self.assertEqual(read.call_count, 1)
        self.assertEqual(result["archive_sha256"], A.sha(loaded_a[0]))
        self.assertEqual((Path(result["source_dir"]) / "README.md").read_bytes(), readme_a)

    def test_overwrite_and_output_inside_source_are_rejected(self):
        out, archive = self.create()
        original = archive.read_bytes()
        with self.assertRaisesRegex(ValueError, "already exists"):
            A.create(self.source, out)
        self.assertEqual(archive.read_bytes(), original)
        with self.assertRaisesRegex(ValueError, "copied source tree"):
            A.create(self.source, self.source / "tests/new-output")
        with self.assertRaisesRegex(ValueError, "already exists"):
            A.extract(archive, self.source)

    def test_version_dependency_license_and_cpp_closure_fail_closed(self):
        for name, data, expected in (
                ("VERSION", b"0.1.0\n", "VERSION"),
                (A.DEPENDENCY_TREES[0] + "/boost/typelayout/fixture.hpp", b"changed", "content mismatch"),
                (A.DEPENDENCY_LICENSES[0], b"changed", "license mismatch"),
                ("tests/missing.cpp", b'#include "absent.hpp"\n', "include missing")):
            path = self.source / name
            original = path.read_bytes() if path.exists() else None
            self.write(name, data)
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, expected):
                A.create(self.source, self.root / "never-created")
            self.assertFalse((self.root / "never-created").exists())
            if original is None:
                path.unlink()
            else:
                path.write_bytes(original)

    def test_source_mutation_is_not_published(self):
        first = A.collect_source(self.source)
        changed = copy.deepcopy(first[1])
        changed["README.md"] = b"changed"
        with mock.patch.object(A, "collect_source", side_effect=[first, (first[0], changed, first[2])]):
            with self.assertRaisesRegex(ValueError, "changed while packaging"):
                A.create(self.source, self.root / "not-created")
        self.assertFalse((self.root / "not-created").exists())

    def test_tampered_member_and_manifest_must_be_rejected_before_extract(self):
        payload, manifest_data = self.payload()
        payload["README.md"] = b"tampered"
        bad = self.root / "changed.tar.gz"
        bad.write_bytes(A.canonical_gzip(A.canonical_tar(payload, manifest_data)))
        with self.assertRaisesRegex(ValueError, "differ from manifest"):
            A.extract(bad, self.root / "not-extracted")
        self.assertFalse((self.root / "not-extracted").exists())
        duplicate = manifest_data.replace(b'"version":', b'"version": "ignored", "version":', 1)
        bad.write_bytes(A.canonical_gzip(A.canonical_tar(payload, duplicate)))
        with self.assertRaisesRegex(ValueError, "duplicate JSON"):
            A.audit(bad)

    def test_external_manifest_and_checksum_tampering(self):
        out, archive = self.create()
        manifest = out / (A.PACKAGE_NAME + ".manifest.json")
        manifest.write_bytes(manifest.read_bytes() + b" ")
        with self.assertRaisesRegex(ValueError, "external manifest"):
            A.audit(archive, manifest)
        (out / "SHA256SUMS").write_text("wrong")
        with self.assertRaisesRegex(ValueError, "SHA256SUMS"):
            A.audit(archive, checksums=out / "SHA256SUMS")

    def test_duplicate_path_traversal_links_devices_and_case_aliases(self):
        invalid_names = ["../escape", "/escape", A.PACKAGE_NAME + "/../escape",
                         A.PACKAGE_NAME + "/a\\b", A.PACKAGE_NAME + "/a:b",
                         A.PACKAGE_NAME + "/CON", A.PACKAGE_NAME + "/bad.",
                         A.PACKAGE_NAME + "/a//b"]
        for name in invalid_names:
            with self.subTest(name=name):
                bad = self.archive_from_members([(name, b"x", tarfile.REGTYPE)])
                with self.assertRaises(ValueError):
                    A.extract(bad, self.root / "escape-output")
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.DIRTYPE, tarfile.CHRTYPE, tarfile.FIFOTYPE):
            with self.subTest(kind=kind):
                bad = self.archive_from_members([(A.PACKAGE_NAME + "/README.md", b"", kind)])
                with self.assertRaisesRegex(ValueError, "non-regular"):
                    A.audit(bad)
        for pair in (("README.md", "README.md"), ("README.md", "readme.md")):
            bad = self.archive_from_members([(A.PACKAGE_NAME + "/" + name, b"x", tarfile.REGTYPE) for name in pair])
            with self.assertRaisesRegex(ValueError, "duplicate"):
                A.audit(bad)

    def test_noncanonical_order_and_trailing_gzip_data(self):
        payload, manifest = self.payload()
        rows = [(A.PACKAGE_NAME + "/" + name, data, tarfile.REGTYPE)
                for name, data in reversed(sorted({**payload, A.MANIFEST_NAME: manifest}.items()))]
        bad = self.archive_from_members(rows)
        with self.assertRaisesRegex(ValueError, "noncanonical"):
            A.audit(bad)
        _, valid = self.create()
        for extra in (b"trailing", gzip.compress(b"")):
            bad.write_bytes(valid.read_bytes() + extra)
            with self.assertRaisesRegex(ValueError, "trailing gzip"):
                A.audit(bad)

    def test_ancestor_case_alias_and_file_directory_conflicts_are_rejected(self):
        for names, expected in ((["tests/A/one.hpp", "tests/a/two.hpp"], "case-colliding"),
                                (["tests/parent", "tests/parent/child.hpp"], "directory prefix")):
            payload, _ = self.payload()
            payload.update({name: b"fixture\n" for name in names})
            with self.subTest(names=names):
                with self.assertRaisesRegex(ValueError, expected):
                    A.validate_payload(payload)
                # A hostile archive can carry an otherwise self-consistent
                # manifest. Reject its implicit directory aliases before write.
                manifest = A.json_bytes(A.source_manifest(payload, A.dependency_lock(payload)))
                bad = self.root / "directory-alias.tar.gz"
                bad.write_bytes(A.canonical_gzip(A.canonical_tar(payload, manifest)))
                with self.assertRaisesRegex(ValueError, expected):
                    A.extract(bad, self.root / "not-extracted-alias")
                self.assertFalse((self.root / "not-extracted-alias").exists())
        prefix = self.make_prefix("storage")
        installed = A.collect_install_prefix(prefix)
        installed.update({"include/A/one.hpp": b"one", "include/a/two.hpp": b"two"})
        with self.assertRaisesRegex(ValueError, "case-colliding"):
            A.validate_install_payload(installed, "storage")

    def test_source_and_output_symbolic_links_are_rejected(self):
        target = self.root / "outside"
        target.write_text("private")
        link = self.source / "tests/linked.hpp"
        try:
            link.symlink_to(target)
        except OSError as exc:
            self.skipTest("symlink creation unavailable: " + str(exc))
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            A.create(self.source, self.root / "not-created")
        link.unlink()
        parent = self.root / "output-link"
        parent.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            A.create(self.source, parent / "child")

    def test_full_and_storage_prefix_packages_extract_and_audit(self):
        for component in ("full", "storage"):
            with self.subTest(component=component):
                prefix = self.make_prefix(component)
                result = A.create_install_package(prefix, component, self.root / ("package-" + component))
                audit = A.audit(Path(result["archive"]))
                self.assertEqual(audit["component"], component)
                extracted = A.extract(Path(result["archive"]), self.root / ("extracted-" + component))
                root = Path(extracted["source_dir"])
                self.assertTrue((root / "lib/cmake/XOffsetDatastructure/XOffsetTargets.cmake").is_file())
                self.assertEqual((root / "share/XOffsetDatastructure/VERSION").read_text().strip(), A.VERSION)
                self.assertEqual((root / "include/xoffsetdatastructure.hpp").exists(), component == "full")

    def test_crlf_prefix_metadata_parses_without_changing_bound_bytes(self):
        prefix = self.make_prefix("storage")
        for path in (prefix / "lib/cmake/XOffsetDatastructure").glob("*.cmake"):
            path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        result = A.create_install_package(prefix, "storage", self.root / "crlf-package")
        _, payload, _ = A.read_archive(Path(result["archive"]))
        key = "lib/cmake/XOffsetDatastructure/XOffsetDatastructureConfig.cmake"
        self.assertEqual(payload[key], (prefix / key).read_bytes())
        self.assertIn(b"\r\n", payload[key])
        with (prefix / key).open("ab") as stream:
            stream.write(('set(XOffsetDatastructure_VERSION "' + A.VERSION + '")\r\n').encode())
        with self.assertRaisesRegex(ValueError, "component/version"):
            A.create_install_package(prefix, "storage", self.root / "duplicate-declaration")

    def test_prefix_wrong_component_missing_metadata_extra_binary_and_changed_header(self):
        full = self.make_prefix("full")
        with self.assertRaisesRegex(ValueError, "component/version"):
            A.create_install_package(full, "storage", self.root / "wrong-component")
        changes = [("share/XOffsetDatastructure/CHANGELOG.md", None, "missing"),
                   ("bin/unwanted.dll", b"binary", "unexpected"),
                   ("share/XOffsetDatastructure/VERSION", b"0.1.0", "VERSION differs"),
                   ("include/xoffset-deps/boost/typelayout/fixture.hpp", b"changed", "bytes differ")]
        for name, data, expected in changes:
            path = full / name
            old = path.read_bytes() if path.exists() else None
            if data is None:
                path.unlink()
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, expected):
                A.create_install_package(full, "full", self.root / "bad-prefix")
            if old is None:
                path.unlink()
            else:
                path.write_bytes(old)

    def test_prefix_symbolic_link_is_rejected(self):
        prefix = self.make_prefix("storage")
        target = prefix / "share/XOffsetDatastructure/CHANGELOG.md"
        target.unlink()
        try:
            target.symlink_to(self.source / "CHANGELOG.md")
        except OSError as exc:
            self.skipTest("symlink creation unavailable: " + str(exc))
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            A.create_install_package(prefix, "storage", self.root / "bad-prefix")


if __name__ == "__main__":
    unittest.main()

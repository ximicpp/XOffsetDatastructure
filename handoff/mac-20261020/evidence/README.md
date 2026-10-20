# Q32 / F64 portable evidence

This is the complete regular-file content of the eight roots declared in
`BUNDLE.json`, stored without expanding the original hard-link aliases. It is
transported through ordinary Git blobs: four independent ZIP volumes, each
below 90 MiB; no Git LFS or separate manual copy is required.

The original Q32 and F64 run roots, their original frozen roots, both original
offline-audit queues and both original offline-audit result files are included.
The two run roots include all original native stdout/stderr/receipts, command
artifacts, original status/analysis/samples/manifest/resource-ledger/runtime,
verified body pool, frozen inputs, fixtures, protocols, archived tools and Linux
binaries. Construction did not modify these sources or execute any archived
tool, benchmark, compiler, queue, host action or authorization.

Actual scope: **98,418 logical files / 485,631,080,466 logical bytes**. Only
56,658 unique source inodes / 781,767,106 bytes needed to be read. SHA-256 content
deduplication leaves **28,022 CAS blobs / 282,577,279 bytes**, compressed into
four ZIP files totaling **126,548,323 bytes**. Do not expand the logical aliases
into a directory tree; that would recreate roughly 485.6 GB of apparent files.

On a fresh macOS checkout, from the repository root:

```sh
python3 -I -B tools/verify_c14_evidence_bundle_20261020.py
```

The checker needs only Python 3.9+ and its standard library. It checks adjacent
volume and metadata SHA-256 identities, ZIP duplicate/path/regular-file safety,
each complete CAS blob once, every logical reference, all scope totals and all
mapping relationships. It never extracts the original tree, imports project
modules, accesses a network, builds or executes source/native code. Passing is
**transfer integrity**, not a macOS build, test or performance result.

`BUNDLE.json` is the small entry manifest. `logical-paths.jsonl.gz` contains one
JSON line for every original regular file, with `scope`, relative `path`, mode,
byte size and content `sha256`. The scope's `original_root` plus its relative
path identifies the original directory member; a file scope identifies its
`original_root` directly. `catalog.json.gz` maps that SHA-256 to its unique ZIP
`volume` and `member` (`cas/sha256/<sha256>`). These mappings retain the complete
original path identity without following any old absolute path. Original modes
are recorded as metadata; CAS members are inert regular files.

`VERIFICATION_RESULT.json` records the one construction-time integrity check.
`construction/` retains the actual original shared W1 validation queue's seven
small files and a receipt: run `be1d27b37083441bada83c1e58dfaea9`, 31.342 seconds,
exit 0, clean process group. The packaging job used the unchanged original
shared lease (600-second limit, wait 0); zero new native/benchmark runs and zero
original offline-audit replays occurred. The build scripts under
`tools/c14_handoff_20261020/` are old-host provenance, not a macOS entry point.

Excluded from this bundle are compiler/SDK/installed runtime trees outside the
declared roots, external historical raw archives, and individual R1/M2
screening/functional raw build trees outside the roots. Their retained repository
summaries remain separate; this bundle does not claim to contain or re-audit
those external inputs. Old Windows/WSL absolute paths, process permissions,
power settings and queue records are historical evidence and confer no action
authority on the new machine. Linux executables are retained as evidence and
must not be executed as macOS programs.

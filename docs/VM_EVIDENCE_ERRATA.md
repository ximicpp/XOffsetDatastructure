# VM evidence errata

2026-10-15: the archived
`docs/perf/2026-10-15-vm-followup/accepted_f/followup/metadata.json` has stale
`isolation` prose naming D/E and an E binary. Its source fingerprints and the
parent evidence README identify the actual default comparison as D/F, and the
opt-in comparisons as two modes of the same F binary. The F main header hash is
`9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`.

The archived metadata, raw measurements and their hashes are unchanged.
The maintained `tools/run_vm_followup.py` now derives explicit variant source
identities from both header hashes and describes before/after endpoints without
hard-coded candidate letters. This erratum changes interpretation of the stale
label only, not measured results or adoption decisions.

## Summary

Briefly describe what this PR changes and why.

## Checklist

- [ ] Code and documentation are in English (`python3 tools/check_no_cjk.py` reports 0 hits).
- [ ] Relevant unit tests / CTest targets pass on the affected C++ standard(s) (`20`, `23`, and/or `26`).
- [ ] Frozen benchmark files were not modified unintentionally (`python3 bench/check_manifest.py` passes),
      or any deliberate benchmark update is explicitly noted.
- [ ] Wire-format, schema, or untrusted-input changes include negative/fuzz coverage where applicable
      and note any `docs/V2_THREAT_MODEL.md` impact.
- [ ] Performance-sensitive library changes include local validation / Ir gate notes (`docs/PERF_GATE.md`).

## Verification

List the exact build commands, compilers/platforms, and test results you ran.

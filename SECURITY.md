# Security Policy

## Supported Versions

Historical pre-public releases (`3.x` and `4.x` internal V2 iterations) did not promise cross-version
wire or API compatibility. Starting with **2.0.0** (the first public stable release of XOffset V2),
incompatible changes come only in a new major version ([docs/RELEASING.md](docs/RELEASING.md)
section 1.1), so security fixes ship in patch releases of the current major version.

| Branch / Release | Supported |
| --- | --- |
| `main` (current development) | Yes |
| Latest tagged release | Yes |
| Older tags | No |

## Reporting a Vulnerability

Please do **not** open a public GitHub issue for suspected security vulnerabilities (such as memory
corruption, out-of-bounds reads/writes, infinite loops, or validation bypasses on untrusted wires).

Instead, report vulnerabilities privately through GitHub's **Report a vulnerability** button under the
repository's **Security -> Advisories** tab.

Please include as much of the following as possible:

- Affected commit SHA or `XOFFSET_VERSION`
- Build configuration (compiler, C++ standard `20`/`23`/`26`, OS/architecture, `-DXOFFSET_HARDENED`,
  `-DXOFFSET_USE_PMR`, exception mode)
- Which trust tier / entry point was used (`XReceiver<XLink::from_client>`, `XCheck::bounds` /
  `XCheck::full`, `load_verified`, `try_load_verified`, `reload_verified`, `try_reload_verified`, etc.)
- Minimal reproducer or crashing wire bytes (for example, a libFuzzer artifact or standalone test case)

We aim to acknowledge reports within 5 business days and coordinate a fix and regression test
(under `tests/fuzz/regressions/` where applicable) before public disclosure.

## Security Boundaries and Scope

See [docs/V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md) for the full threat model and security
contract. Key boundaries to keep in mind when evaluating reports:

- **Untrusted / client-facing boundary (in scope):** `XReceiver<XLink::from_client>`,
  `TypedXBuffer::load_verified`, `try_load_verified`, `reload_verified`, `try_reload_verified`,
  delta replay on verified links, and `read_schema_ack` / `make_schema_hello` must fail closed on
  arbitrary malformed, truncated, or adversarial input bytes without undefined behavior, out-of-bounds
  memory access, or unbounded allocation beyond configured caps.
- **Trusted internal links (out of scope for adversarial byte injection):** `XLink::server_peer` and
  `XLink::from_server` (defaulting to `XCheck::admit`) assume an authenticated, integrity-protected
  internal link (for example, server-to-server over a trusted network or authenticated storage) and
  intentionally skip deep structural re-validation.
- **Optional hardening (`-DXOFFSET_HARDENED=1`):** enables zeroing of vacated/freed/padding bytes,
  deferred reuse of retired blocks inside `edit()`/`read()` scopes, and sender-side re-validation on
  save. Default builds optimize for throughput and do not scrub freed arena bytes.
- **Denial-of-service limits:** callers handling untrusted inputs must set appropriate per-buffer byte
  limits and `max_growth_ratio` / container caps according to their application budget.

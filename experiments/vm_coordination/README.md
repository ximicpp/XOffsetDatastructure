# VM shared execution entry point

This is a POSIX local-process runner. All participating W1/W2/W3/W4 builds,
functional tests, diagnostics and timing windows use the default stable lock:
`build/vm-coordination/queue.lock`. Never delete, rename or replace that file.
Metadata is descriptive; only the operating-system lock establishes ownership.

```sh
python3 experiments/vm_coordination/run_job.py status
python3 experiments/vm_coordination/run_job.py run \
  --owner W1 --kind build --out build/vm-network/NEW-build-receipt \
  --timeout-seconds 600 -- cmake --build build/vm-network/SELECTED --parallel 2
python3 experiments/vm_coordination/run_job.py audit --out build/vm-network/NEW-build-receipt
```

`--out` must not exist, including for a retry after busy/failure. Argument lists
are executed directly; there is no implicit shell. `--wait-seconds` defaults to
15, and `--termination-grace-seconds` to 2. Waiting for the lock does not consume
the command timeout. Use one `--kind timing` job for the complete registered
measurement window. There is no FIFO fairness guarantee.

| Exit code | Result status | Meaning |
| --- | --- | --- |
| 0 | complete | Command returned zero; owned session has no live processes; evidence finalized |
| 1 | failed | Nonzero command or leader exited leaving live descendants |
| 2 | no job created | CLI/unsupported daemon operation rejected |
| 70 | error | Launch, inspection, output or supervisor failure |
| 75 | busy | Bounded lock wait expired; command was never started |
| 124 | timeout | Deadline expired; cleanup precedes unlocking |
| 130 | cancelled | Signal, explicit cancellation or public-client disappearance |

`status` returns zero and JSON `idle`/`busy`; it does not infer lock ownership
from timestamps. A stale JSON record never breaks a held operating-system lock.

The public client talks to a separate supervisor. That supervisor holds the lock
across command launch, execution, cancellation and cleanup. Commands have a new
session and stdin is `/dev/null`, separate from the cancellation pipe. SIGINT,
SIGTERM, SIGHUP and even SIGKILL of the **public client** are handled through the
supervisor. Do not forcibly kill the supervisor itself. Session members are
enumerated only at command exit/cleanup, so formal timing has no repeated `ps`
probes while the command runs. Each signaled PID is checked against the owned
session. A child may change process group within that session. A leader that
leaves live children is failed and its remaining work terminated. Zombie PIDs
are reported separately; they cannot execute and may await the OS reaper.

Commands must retain descendants in that session. `setsid`, double-fork
daemonization, remote jobs, job schedulers and daemon-controlled work are outside
this contract. Do not conceal those operations in a shell/Python wrapper. Direct
Docker/Podman/nerdctl/ctr commands and reserved `--docker` are rejected before any
output is created. A portable process-group cleanup cannot prove a daemon's
container stopped. Docker remains unavailable until a separate owned-container
create/verify/start/stop protocol and actual daemon cleanup tests are accepted.
No container was started or stopped to validate this release.

The requested deadline bounds command execution. Cleanup may exceed that bound
if the OS does not yet terminate a process; the supervisor retains the lock
instead of allowing concurrent heavy work. Failure to inspect owned processes
also retains the lock and retries cleanup. This coordinates participating jobs;
it cannot prevent unrelated background load from affecting timing.

Each receipt has an exclusive `request.json`, stdout/stderr logs (if launched),
`supervisor.log`, `events.json`, `result.json` and `manifest.json`. The request
records the exact argv, working directory, runner hash and current production
header hashes. It does not dump the environment. The manifest recursively hashes
all regular files under the receipt directory, excluding only its own root
manifest; symlinks and special files are refused. Files outside that directory,
including a command's separate build/result directory, need the consumer's own
inventory/audit. Receipt audit checks integrity and result consistency offline;
it never executes the command or claims the command's assertions were correct.
It is a checksum audit, not an authenticity signature.

Lightweight validation uses separate lock directories and retains every run:

```sh
python3 experiments/vm_coordination/test_run_job.py \
  --evidence-root build/vm-coordination/NEW-tests
```

The production queue was not acquired by these tests. Test fixtures launch only
short Python processes, and cancel only their own clients/descendants. Results
are tied to the runner hash in each new evidence directory. Earlier runs refer
to earlier source hashes and remain unchanged.

# cereal v1.3.2 experimental adapter

Trusted BinaryArchive profile, detached native std-container state. The state is
sc::Snapshot itself because std::string/std::vector are cereal's natural native
representation. Query/mutation operate those containers; no oracle operation or
precomputed final state is called. export_state is an audit-only copy.

A complete erase stage first checks ordered statuses (including repeated/missing
IDs), builds a removal set, then performs one stable remove_if/erase pass. Set
construction and all movement/cleanup occur inside mutate; no future values are
pre-reserved. This strategy was chosen before any performance run.

load uses a read-only span-backed stream to reconstruct a fresh state. It neither
adopts nor retains the caller buffer; archive, stream and buffer die before return.
emit moves the stream's owning std::string out. All state allocations are owned
by the unique_ptr and standard containers and released by ordinary RAII.

Default BinaryArchive is intentionally not PortableBinary. This same-machine
trusted profile is not an adversarial-input admission: cereal-controlled length
allocation has not been made a bounded U loader. Total wire and schema checks do
not imply that malicious internal lengths are rejected before allocation.

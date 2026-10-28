// Device mesh demo: schema.lock (xmesh_schema_freeze). OWNER: types thread.
//
//   xmesh_schema_freeze write LOCK    writes the fingerprints of the eleven roots
//   xmesh_schema_freeze check LOCK    fails when this build's differ from LOCK
//
// The fingerprints are those of INTERFACES.md section 3: sizeof, alignof,
// wire_schema_hash_v and schema_name_digest_v. Any change to schema.hpp
// changes them, and with them every wire and the Hello handshake; `check`
// runs with the hub (C++26 reflection or C++20 field lists), and the
// self-test compares the same values with the frozen table on every device.
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include "types/fingerprints.hpp"

namespace {

std::string lock_text() {
    std::string out = "# demo/mesh/schema.lock: xmesh_schema_freeze write (INTERFACES.md section 3)\n";
    out += "version " + std::to_string(XMESH_SCHEMA_VERSION) + "\n";
    out += "# root sizeof alignof wire_schema_hash schema_name_digest\n";
    for (const xmesh::fp::Fingerprint& f : xmesh::fp::current()) {
        char line[160];
        std::snprintf(line, sizeof line, "root %s %u %u %016" PRIx64 " %016" PRIx64 "\n", f.name, f.size, f.align,
                      f.schema_hash, f.name_digest);
        out += line;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3 || (std::strcmp(argv[1], "write") != 0 && std::strcmp(argv[1], "check") != 0)) {
        std::fprintf(stderr, "usage: %s write|check LOCK\n", argc ? argv[0] : "xmesh_schema_freeze");
        return 2;
    }
    const std::string want = lock_text();
    if (std::strcmp(argv[1], "write") == 0) {
        std::FILE* f = std::fopen(argv[2], "wb");
        if (!f || std::fwrite(want.data(), 1, want.size(), f) != want.size() || std::fclose(f) != 0) {
            std::fprintf(stderr, "schema_freeze: cannot write %s\n", argv[2]);
            return 1;
        }
        std::printf("schema_freeze: wrote %s\n", argv[2]);
        return 0;
    }
    std::string have;
    if (std::FILE* f = std::fopen(argv[2], "rb")) {
        char buf[4096];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) have.append(buf, n);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "schema_freeze: cannot read %s\n", argv[2]);
        return 1;
    }
    if (have != want) {
        std::fprintf(stderr, "schema_freeze: this build's fingerprints differ from %s\n--- lock\n%s--- this build\n%s",
                     argv[2], have.c_str(), want.c_str());
        return 1;
    }
    std::printf("schema_freeze: %s matches (11 roots)\n", argv[2]);
    return 0;
}

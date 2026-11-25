// Must not compile: a type that cannot be a wire type, sent with
// save_verified. One case per build, chosen by WIRE_FAIL_<CASE>;
// tests/CMakeLists.txt checks that the diagnostic names the rule broken, not
// only the generic "requires an admitted fixed-schema wire type".
#include "../../xoffsetdatastructure.hpp"

#include <cstdint>

using namespace XOffsetDatastructure;

#if defined(WIRE_FAIL_ALIGN)            // a SIMD-style vector in a container
struct alignas(16) Vec4 { float x, y, z, w; };
XOFFSET_RECORD(Vec4, x, y, z, w)
struct Mesh { std::int32_t id; XVector<Vec4> points; };
XOFFSET_RECORD(Mesh, id, points)
#elif defined(WIRE_FAIL_POINTER)        // a pointer member
struct Mesh { std::int32_t id; const char* label; };
XOFFSET_RECORD(Mesh, id, label)
#elif defined(WIRE_FAIL_BASE)           // a base class
struct Shape { std::int32_t id; };
XOFFSET_RECORD(Shape, id)
struct Mesh : Shape { std::int32_t points; };
XOFFSET_RECORD_DERIVED(Mesh, (Shape), points)
#else
#error "define one WIRE_FAIL_<CASE>"
#endif
XOFFSET_REGISTER_SCHEMA_NAME(Mesh, "test.wire_admission.Mesh")

int main() {
    auto buffer = TypedXBuffer<Mesh>::create(4096);
    return static_cast<int>(buffer.save_verified().size());
}

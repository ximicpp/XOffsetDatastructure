// Test: This file SHOULD NOT COMPILE
// Purpose: Demonstrate compile-time error when using polymorphic types

#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct BadType {
    int id;
    virtual void process() {}  // ❌ Virtual function makes this polymorphic
};

int main() {
    XBufferExt xbuf(1024);
    
    // This line should cause a COMPILE ERROR with a helpful message
    auto* obj = xbuf.make<BadType>("test");
    
    // Expected error message:
    // static assertion failed: Cannot use polymorphic types in XBuffer!
    // Polymorphic types contain virtual function table pointers (vptr) that:
    //   - Are process-specific memory addresses
    //   - Cannot be serialized or deserialized
    //   - Will cause crashes when loaded in different processes
    //
    // Solution: Use plain POD types or non-polymorphic classes.
    // Example:
    //   ❌ BAD:  struct Data { virtual void f() {} };
    //   ✅ GOOD: struct Data { void f() {} };  // No 'virtual' keyword
    
    return 0;
}

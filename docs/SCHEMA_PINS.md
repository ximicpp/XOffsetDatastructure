# Pinning user schema signatures (K7)

`XOFFSET_PIN_SCHEMA` pins a type's wire layout to a piece of signature text. When the layout computed by
any compiler for any target platform differs from that text, **compilation fails** right away, and the
error message gives the layout that compiler computed. When the builds of both sending and receiving ends
(the server and each platform's clients) include the same pin file, object representation mismatches are
found at build time, rather than at run time when `load_verified` rejects a different schema fingerprint.

## Usage

1. Generate the pin text (run once, on any one supported compiler):

   ```cpp
   #include "my_schema.hpp"
   #include <iostream>
   int main() {
       std::cout << XOffsetDatastructure::schema_pin<game::Player>("game::Player") << "\n";
   }
   ```

   The output looks like:

   ```cpp
   XOFFSET_PIN_SCHEMA(game::Player, "record[s:96,a:8]{@0:u64,...}");
   ```

2. Put the output into a pin file (for example `my_schema_pins.hpp`), commit it, and include it at least
   once in every build that sends or receives the data (any one translation unit is enough).

3. When you change the schema on purpose, regenerate and commit the pin file. Reviewers see the layout
   change in the pin file's diff.

## Notes

- The signature covers the whole layout: field offsets, scalar types and widths, container header format,
  alignment, array lengths, and enum underlying types. It is the input of the schema fingerprint in the
  wire header (the fingerprint also includes the registered schema name).
- The macro can go at namespace scope or inside a function body. When the type name contains a comma,
  give it an alias with `using` first.
- Each build is evaluated by its own compiler, which when cross-compiling is the target platform's
  compiler, so Windows (MinGW) client builds are bound by it too. Android and iOS clients use the C++20
  field-list build (since internal-3.2.0), see [CLIENT_TARGETS.md](CLIENT_TARGETS.md).
- This is a build-time check and does not replace run-time validation: `load_verified` still fully
  validates every wire.
- The `schema_pin_mismatch` test in CTest confirms that a mismatch really fails to compile.

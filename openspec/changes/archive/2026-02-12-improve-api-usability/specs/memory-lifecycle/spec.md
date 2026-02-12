## ADDED Requirements

### Requirement: XString Direct Assignment
The library SHALL provide `operator=(const char*)` and `operator=(std::string_view)` on XString
so that users can assign string values without manually constructing temporaries with allocators.

#### Scenario: Assign C-string to XString member
- **WHEN** a user writes `player->name = "Alice"`
- **THEN** XString uses its internally-held allocator to call `assign("Alice", 5)`
- **AND** the previous string data in the segment is deallocated
- **AND** the new string data is allocated in the same segment

#### Scenario: Assign string_view to XString member
- **WHEN** a user writes `player->name = std::string_view("Bob")`
- **THEN** XString uses its internally-held allocator to perform the assignment
- **AND** the behavior is equivalent to `assign(sv.data(), sv.size())`

### Requirement: Pointer Invalidation Documentation
The library SHALL document in README.md a "Critical Safety Rules" section that explicitly
warns users about pointer/reference/iterator invalidation after `grow()`, `shrink_to_fit()`,
and `update_after_shrink()`.

#### Scenario: User reads README before using grow()
- **WHEN** a user reads the README "Critical Safety Rules" section
- **THEN** they find explicit warnings about pointer invalidation after buffer mutations
- **AND** they find recommended patterns (re-acquire via `find<T>()`)

### Requirement: Bulk Deallocation Documentation
The library SHALL document that segment destruction does NOT invoke individual object
destructors, and that the Domain S constraint (no external resource handles) is the
safety guarantee.

#### Scenario: User understands destructor semantics
- **WHEN** a user reads the "Critical Safety Rules" section
- **THEN** they understand that `~XBufferExt()` bulk-frees the backing store
- **AND** they understand that Domain S types MUST NOT hold external resources (file handles, sockets)

### Requirement: Thread Safety Documentation
The library SHALL document that XBufferExt uses `null_mutex_family` and is NOT thread-safe.
Any concurrent access MUST be externally synchronized.

#### Scenario: User reads thread safety warning
- **WHEN** a user reads the library documentation
- **THEN** they find an explicit "NOT thread-safe" warning
- **AND** they find guidance on external synchronization strategies

### Requirement: Serialization Optimization
The library SHALL provide `save_to_vector()` returning `std::vector<char>` and `used_size()`
to allow efficient serialization without copying unused free space.

#### Scenario: Save only used bytes
- **WHEN** a user calls `xbuf.save_to_vector()`
- **THEN** the returned vector contains only the used portion of the segment
- **AND** the size is equal to `xbuf.used_size()`

### Requirement: Buffer Capacity Estimation
The library SHALL provide `estimate_buffer_size(user_data_bytes)` that returns a recommended
initial buffer size accounting for segment manager overhead.

#### Scenario: Estimate buffer size for 1KB user data
- **WHEN** a user calls `estimate_buffer_size(1024)`
- **THEN** the returned value accounts for segment header (~128B), free-list overhead, and index root
- **AND** the returned value is sufficient to hold 1024 bytes of user data without immediate grow()

### Requirement: Type Safety Gate on construct()
The library SHALL enforce `validate_xbuffer_type<T>()` at the `construct()` level or
clearly document that `construct()` is an internal/advanced API that bypasses safety checks.
Users MUST be guided to use `make<T>()` instead.

#### Scenario: Unsafe type rejected at construct level
- **WHEN** a user calls `xbuf.construct<UnsafeType>(name)(xbuf.get_segment_manager())`
  where `UnsafeType` contains a `std::string` member
- **THEN** the compilation fails with a static_assert error
- **AND** the error message directs the user to use `make<T>()` or fix the type

#### Scenario: Safe type accepted at construct level
- **WHEN** a user calls `xbuf.construct<SafeType>(name)(xbuf.get_segment_manager())`
  where `SafeType` contains only safe members
- **THEN** the object is constructed normally in the segment

### Requirement: Transparent Container Lookup
The library SHALL support looking up XMap and XSet entries using `const char*` directly,
without requiring the user to construct a temporary XString with an allocator.

#### Scenario: Map lookup by const char*
- **WHEN** a user writes `data->scores["Alice"]`
- **THEN** the lookup uses a transparent comparator to compare `const char*` against XString keys
- **AND** no temporary XString allocation occurs in the segment

#### Scenario: Set find by const char*
- **WHEN** a user writes `data->string_set.find("item")`
- **THEN** the find operation accepts `const char*` through a transparent comparator
- **AND** returns an iterator without constructing a temporary XString

### Requirement: Named Object Deletion
The library SHALL provide `XBufferExt::remove<T>(const char* name)` as the symmetric
counterpart to `make<T>(name)`, wrapping the underlying `destroy<T>(name)`.

#### Scenario: Delete a named object
- **WHEN** a user calls `xbuf.remove<Player>("Hero")`
- **THEN** the named object is destroyed and its memory returned to the segment free-list
- **AND** subsequent `find_ex<Player>("Hero")` returns `{nullptr, false}`

### Requirement: shrink_to_fit Optimization
The library SHALL minimize data copies in `shrink_to_fit()` to at most 2 copies
(currently 3), by eliminating the redundant copy in `update_after_shrink()`.

#### Scenario: Efficient shrink operation
- **WHEN** a user calls `xbuf.shrink_to_fit()`
- **THEN** the buffer is compacted with at most 2 full data copies
- **AND** all data integrity is preserved
- **AND** all existing pointers/references are invalidated (same as before)

### Requirement: Per-Member Type Safety Diagnostics
The library SHALL use C++26 reflection to provide per-member static_assert diagnostics
when `validate_xbuffer_type<T>()` fails, identifying exactly which member is unsafe.

#### Scenario: Struct with one unsafe member
- **WHEN** a user defines a struct with one `std::string` member and calls `make<T>()`
- **THEN** the compiler error identifies the specific unsafe member by name
- **AND** the error message suggests the safe alternative (e.g., "use XString instead")

# CK10D: SLG A hot path machine code, anchor vs head

anchor `/Volumes/XOffsetCS/perf-gate/build/ck10d/anc/slg_bench`, head `/Volumes/XOffsetCS/perf-gate/build/ck10d/head/slg_bench`. 7305 / 7608 functions in total.

| function | anc size | head size | anc %64 | head %64 | anc %128 | head %128 | strict | loose |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `XOffsetDatastructure::XBufferCore::FreeTableCache::~FreeTableCache()` | 200 | 200 | 32 | 28 | 32 | 28 | differs | DIFFERS |
| `XOffsetDatastructure::XBufferCore::take_free_block(XOffsetDatastructure::XBufferCore::FreeTable&, unsigned int, XOffsetDatastructure::XBufferCore::Fre` | 724 | 740 | 40 | 0 | 104 | 64 | differs | DIFFERS |
| `void XOffsetDatastructure::XBufferCore::as_arena_failure<XOffsetDatastructure::XBufferCore::reinit_from_wire(XOffsetDatastructure::XBufferCore::WirePa` | 380 | 380 | 12 | 36 | 12 | 36 | differs | DIFFERS |
| `XOffsetDatastructure::XBufferCore::FreeTableRecycler::clear_entries(XOffsetDatastructure::XBufferCore::FreeTable&)` | 456 | 468 | 16 | 0 | 16 | 0 | differs | DIFFERS |
| `XOffsetDatastructure::XBufferCore::FreeTableRecycler::recycle(XOffsetDatastructure::XBufferCore::FreeTable*, bool)` | 788 | 784 | 20 | 52 | 20 | 52 | differs | DIFFERS |
| `void XOffsetDatastructure::XBufferCore::reinit_from_storage<XOffsetDatastructure::XBufferCore::reinit_from_wire(XOffsetDatastructure::XBufferCore::Wir` | 980 | 980 | 60 | 20 | 124 | 20 | differs | identical |
| `XOffsetDatastructure::XBufferCore::insert_free_block_inline(unsigned int, unsigned int)` | 472 | 480 | 0 | 48 | 64 | 48 | differs | DIFFERS |
| `XOffsetDatastructure::XBufferCore::load_free_blocks_general(void const*, unsigned long)` | 1540 | 1556 | 0 | 8 | 0 | 8 | differs | DIFFERS |
| `XOffsetDatastructure::XBufferCore::insert_free_block_general(unsigned int, unsigned int)` | 1172 | 1180 | 44 | 36 | 108 | 100 | differs | DIFFERS |
| `void XOffsetDatastructure::XBufferCore::reinit_from_storage_shrinking<XOffsetDatastructure::XBufferCore::reinit_from_wire(XOffsetDatastructure::XBuffe` | 1084 | 1084 | 16 | 40 | 80 | 104 | differs | identical |
| `XOffsetDatastructure::XBufferCore::FreeTable::next_range()` | | | | | | | only in head | |
| `void XOffsetDatastructure::detail::direct_value<XOffsetDatastructure::XString, false>(XOffsetDatastructure::XString const&, unsigned long, XOffsetData` | 600 | 600 | 28 | 56 | 92 | 120 | differs | identical |
| `void XOffsetDatastructure::detail::direct_value<XOffsetDatastructure::XString, true>(XOffsetDatastructure::XString const&, unsigned long, XOffsetDatas` | 600 | 600 | 16 | 44 | 80 | 108 | differs | identical |
| `void XOffsetDatastructure::detail::direct_value<XOffsetDatastructure::XVector<slg::xm::Hero>, false>(XOffsetDatastructure::XVector<slg::xm::Hero> cons` | 384 | 384 | 36 | 0 | 100 | 0 | identical | identical |
| `void XOffsetDatastructure::detail::direct_value<XOffsetDatastructure::XVector<slg::xm::Hero>, true>(XOffsetDatastructure::XVector<slg::xm::Hero> const` | 292 | 292 | 36 | 0 | 100 | 0 | identical | identical |
| `void XOffsetDatastructure::detail::direct_value<XOffsetDatastructure::XVector<slg::xmr::Hero>, false>(XOffsetDatastructure::XVector<slg::xmr::Hero> co` | 384 | 384 | 8 | 16 | 72 | 80 | identical | identical |
| `void XOffsetDatastructure::detail::direct_value<XOffsetDatastructure::XVector<slg::xmr::Hero>, true>(XOffsetDatastructure::XVector<slg::xmr::Hero> con` | 292 | 292 | 32 | 40 | 32 | 40 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xm::Army, XOffsetDatastructure::detail::DirectWriter<false>, XOffsetDatastructure::detail::Compo` | 188 | 188 | 48 | 12 | 48 | 76 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xm::Army, XOffsetDatastructure::detail::DirectWriter<true>, XOffsetDatastructure::detail::Compos` | 188 | 188 | 32 | 60 | 96 | 124 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xm::BattleMsg, XOffsetDatastructure::detail::DirectWriter<false>, XOffsetDatastructure::detail::` | 256 | 256 | 48 | 12 | 48 | 76 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xm::BattleMsg, XOffsetDatastructure::detail::DirectWriter<true>, XOffsetDatastructure::detail::C` | 188 | 188 | 28 | 56 | 92 | 120 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xm::BattleMsg, XOffsetDatastructure::detail::CompactMeasure, XOffsetDatastructure::detail::Compo` | 180 | 180 | 0 | 28 | 0 | 28 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xmr::Army, XOffsetDatastructure::detail::DirectWriter<false>, XOffsetDatastructure::detail::Comp` | 188 | 188 | 20 | 28 | 20 | 28 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xmr::Army, XOffsetDatastructure::detail::DirectWriter<true>, XOffsetDatastructure::detail::Compo` | 188 | 188 | 28 | 36 | 28 | 36 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xmr::BattleMsg, XOffsetDatastructure::detail::DirectWriter<false>, XOffsetDatastructure::detail:` | 256 | 256 | 20 | 28 | 20 | 28 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xmr::BattleMsg, XOffsetDatastructure::detail::DirectWriter<true>, XOffsetDatastructure::detail::` | 188 | 188 | 24 | 32 | 24 | 32 | identical | identical |
| `void XOffsetDatastructure::detail::compose_value<slg::xmr::BattleMsg, XOffsetDatastructure::detail::CompactMeasure, XOffsetDatastructure::detail::Comp` | 180 | 180 | 12 | 20 | 12 | 20 | identical | identical |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_compact_into<slg::xm::ViewSnapshot>(XOffsetDatastructure::XBufferCore&, std::__1::basic_s` | 1824 | 1824 | 12 | 40 | 12 | 40 | differs | DIFFERS |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xm::BattleMsg, slg::xm::Army, slg::xm::Army>(XOffsetDatastructure::XBu` | 1916 | 1916 | 56 | 20 | 56 | 84 | differs | DIFFERS |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xmr::BattleMsg, slg::xmr::Army, slg::xmr::Army>(XOffsetDatastructure::` | 1916 | 1916 | 52 | 60 | 116 | 124 | differs | DIFFERS |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xm::Army>(XOffsetDatastructure::XFixedVector<slg::xm::Army> const&, XOffsetDatastructure::` | 404 | 404 | 20 | 36 | 20 | 100 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xm::Buff>(XOffsetDatastructure::XFixedVector<slg::xm::Buff> const&, XOffsetDatastructure::` | 340 | 340 | 40 | 4 | 104 | 4 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xm::Hero>(XOffsetDatastructure::XFixedVector<slg::xm::Hero> const&, XOffsetDatastructure::` | 412 | 412 | 36 | 0 | 36 | 64 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xm::Skill>(XOffsetDatastructure::XFixedVector<slg::xm::Skill> const&, XOffsetDatastructure` | 340 | 340 | 20 | 48 | 20 | 48 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xm::Troop>(XOffsetDatastructure::XFixedVector<slg::xm::Troop> const&, XOffsetDatastructure` | 340 | 340 | 60 | 24 | 60 | 88 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xm::UnitHero>(XOffsetDatastructure::XFixedVector<slg::xm::UnitHero> const&, XOffsetDatastr` | 340 | 340 | 28 | 56 | 28 | 56 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xmr::Army>(XOffsetDatastructure::XFixedVector<slg::xmr::Army> const&, XOffsetDatastructure` | 404 | 404 | 56 | 60 | 56 | 124 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xmr::Buff>(XOffsetDatastructure::XFixedVector<slg::xmr::Buff> const&, XOffsetDatastructure` | 340 | 340 | 0 | 8 | 0 | 8 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xmr::Hero>(XOffsetDatastructure::XFixedVector<slg::xmr::Hero> const&, XOffsetDatastructure` | 412 | 412 | 16 | 24 | 16 | 24 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xmr::Skill>(XOffsetDatastructure::XFixedVector<slg::xmr::Skill> const&, XOffsetDatastructu` | 340 | 340 | 44 | 52 | 44 | 52 | differs | identical |
| `void XOffsetDatastructure::detail::validate_wire_value<slg::xmr::Troop>(XOffsetDatastructure::XFixedVector<slg::xmr::Troop> const&, XOffsetDatastructu` | 340 | 340 | 20 | 28 | 84 | 92 | differs | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::ShardState>(slg::xm::` | 1144 | 1144 | 24 | 44 | 88 | 108 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::PlayerPatch>(slg::xm:` | 1144 | 1144 | 36 | 16 | 36 | 80 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::PlayerState>(slg::xm:` | 1144 | 1144 | 36 | 52 | 100 | 52 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::ViewSnapshot>(slg::xm` | 1144 | 1144 | 4 | 32 | 68 | 96 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::Report>(slg::xm::Repo` | 1144 | 1144 | 52 | 16 | 52 | 16 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::BattleMsg>(slg::xm::B` | 1144 | 1144 | 28 | 56 | 92 | 120 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::ViewDelta>(slg::xm::V` | 1144 | 1144 | 32 | 12 | 32 | 76 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::ShardState>(slg::xmr` | 1144 | 1144 | 60 | 40 | 60 | 104 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::PlayerState>(slg::xm` | 1144 | 1144 | 56 | 60 | 56 | 124 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::Report>(slg::xmr::Re` | 1144 | 1144 | 52 | 16 | 116 | 16 | identical | identical |
| `void XOffsetDatastructure::detail::for_each_unowned_run<void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::BattleMsg>(slg::xmr:` | 1144 | 1144 | 24 | 32 | 24 | 32 | identical | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::ShardState>(slg::xm::ShardState const&, XOffsetDatastructure::XBufferCore&, s` | 904 | 904 | 16 | 36 | 80 | 100 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::PlayerPatch>(slg::xm::PlayerPatch const&, XOffsetDatastructure::XBufferCore&,` | 892 | 892 | 40 | 20 | 40 | 84 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::PlayerState>(slg::xm::PlayerState const&, XOffsetDatastructure::XBufferCore&,` | 900 | 900 | 32 | 48 | 96 | 48 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::ViewSnapshot>(slg::xm::ViewSnapshot const&, XOffsetDatastructure::XBufferCore` | 904 | 904 | 60 | 24 | 60 | 88 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::Report>(slg::xm::Report const&, XOffsetDatastructure::XBufferCore&, std::__1:` | 892 | 892 | 56 | 20 | 56 | 20 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::BattleMsg>(slg::xm::BattleMsg const&, XOffsetDatastructure::XBufferCore&, std` | 964 | 964 | 48 | 12 | 48 | 76 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xm::ViewDelta>(slg::xm::ViewDelta const&, XOffsetDatastructure::XBufferCore&, std` | 928 | 928 | 0 | 44 | 0 | 44 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::ShardState>(slg::xmr::ShardState const&, XOffsetDatastructure::XBufferCore&,` | 904 | 904 | 52 | 32 | 52 | 96 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::PlayerState>(slg::xmr::PlayerState const&, XOffsetDatastructure::XBufferCore` | 900 | 900 | 52 | 56 | 52 | 120 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::Report>(slg::xmr::Report const&, XOffsetDatastructure::XBufferCore&, std::__` | 892 | 892 | 56 | 20 | 120 | 20 | differs | identical |
| `void XOffsetDatastructure::detail::verify_structural_wire_graph<slg::xmr::BattleMsg>(slg::xmr::BattleMsg const&, XOffsetDatastructure::XBufferCore&, s` | 964 | 964 | 20 | 28 | 84 | 92 | differs | identical |
| `XOffsetDatastructure::detail::RawWireVerification XOffsetDatastructure::detail::verify_structural_wire_graph_raw_at<slg::xmr::BattleMsg>(char const*, ` | 592 | 592 | 28 | 0 | 28 | 64 | differs | identical |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::ShardState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_re` | 1256 | 1256 | 36 | 56 | 36 | 56 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::PlayerPatch>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_r` | 1256 | 1256 | 4 | 48 | 68 | 112 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::PlayerState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_r` | 1256 | 1256 | 4 | 20 | 4 | 84 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::ViewSnapshot>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_` | 1256 | 1256 | 8 | 36 | 72 | 100 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::Report>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resour` | 1256 | 1256 | 36 | 0 | 100 | 64 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::BattleMsg>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_res` | 1256 | 1256 | 24 | 52 | 88 | 116 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xm::ViewDelta>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_res` | 1256 | 1256 | 24 | 4 | 24 | 68 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::ShardState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_r` | 1256 | 1256 | 8 | 52 | 8 | 52 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::PlayerState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_` | 1256 | 1256 | 40 | 44 | 40 | 108 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::Report>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resou` | 1256 | 1256 | 36 | 0 | 36 | 64 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::BattleMsg>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_re` | 1256 | 1256 | 0 | 8 | 0 | 8 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::ShardState>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastructu` | 544 | 544 | 24 | 44 | 88 | 108 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::PlayerPatch>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastruct` | 544 | 544 | 56 | 36 | 120 | 36 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::PlayerState>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastruct` | 544 | 544 | 56 | 8 | 56 | 8 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::ViewSnapshot>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastruc` | 544 | 544 | 60 | 24 | 124 | 24 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::Report>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastructure::` | 544 | 544 | 24 | 52 | 24 | 116 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::BattleMsg>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastructur` | 544 | 544 | 16 | 44 | 16 | 44 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xm::ViewDelta>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastructur` | 544 | 544 | 12 | 56 | 76 | 120 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xmr::ShardState>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastruct` | 544 | 544 | 60 | 40 | 60 | 104 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xmr::PlayerState>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastruc` | 544 | 544 | 28 | 32 | 92 | 32 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xmr::Report>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastructure:` | 544 | 544 | 24 | 52 | 88 | 116 | differs | identical |
| `void XOffsetDatastructure::XBuffer::admit_wire_header<slg::xmr::BattleMsg>(std::__1::span<std::byte const, 18446744073709551615ul>, XOffsetDatastructu` | 544 | 544 | 52 | 60 | 52 | 60 | differs | identical |
| `void XOffsetDatastructure::XBuffer::save_compact_into<slg::xm::ViewSnapshot>(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::alloc` | 340 | 340 | 56 | 20 | 56 | 84 | differs | identical |
| `void XOffsetDatastructure::XBuffer::save_compact_into<slg::xm::BattleMsg, slg::xm::Army, slg::xm::Army>(std::__1::basic_string<char, std::__1::char_tr` | 428 | 428 | 12 | 40 | 12 | 40 | differs | identical |
| `void XOffsetDatastructure::XBuffer::save_compact_into<slg::xmr::BattleMsg, slg::xmr::Army, slg::xmr::Army>(std::__1::basic_string<char, std::__1::char` | 428 | 428 | 8 | 16 | 72 | 80 | differs | identical |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::ShardState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, ` | 844 | 844 | 12 | 32 | 12 | 32 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::PlayerPatch>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long,` | 844 | 844 | 44 | 24 | 44 | 88 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::PlayerState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long,` | 844 | 844 | 44 | 60 | 108 | 60 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::ViewSnapshot>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long` | 844 | 844 | 48 | 12 | 48 | 76 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::Report>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std:` | 844 | 844 | 12 | 40 | 76 | 40 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::BattleMsg>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, s` | 844 | 844 | 0 | 28 | 64 | 92 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::ViewDelta>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, s` | 844 | 844 | 0 | 44 | 0 | 44 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::ShardState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long,` | 844 | 844 | 48 | 28 | 112 | 28 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::PlayerState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long` | 844 | 844 | 16 | 20 | 16 | 84 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::Report>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std` | 844 | 844 | 12 | 40 | 12 | 40 | differs | DIFFERS |
| `void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::BattleMsg>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, ` | 844 | 844 | 40 | 48 | 104 | 112 | differs | DIFFERS |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda0'()>(slg::CycleStat` | 348 | 348 | 52 | 40 | 52 | 40 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda10'()>(slg::CycleSta` | 336 | 336 | 52 | 40 | 52 | 40 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda11'()>(slg::CycleSta` | 348 | 348 | 4 | 56 | 4 | 120 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda12'()>(slg::CycleSta` | 444 | 444 | 16 | 4 | 80 | 68 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda13'()>(slg::CycleSta` | 340 | 340 | 36 | 24 | 36 | 24 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda14'()>(slg::CycleSta` | 348 | 348 | 56 | 44 | 120 | 108 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda15'()>(slg::CycleSta` | 524 | 524 | 20 | 8 | 84 | 72 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda16'()>(slg::CycleSta` | 308 | 308 | 32 | 20 | 96 | 84 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda17'()>(slg::CycleSta` | 336 | 336 | 20 | 8 | 20 | 8 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda18'()>(slg::CycleSta` | 308 | 308 | 36 | 24 | 100 | 88 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda19'()>(slg::CycleSta` | 348 | 348 | 24 | 12 | 24 | 12 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda1'()>(slg::CycleStat` | 324 | 324 | 56 | 44 | 120 | 108 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda20'()>(slg::CycleSta` | 524 | 524 | 52 | 40 | 116 | 104 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda21'()>(slg::CycleSta` | 464 | 464 | 20 | 8 | 84 | 72 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda22'()>(slg::CycleSta` | 348 | 348 | 36 | 24 | 36 | 24 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda23'()>(slg::CycleSta` | 432 | 432 | 56 | 44 | 120 | 108 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda24'()>(slg::CycleSta` | 464 | 464 | 56 | 44 | 56 | 44 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda25'()>(slg::CycleSta` | 432 | 432 | 8 | 60 | 8 | 124 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda26'()>(slg::CycleSta` | 308 | 308 | 56 | 44 | 56 | 44 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda27'()>(slg::CycleSta` | 428 | 428 | 4 | 56 | 4 | 120 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda28'()>(slg::CycleSta` | 1696 | 1696 | 20 | 8 | 20 | 8 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda29'()>(slg::CycleSta` | 596 | 596 | 52 | 40 | 52 | 40 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda2'()>(slg::CycleStat` | 340 | 340 | 16 | 4 | 16 | 4 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda3'()>(slg::CycleStat` | 464 | 464 | 36 | 24 | 100 | 88 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda4'()>(slg::CycleStat` | 336 | 336 | 52 | 40 | 52 | 40 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda5'()>(slg::CycleStat` | 308 | 308 | 4 | 56 | 4 | 120 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda6'()>(slg::CycleStat` | 336 | 336 | 56 | 44 | 56 | 44 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda7'()>(slg::CycleStat` | 348 | 348 | 8 | 60 | 8 | 124 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda8'()>(slg::CycleStat` | 444 | 444 | 56 | 44 | 56 | 44 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda9'()>(slg::CycleStat` | 524 | 524 | 40 | 28 | 40 | 28 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda'()>(slg::CycleStats` | 344 | 344 | 28 | 16 | 92 | 80 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda0'()>(slg::CycleStats` | 336 | 336 | 24 | 12 | 24 | 12 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda10'()>(slg::CycleStat` | 336 | 336 | 52 | 40 | 116 | 104 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda11'()>(slg::CycleStat` | 336 | 336 | 4 | 56 | 68 | 56 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda12'()>(slg::CycleStat` | 444 | 444 | 8 | 60 | 8 | 124 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda13'()>(slg::CycleStat` | 340 | 340 | 24 | 12 | 88 | 76 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda14'()>(slg::CycleStat` | 336 | 336 | 44 | 32 | 44 | 32 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda15'()>(slg::CycleStat` | 524 | 524 | 60 | 48 | 124 | 112 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda16'()>(slg::CycleStat` | 308 | 308 | 8 | 60 | 8 | 124 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda17'()>(slg::CycleStat` | 336 | 336 | 60 | 48 | 60 | 48 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda18'()>(slg::CycleStat` | 324 | 324 | 12 | 0 | 12 | 0 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda19'()>(slg::CycleStat` | 344 | 344 | 16 | 4 | 80 | 68 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda1'()>(slg::CycleStats` | 324 | 324 | 40 | 28 | 104 | 92 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda20'()>(slg::CycleStat` | 524 | 524 | 40 | 28 | 40 | 28 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda21'()>(slg::CycleStat` | 464 | 464 | 8 | 60 | 8 | 124 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda22'()>(slg::CycleStat` | 336 | 336 | 24 | 12 | 88 | 76 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda23'()>(slg::CycleStat` | 432 | 432 | 40 | 28 | 104 | 92 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda24'()>(slg::CycleStat` | 464 | 464 | 32 | 20 | 96 | 84 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda25'()>(slg::CycleStat` | 432 | 432 | 48 | 36 | 48 | 36 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda26'()>(slg::CycleStat` | 308 | 308 | 32 | 20 | 96 | 84 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda27'()>(slg::CycleStat` | 428 | 428 | 52 | 40 | 116 | 104 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda28'()>(slg::CycleStat` | 1664 | 1664 | 60 | 48 | 60 | 48 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda29'()>(slg::CycleStat` | 584 | 584 | 60 | 48 | 60 | 48 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda2'()>(slg::CycleStats` | 340 | 340 | 40 | 28 | 104 | 92 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda3'()>(slg::CycleStats` | 452 | 452 | 60 | 48 | 60 | 48 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda4'()>(slg::CycleStats` | 336 | 336 | 0 | 52 | 0 | 116 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda5'()>(slg::CycleStats` | 308 | 308 | 16 | 4 | 80 | 68 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda6'()>(slg::CycleStats` | 336 | 336 | 4 | 56 | 4 | 120 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda7'()>(slg::CycleStats` | 336 | 336 | 20 | 8 | 84 | 72 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda8'()>(slg::CycleStats` | 444 | 444 | 48 | 36 | 112 | 100 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda9'()>(slg::CycleStats` | 524 | 524 | 40 | 28 | 104 | 92 | differs | identical |
| `void slg::timed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda'()>(slg::CycleStats&` | 344 | 344 | 0 | 52 | 64 | 52 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda12'()>(slg::CycleS` | 324 | 324 | 32 | 20 | 96 | 84 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda23'()>(slg::CycleS` | 312 | 312 | 0 | 52 | 0 | 116 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda27'()>(slg::CycleS` | 308 | 308 | 44 | 32 | 108 | 96 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda8'()>(slg::CycleSt` | 324 | 324 | 36 | 24 | 100 | 88 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda12'()>(slg::CycleSt` | 324 | 324 | 20 | 8 | 20 | 8 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda23'()>(slg::CycleSt` | 312 | 312 | 40 | 28 | 40 | 28 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda27'()>(slg::CycleSt` | 308 | 308 | 20 | 8 | 20 | 8 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda8'()>(slg::CycleSta` | 324 | 324 | 36 | 24 | 36 | 24 | differs | identical |
| `slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)` | 5492 | 5492 | 12 | 0 | 12 | 0 | differs | identical |
| `slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)` | 5492 | 5492 | 48 | 36 | 48 | 36 | differs | identical |
| `void std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>::resize_and_overwrite[abi:ne210000]<void XOffsetDatastructur` | 440 | 440 | 28 | 56 | 92 | 120 | identical | identical |
| `void std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>::resize_and_overwrite[abi:ne210000]<void XOffsetDatastructur` | 440 | 440 | 48 | 12 | 112 | 12 | identical | identical |
| `void std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>::resize_and_overwrite[abi:ne210000]<void XOffsetDatastructur` | 440 | 440 | 44 | 52 | 44 | 52 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xm::Hero, false>(slg::xm::Hero const&, un` | 632 | 632 | 36 | 0 | 100 | 0 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xm::Hero, true>(slg::xm::Hero const&, uns` | 456 | 456 | 8 | 36 | 8 | 36 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xm::BattleMsg, false>(slg::xm::BattleMsg ` | 196 | 196 | 60 | 24 | 60 | 88 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xm::BattleMsg, true>(slg::xm::BattleMsg c` | 392 | 392 | 40 | 4 | 40 | 68 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xmr::Hero, false>(slg::xmr::Hero const&, ` | 632 | 632 | 8 | 16 | 72 | 80 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xmr::Hero, true>(slg::xmr::Hero const&, u` | 456 | 456 | 4 | 12 | 68 | 76 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xmr::BattleMsg, false>(slg::xmr::BattleMs` | 196 | 196 | 8 | 16 | 72 | 80 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::direct_value<slg::xmr::BattleMsg, true>(slg::xmr::BattleMsg` | 392 | 392 | 12 | 20 | 12 | 20 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::compose_value<slg::xm::BattleMsg, XOffsetDatastructure::det` | 328 | 328 | 24 | 52 | 24 | 52 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::compose_value<slg::xm::BattleMsg, XOffsetDatastructure::det` | 512 | 512 | 52 | 16 | 52 | 80 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::compose_value<slg::xmr::BattleMsg, XOffsetDatastructure::de` | 328 | 328 | 20 | 28 | 84 | 92 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::compose_value<slg::xmr::BattleMsg, XOffsetDatastructure::de` | 512 | 512 | 0 | 8 | 64 | 72 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<6ul, void XOffsetDatastructure::detail::validate_wire_value_raw<slg::xmr::BattleMsg>(char const*, X` | 356 | 356 | 44 | 16 | 44 | 80 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<7ul, void XOffsetDatastructure::detail::direct_value<slg::xm::Army, false>(slg::xm::Army const&, un` | 632 | 632 | 44 | 8 | 108 | 8 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<7ul, void XOffsetDatastructure::detail::direct_value<slg::xm::Army, true>(slg::xm::Army const&, uns` | 456 | 456 | 28 | 56 | 28 | 56 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<7ul, void XOffsetDatastructure::detail::direct_value<slg::xmr::Army, false>(slg::xmr::Army const&, ` | 632 | 632 | 16 | 24 | 80 | 88 | identical | identical |
| `auto void XOffsetDatastructure::detail::static_for<7ul, void XOffsetDatastructure::detail::direct_value<slg::xmr::Army, true>(slg::xmr::Army const&, u` | 456 | 456 | 24 | 32 | 88 | 96 | identical | identical |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_compact_into<slg::xm::ViewSnapshot>(XOffsetDatastructure::XBufferCore&, std::__1::basic_s` | 304 | 304 | 44 | 8 | 44 | 72 | differs | identical |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xm::BattleMsg, slg::xm::Army, slg::xm::Army>(XOffsetDatastructure::XBu` | 316 | 316 | 52 | 16 | 52 | 80 | differs | identical |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xmr::BattleMsg, slg::xmr::Army, slg::xmr::Army>(XOffsetDatastructure::` | 316 | 316 | 48 | 56 | 112 | 120 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda12'()>(slg::CycleS` | 92 | 92 | 52 | 40 | 116 | 104 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda1'()>(slg::CycleSt` | 92 | 92 | 28 | 16 | 28 | 16 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda23'()>(slg::CycleS` | 92 | 92 | 28 | 16 | 28 | 16 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda27'()>(slg::CycleS` | 92 | 92 | 40 | 28 | 40 | 28 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<false>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda8'()>(slg::CycleSt` | 92 | 92 | 28 | 16 | 92 | 80 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda12'()>(slg::CycleSt` | 92 | 92 | 44 | 32 | 44 | 32 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda1'()>(slg::CycleSta` | 92 | 92 | 12 | 0 | 12 | 0 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda23'()>(slg::CycleSt` | 92 | 92 | 12 | 0 | 12 | 0 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda27'()>(slg::CycleSt` | 92 | 92 | 24 | 12 | 24 | 12 | differs | identical |
| `void slg::untimed<slg::StdRunner<slg::FlatBuffersFormatT<true>>::run_cycle_reuse(int, slg::CycleStats&, slg::SampleWires*)::'lambda8'()>(slg::CycleSta` | 92 | 92 | 20 | 8 | 20 | 8 | differs | identical |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_compact_into<slg::xm::ViewSnapshot>(XOffsetDatastructure::XBufferCore&, std::__1::basic_s` | 436 | 436 | 20 | 48 | 20 | 48 | differs | DIFFERS |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xm::BattleMsg, slg::xm::Army, slg::xm::Army>(XOffsetDatastructure::XBu` | 436 | 436 | 40 | 4 | 40 | 68 | differs | DIFFERS |
| `void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xmr::BattleMsg, slg::xmr::Army, slg::xmr::Army>(XOffsetDatastructure::` | 436 | 436 | 36 | 44 | 100 | 108 | differs | DIFFERS |

Summary: strict 45 identical / 162 differ; loose 171 identical / 36 differ; 1 only in one binary.

## XOffsetDatastructure::XBufferCore::FreeTableCache::~FreeTableCache()
```diff
--- anchor
+++ head
@@ -15,8 +15,8 @@
 mov	w21, #0x3088
 adrp x9, PAGE
-ldr	d8, [x9, #0xe30]
+ldr	d8, [x9, #0xf0]
 b	+0x5c
 mov	x0, x20
-mov	w1, #0x30b8
+mov	w1, #0x30c0
 bl	ADDR ; symbol stub for: __ZdlPvm
 ldr	x8, [x19, #0x20]
```

## XOffsetDatastructure::XBufferCore::take_free_block(XOffsetDatastructure::XBufferCore::FreeTable&, unsigned int, XOffsetDatastructure::XBufferCore::FreeSplit&)
```diff
--- anchor
+++ head
@@ -6,5 +6,5 @@
 add	x29, sp, #0x40
 cmp	w2, #0x1, lsl #12
-b.hi	+0x18c
+b.hi	+0x194
 lsr	w10, w2, #3
 sub	w9, w10, #0x1
@@ -12,5 +12,5 @@
 umaddl	x11, w9, w8, x1
 ldr	w8, [x11]
-cbz	w8, +0xd0
+cbz	w8, +0xd8
 ldr	w10, [x11, #0x4]
 mov	x12, x11
@@ -40,4 +40,6 @@
 mov.d	v1[0], x9
 sub.2d	v0, v0, v1
+mov	w9, #0x30b8
+strb	w10, [x1, x9]
 str	q0, [x1, #0x30a0]
 ldp	x10, x9, [x0, #0x28]
@@ -53,5 +55,5 @@
 lsr	w8, w2, #9
 cmp	w2, #0x1, lsl #12
-b.eq	+0xf4
+b.eq	+0xfc
 add	x9, x1, w8, uxtw #3
 ldr	x9, [x9, #0x3000]
@@ -59,15 +61,15 @@
 lsl	x10, x11, x10
 ands	x9, x9, x10
-b.ne	+0x11c
+b.ne	+0x124
 ldr	w9, [x1, #0x3040]
 mov	w10, #-0x2
 lsl	w8, w10, w8
 ands	w8, w9, w8
-b.eq	+0x18c
+b.eq	+0x194
 rbit	w8, w8
 clz	w8, w8
 add	x9, x1, w8, uxtw #3
 ldr	x9, [x9, #0x3000]
-cbz	x9, +0x18c
+cbz	x9, +0x194
 rbit	x9, x9
 clz	x9, x9
@@ -85,5 +87,5 @@
 ldr	w20, [x12, w10, uxtw #2]
 str	w10, [x11]
-b.ne	+0x280
+b.ne	+0x288
 add	x11, x1, w8, uxtw #3
 mov	w10, #0x1
@@ -92,12 +94,12 @@
 bics	x9, x12, x9
 str	x9, [x11, #0x3000]
-b.ne	+0x280
+b.ne	+0x288
 lsl	w8, w10, w8
 ldr	w9, [x1, #0x3040]
 bic	w8, w9, w8
 str	w8, [x1, #0x3040]
-b	+0x280
+b	+0x288
 ldr	x8, [x1, #0x3090]
-cbz	x8, +0x1e0
+cbz	x8, +0x1e8
 mov	w9, #0x3090
 add	x9, x1, x9
@@ -110,13 +112,13 @@
 csel	x19, x19, x11, lo
 ldr	x11, [x11, x12]
-cbnz	x11, +0x1a8
+cbnz	x11, +0x1b0
 cmp	x19, x9
-b.eq	+0x1e0
+b.eq	+0x1e8
 ldr	x10, [x19, #0x8]
-cbz	x10, +0x1fc
+cbz	x10, +0x204
 mov	x9, x10
 ldr	x10, [x10]
-cbnz	x10, +0x1d0
-b	+0x214
+cbnz	x10, +0x1d8
+b	+0x21c
 mov	x0, #0x0
 ldp	x29, x30, [sp, #0x40]
@@ -131,5 +133,5 @@
 cmp	x10, x11
 mov	x10, x9
-b.ne	+0x200
+b.ne	+0x208
 mov	x24, x3
 mov	x22, x2
@@ -138,5 +140,5 @@
 ldr	x10, [x1, #0x3088]
 cmp	x10, x19
-b.ne	+0x234
+b.ne	+0x23c
 str	x9, [x1, #0x3088]
 ldr	x9, [x1, #0x3098]
@@ -149,5 +151,5 @@
 mov	x3, x24
 ldr	x0, [x24, #0x8]
-cbz	x0, +0x268
+cbz	x0, +0x270
 mov	w1, #0x28
 bl	ADDR ; symbol stub for: __ZdlPvm
@@ -160,12 +162,14 @@
 mov	x2, x22
 mov	w8, w21
+ldr	q0, [x1, #0x30a0]
 mov	w9, #0x1
-dup.2d	v0, x9
-ldr	q1, [x1, #0x30a0]
-mov.d	v0[0], x8
-sub.2d	v0, v1, v0
+dup.2d	v1, x9
+mov.d	v1[0], x8
+sub.2d	v0, v0, v1
 str	q0, [x1, #0x30a0]
+mov	w8, #0x30b8
+strb	w9, [x1, x8]
 subs	w8, w21, w2
-b.ls	+0x2ac
+b.ls	+0x2bc
 add	w9, w20, w2
 stp	w9, w8, [x3]
```

## void XOffsetDatastructure::XBufferCore::as_arena_failure<XOffsetDatastructure::XBufferCore::reinit_from_wire(XOffsetDatastructure::XBufferCore::WirePayload const&, unsigned long)::'lambda'()&>(XOffset
```diff
--- anchor
+++ head
@@ -24,5 +24,5 @@
 ldr	x10, [x10]
 adrp x11, PAGE
-ldr	q0, [x11, #0xd90]
+ldr	q0, [x11, #0xfe0]
 str	q0, [x8]
 stp	x9, x10, [x8, #0x10]
```

## XOffsetDatastructure::XBufferCore::FreeTableRecycler::clear_entries(XOffsetDatastructure::XBufferCore::FreeTable&)
```diff
--- anchor
+++ head
@@ -110,4 +110,7 @@
 str	q0, [x19, #0x3090]
 str	q0, [x19, #0x30a0]
+mov	w8, #0x30b8
+mov	w9, #0x1
+strb	w9, [x19, x8]
 ldp	x29, x30, [sp, #0x10]
 ldp	x20, x19, [sp], #0x20
```

## XOffsetDatastructure::XBufferCore::FreeTableRecycler::recycle(XOffsetDatastructure::XBufferCore::FreeTable*, bool)
```diff
--- anchor
+++ head
@@ -24,5 +24,5 @@
 adrp x21, PAGE
 ldr	x21, [x21, #OFF] ; literal pool symbol address: __ZZN20XOffsetDatastructure11XBufferCore16free_table_cacheEvE5cache
-tbz	w8, #0x0, +0x2dc
+tbz	w8, #0x0, +0x2d8
 ldr	x8, [x21]
 mov	x0, x21
@@ -31,5 +31,5 @@
 cmp	x8, #0x4
 b.ne	+0x140
-cbz	x19, +0x2b8
+cbz	x19, +0x2b4
 mov	w8, #0x3088
 ldr	x1, [x19, #0x3090]
@@ -38,5 +38,5 @@
 mov	w20, #0x3000
 adrp x8, PAGE
-ldr	d8, [x8, #0xe30]
+ldr	d8, [x8, #0xf0]
 add	x21, x19, x20
 ldur	w8, [x21, #-0x14]
@@ -50,5 +50,5 @@
 b.eq	+0x114
 b	+0xa0
-cbz	x19, +0x2b8
+cbz	x19, +0x2b4
 mov	w8, #0x3088
 ldr	x1, [x19, #0x3090]
@@ -57,5 +57,5 @@
 mov	w20, #0x3000
 adrp x8, PAGE
-ldr	d8, [x8, #0xe30]
+ldr	d8, [x8, #0xf0]
 add	x21, x19, x20
 ldur	w8, [x21, #-0x14]
@@ -69,5 +69,5 @@
 b.ne	+0xec
 mov	x0, x19
-mov	w1, #0x30b8
+mov	w1, #0x30c0
 ldp	x29, x30, [sp, #0x80]
 ldp	x20, x19, [sp, #0x70]
@@ -80,33 +80,33 @@
 b	ADDR ; symbol stub for: __ZdlPvm
 mov	x21, x0
+add	x9, x19, #0x3, lsl #12
 cbz	w20, +0x154
-mov	w8, #0x30b0
-ldrb	w8, [x19, x8]
-tbz	w8, #0x0, +0x2a0
-str	x21, [sp, #0x8]
-mov	x22, #0x0
+ldrb	w8, [x9, #0xb9]
+tbz	w8, #0x0, +0x29c
+stp	x9, x21, [sp]
 mov	x23, #0x0
+mov	x24, #0x0
 mov	w8, #0x3048
 add	x10, x19, x8
 add	x11, x19, #0x3, lsl #12
-mov	w26, #0x18
-mov	w27, #0x4000
+mov	w27, #0x18
+mov	w28, #0x4000
 adrp x8, PAGE
-ldr	d8, [x8, #0xe30]
+ldr	d8, [x8, #0xf0]
 stp	x11, x10, [sp, #0x10]
-ldr	x25, [x10, x22, lsl #3]
-lsl	w28, w22, #6
+ldr	x26, [x10, x23, lsl #3]
+lsl	w22, w23, #6
 mov	x21, #0x0
-cbz	x25, +0x210
-rbit	x8, x25
+cbz	x26, +0x210
+rbit	x8, x26
 clz	x8, x8
-orr	x8, x28, x8
-umaddl	x24, w8, w26, x19
-ldr	w8, [x24, #0x4]
+orr	x8, x22, x8
+umaddl	x25, w8, w27, x19
+ldr	w8, [x25, #0x4]
 cmp	w8, #0x4
 csel	w10, w8, wzr, hi
 cbz	w20, +0x1c4
 ubfiz	x9, x10, #2, #32
-sub	x11, x27, x23
+sub	x11, x28, x24
 cmp	w10, #0x40
 ccmp	x9, x11, #0x2, ls
@@ -114,36 +114,36 @@
 cmp	w8, #0x5
 b.lo	+0x1d8
-ldr	x0, [x24, #0x8]
+ldr	x0, [x25, #0x8]
 cbz	x0, +0x1d8
 bl	ADDR ; symbol stub for: __ZdaPv
-str	d8, [x24]
+str	d8, [x25]
 b	+0x1f4
-str	wzr, [x24]
-add	x23, x9, x23
-neg	x8, x25
-and	x8, x25, x8
+str	wzr, [x25]
+add	x24, x9, x24
+neg	x8, x26
+and	x8, x26, x8
 orr	x21, x8, x21
-sub	x8, x25, #0x1
-ands	x25, x8, x25
+sub	x8, x26, #0x1
+ands	x26, x8, x26
 b.ne	+0x190
 ldp	x11, x10, [sp, #0x10]
-ldr	x8, [x10, x22, lsl #3]
+ldr	x8, [x10, x23, lsl #3]
 mvn	x8, x8
 b	+0x214
 mov	x8, #-0x1
-ldr	x9, [x11, x22, lsl #3]
+ldr	x9, [x11, x23, lsl #3]
 ands	x8, x9, x8
 b.eq	+0x23c
 rbit	x9, x8
 clz	x9, x9
-orr	x9, x28, x9
-umull	x9, w9, w26
+orr	x9, x22, x9
+umull	x9, w9, w27
 str	wzr, [x19, x9]
 sub	x9, x8, #0x1
 b	+0x218
-str	xzr, [x11, x22, lsl #3]
-str	x21, [x10, x22, lsl #3]
-add	x22, x22, #0x1
-cmp	x22, #0x8
+str	xzr, [x11, x23, lsl #3]
+str	x21, [x10, x23, lsl #3]
+add	x23, x23, #0x1
+cmp	x23, #0x8
 b.ne	+0x180
 str	wzr, [x19, #0x3040]
@@ -157,14 +157,13 @@
 movi.2d	v0, #0000000000000000
 str	q0, [x19, #0x3090]
-mov	w8, #0x30a0
-add	x8, x19, x8
 str	q0, [x19, #0x30a0]
-strb	wzr, [x8, #0x10]
-ldr	x10, [sp, #0x8]
+mov	w8, #0x1
+ldp	x9, x10, [sp]
+strh	w8, [x9, #0xb8]
 ldr	x8, [x10, #0x20]
 add	x9, x8, #0x1
 str	x9, [x10, #0x20]
 str	x19, [x10, x8, lsl #3]
-b	+0x2b8
+b	+0x2b4
 mov	x0, x19
 bl	__ZN20XOffsetDatastructure11XBufferCore17FreeTableRecycler13clear_entriesERNS0_9FreeTableE
```

## XOffsetDatastructure::XBufferCore::insert_free_block_inline(unsigned int, unsigned int)
```diff
--- anchor
+++ head
@@ -9,5 +9,5 @@
 add	x29, sp, #0x30
 ldr	x8, [x0, #0xa8]
-cbz	x8, +0xbc
+cbz	x8, +0xc4
 lsr	w9, w2, #3
 sub	w9, w9, #0x1
@@ -16,5 +16,5 @@
 ldp	w10, w12, [x11]
 cmp	w10, w12
-b.eq	+0x12c
+b.eq	+0x134
 mov	x13, x11
 ldr	x14, [x13, #0x8]!
@@ -36,9 +36,11 @@
 str	w9, [x8, #0x3040]
 mov	w9, w2
-ldr	q0, [x8, #0x30a0]
-dup.2d	v1, x0
-mov.d	v1[0], x9
-add.2d	v0, v0, v1
+dup.2d	v0, x0
+mov.d	v0[0], x9
+ldr	q1, [x8, #0x30a0]
+add.2d	v0, v1, v0
 str	q0, [x8, #0x30a0]
+mov	w9, #0x30b8
+strb	w0, [x8, x9]
 ldp	x29, x30, [sp, #0x30]
 ldp	x20, x19, [sp, #0x20]
@@ -53,5 +55,5 @@
 ldrb	w8, [x0]
 cmp	w8, #0x1
-b.ne	+0x12c
+b.ne	+0x134
 adrp x22, PAGE
 ldr	x22, [x22, #OFF] ; literal pool symbol address: __ZGVZN20XOffsetDatastructure11XBufferCore16free_table_cacheEvE5cache
@@ -62,9 +64,9 @@
 adrp x0, PAGE
 ldr	x0, [x0, #OFF] ; literal pool symbol address: __ZZN20XOffsetDatastructure11XBufferCore16free_table_cacheEvE5cache
-tbz	w10, #0x0, +0x16c
+tbz	w10, #0x0, +0x174
 ldr	x8, [x0]
 blr	x8
 ldr	x8, [x0, #0x20]
-cbz	x8, +0x12c
+cbz	x8, +0x134
 sub	x8, x8, #0x1
 str	x8, [x0, #0x20]
@@ -72,5 +74,5 @@
 ldr	x0, [x20, #0xa8]
 str	x8, [x20, #0xa8]
-cbnz	x0, +0x144
+cbnz	x0, +0x14c
 cbnz	x8, +0x2c
 mov	w0, #0x0
@@ -89,5 +91,5 @@
 ldr	x8, [x21, #0xa8]
 cbnz	x8, +0x2c
-b	+0x12c
+b	+0x134
 ldr	x10, [x0]
 blr	x10
@@ -115,4 +117,4 @@
 blr	x8
 ldr	x8, [x0, #0x20]
-cbnz	x8, +0x110
-b	+0x12c
+cbnz	x8, +0x118
+b	+0x134
```

## XOffsetDatastructure::XBufferCore::load_free_blocks_general(void const*, unsigned long)
```diff
--- anchor
+++ head
@@ -1,44 +1,45 @@
-sub	sp, sp, #0x100
-stp	x28, x27, [sp, #0xa0]
-stp	x26, x25, [sp, #0xb0]
-stp	x24, x23, [sp, #0xc0]
-stp	x22, x21, [sp, #0xd0]
-stp	x20, x19, [sp, #0xe0]
-stp	x29, x30, [sp, #0xf0]
-add	x29, sp, #0xf0
+sub	sp, sp, #0x110
+stp	x28, x27, [sp, #0xb0]
+stp	x26, x25, [sp, #0xc0]
+stp	x24, x23, [sp, #0xd0]
+stp	x22, x21, [sp, #0xe0]
+stp	x20, x19, [sp, #0xf0]
+stp	x29, x30, [sp, #0x100]
+add	x29, sp, #0x100
 mov	x24, x2
 mov	x23, x1
-mov	x11, x0
-ldp	x8, x9, [x0, #0x18]
-stp	x8, x9, [x29, #-0x68]
-cbz	x9, +0x44
-add	x9, x9, #0x8
-mov	w10, #0x1
-ldadd	x10, x9, [x9]
-cbz	x24, +0x468
-str	x11, [sp]
-sub	x8, x29, #0x70
+mov	x10, x0
+ldr	x8, [x0, #0x20]
+ldur	q0, [x0, #0x18]
+stur	q0, [x29, #-0x70]
+cbz	x8, +0x48
+add	x8, x8, #0x8
+mov	w9, #0x1
+ldadd	x9, x8, [x8]
+cbz	x24, +0x474
+str	x10, [sp, #0x8]
+sub	x8, x29, #0x78
 bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
 mov	x28, #0x0
-ldur	x22, [x29, #-0x70]
+ldur	x22, [x29, #-0x78]
 add	x25, x22, #0x3, lsl #12
 mov	w8, #0x3090
 add	x8, x22, x8
-str	x8, [sp, #0x58]
+str	x8, [sp, #0x60]
 mov	w8, #0x3048
 add	x8, x22, x8
-stp	x8, x25, [sp, #0x18]
+stp	x8, x25, [sp, #0x20]
 sub	x9, x23, #0x4
 add	x10, x23, #0xc
 neg	x8, x24
-stp	x8, x10, [sp, #0x38]
+stp	x8, x10, [sp, #0x40]
 add	x8, x23, #0x40
-stp	x8, x9, [sp, #0x8]
+stp	x8, x9, [sp, #0x10]
 mov	x26, #-ADDR
 movk	x26, #0x0, lsl #48
 mov	w21, #0x1
-str	x22, [sp, #0x48]
-stp	x23, x24, [sp, #0x28]
-b	+0x100
+str	x22, [sp, #0x50]
+stp	x23, x24, [sp, #0x30]
+b	+0x104
 ldr	w8, [x17]
 add	w8, w8, w20
@@ -62,5 +63,5 @@
 mov	x28, x15
 cmp	x15, x24
-b.hs	+0x458
+b.hs	+0x45c
 lsl	x12, x28, #3
 add	x8, x23, x12
@@ -68,12 +69,12 @@
 lsr	x27, x19, #32
 cmp	x19, x26
-b.hi	+0x164
+b.hi	+0x168
 add	x15, x28, #0x1
 cmp	x15, x24
-str	x12, [sp, #0x60]
-str	x8, [sp, #0x78]
-b.hs	+0x208
+str	x12, [sp, #0x68]
+str	x8, [sp, #0x80]
+b.hs	+0x20c
 sub	x8, x24, x28
-ldp	x10, x9, [sp, #0x38]
+ldp	x10, x9, [sp, #0x40]
 add	x9, x9, x12
 add	x10, x10, x28
@@ -81,30 +82,30 @@
 ldr	w11, [x9], #0x8
 cmp	w11, w27
-b.ne	+0x230
+b.ne	+0x234
 add	x20, x20, #0x1
 cmn	x10, x20
-b.ne	+0x140
+b.ne	+0x144
 mov	x20, x8
 mov	x15, x24
-b	+0x234
+b	+0x238
 mov	w0, #0x28
 bl	ADDR ; symbol stub for: __Znwm
 mov	x1, x0
 stp	w27, w19, [x1, #0x1c]
-ldr	x10, [sp, #0x58]
+ldr	x10, [sp, #0x60]
 mov	x8, x10
 mov	x9, x10
 ldr	x10, [x10]
-cbnz	x10, +0x194
-b	+0x1b0
+cbnz	x10, +0x198
+b	+0x1b4
 ldr	x10, [x8, #0x8]
-cbz	x10, +0x210
+cbz	x10, +0x214
 mov	x8, x10
 ldr	w9, [x10, #0x1c]
 cmp	w9, w27
-b.ls	+0x18c
+b.ls	+0x190
 ldr	x10, [x8]
 mov	x9, x8
-cbnz	x10, +0x194
+cbnz	x10, +0x198
 stp	xzr, xzr, [x1]
 str	x8, [x1, #0x10]
@@ -112,5 +113,5 @@
 ldr	x8, [x22, #0x3088]
 ldr	x8, [x8]
-cbz	x8, +0x1cc
+cbz	x8, +0x1d0
 str	x8, [x22, #0x3088]
 ldr	x0, [x22, #0x3090]
@@ -127,8 +128,8 @@
 mov	x28, x15
 cmp	x15, x24
-b.lo	+0x100
-b	+0x458
+b.lo	+0x104
+b	+0x45c
 mov	w20, #0x1
-b	+0x234
+b	+0x238
 add	x9, x8, #0x8
 stp	xzr, xzr, [x1]
@@ -137,6 +138,6 @@
 ldr	x8, [x22, #0x3088]
 ldr	x8, [x8]
-cbnz	x8, +0x1c8
-b	+0x1cc
+cbnz	x8, +0x1cc
+b	+0x1d0
 add	x15, x28, x20
 lsr	w8, w27, #3
@@ -148,9 +149,9 @@
 cmp	w8, w20
 lsr	w0, w16, #6
-b.hs	+0x2bc
-str	w0, [sp, #0x6c]
+b.hs	+0x2c0
+str	w0, [sp, #0x74]
 mov	x21, x17
-str	x16, [sp, #0x50]
-str	x15, [sp, #0x70]
+str	x16, [sp, #0x58]
+str	x15, [sp, #0x78]
 lsl	w8, w26, #1
 add	w9, w19, w20
@@ -164,5 +165,5 @@
 mov	x23, x0
 mov	x17, x21
-cbz	w19, +0x2e0
+cbz	w19, +0x2e4
 mov	x8, x17
 ldr	x24, [x8, #0x8]!
@@ -173,5 +174,5 @@
 bl	ADDR ; symbol stub for: _memcpy
 mov	x17, x21
-b	+0x2e4
+b	+0x2e8
 mov	x8, x17
 ldr	x9, [x8, #0x8]!
@@ -181,9 +182,9 @@
 mov	x26, #-ADDR
 movk	x26, #0x0, lsl #48
-cbnz	x20, +0x35c
-b	+0xa8
+cbnz	x20, +0x360
+b	+0xac
 ldr	x24, [x17, #0x8]
-ldr	x15, [sp, #0x70]
-ldr	w0, [sp, #0x6c]
+ldr	x15, [sp, #0x78]
+ldr	w0, [sp, #0x74]
 mov	x22, x17
 str	x23, [x22, #0x8]!
@@ -191,10 +192,10 @@
 cmp	w26, #0x5
 ccmp	x24, #0x0, #0x4, hs
-b.eq	+0x328
+b.eq	+0x32c
 mov	x0, x24
 bl	ADDR ; symbol stub for: __ZdaPv
-ldr	w0, [sp, #0x6c]
+ldr	w0, [sp, #0x74]
 mov	x17, x21
-ldr	x15, [sp, #0x70]
+ldr	x15, [sp, #0x78]
 ldp	w19, w8, [x21]
 ldr	x9, [x21, #0x8]
@@ -202,30 +203,30 @@
 csel	x23, x9, x22, hi
 add	x8, x23, w19, uxtw #2
-ldp	x11, x25, [sp, #0x18]
+ldp	x11, x25, [sp, #0x20]
 mov	w9, #0x1
-strb	w9, [x25, #0xb0]
-ldp	x22, x16, [sp, #0x48]
+strb	w9, [x25, #0xb9]
+ldp	x22, x16, [sp, #0x50]
 lsl	x9, x9, x16
 ldr	x10, [x11, w0, uxtw #3]
 orr	x9, x10, x9
 str	x9, [x11, w0, uxtw #3]
-ldp	x23, x24, [sp, #0x28]
+ldp	x23, x24, [sp, #0x30]
 mov	x26, #-ADDR
 movk	x26, #0x0, lsl #48
-cbz	x20, +0xa8
+cbz	x20, +0xac
 cmp	x20, #0x4
-b.ls	+0x394
+b.ls	+0x398
 add	x9, x8, x20, lsl #2
 add	x10, x20, x28
-ldr	x11, [sp, #0x10]
+ldr	x11, [sp, #0x18]
 add	x10, x11, x10, lsl #3
 cmp	x8, x10
-ldr	x10, [sp, #0x78]
+ldr	x10, [sp, #0x80]
 ccmp	x10, x9, #0x2, lo
-b.lo	+0x394
+b.lo	+0x398
 cmp	x20, #0x11
-b.hs	+0x3bc
+b.hs	+0x3c0
 mov	x9, #0x0
-b	+0x41c
+b	+0x420
 mov	x9, #0x0
 sub	x10, x20, x9
@@ -236,6 +237,6 @@
 str	w9, [x8], #0x4
 subs	x10, x10, #0x1
-b.ne	+0x3a8
-b	+0xa8
+b.ne	+0x3ac
+b	+0xac
 ands	x9, x20, #0xf
 mov	w10, #0x10
@@ -243,6 +244,6 @@
 sub	x9, x20, x10
 add	x11, x8, #0x20
-ldr	x12, [sp, #0x8]
-ldr	x13, [sp, #0x60]
+ldr	x12, [sp, #0x10]
+ldr	x13, [sp, #0x68]
 add	x13, x12, x13
 mov	x12, x20
@@ -259,7 +260,7 @@
 sub	x12, x12, #0x10
 cmp	x10, x12
-b.ne	+0x3e0
+b.ne	+0x3e4
 cmp	x10, #0x5
-b.lo	+0x398
+b.lo	+0x39c
 mov	x11, x9
 ands	x9, x20, #0x3
@@ -275,24 +276,27 @@
 str	q0, [x10], #0x10
 adds	x11, x11, #0x4
-b.ne	+0x444
-b	+0x398
-ldur	x8, [x29, #-0x68]
-ldr	x11, [sp]
```

## XOffsetDatastructure::XBufferCore::insert_free_block_general(unsigned int, unsigned int)
```diff
--- anchor
+++ head
@@ -7,5 +7,5 @@
 stp	x29, x30, [sp, #0x80]
 add	x29, sp, #0x80
-mov	x20, x2
+mov	x21, x2
 mov	x23, x1
 mov	x22, x0
@@ -17,17 +17,17 @@
 strb	wzr, [sp, #0x10]
 strb	wzr, [sp, #0x28]
-cmp	w20, #0x1, lsl #12
+cmp	w21, #0x1, lsl #12
 b.ls	+0xd0
 add	x8, sp, #0x10
-add	x21, x8, #0x8
+add	x20, x8, #0x8
 stp	xzr, xzr, [sp, #0x18]
-str	x21, [sp, #0x10]
+str	x20, [sp, #0x10]
 mov	w8, #0x1
 strb	w8, [sp, #0x28]
 mov	w0, #0x28
 bl	ADDR ; symbol stub for: __Znwm
-stp	w20, w23, [x0, #0x1c]
+stp	w21, w23, [x0, #0x1c]
 stp	xzr, xzr, [x0]
-str	x21, [x0, #0x10]
+str	x20, [x0, #0x10]
 stp	x0, x0, [sp, #0x10]
 mov	x1, x0
@@ -39,25 +39,25 @@
 ldapur	x8, [x25, #0x8]
 cbz	x8, +0x1ac
-mov	x21, #0x0
+mov	x20, #0x0
 ldr	x26, [x8, #0xa8]
 cbnz	x26, +0x248
-cbnz	x21, +0x23c
-add	x8, sp, #0x8
-bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
-ldr	x21, [sp, #0x8]
+cbnz	x20, +0x23c
+add	x8, sp, #0x8
+bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
+ldr	x20, [sp, #0x8]
 ldapur	x8, [x25, #0x8]
 cbnz	x8, +0xa4
 mov	x22, #0x0
-mov	w20, #0x1
-b	+0x3c0
-stp	w23, w20, [sp]
+mov	w21, #0x1
+b	+0x3c8
+stp	w23, w21, [sp]
 mov	w27, #0x0
 mov	x24, #0x0
-mov	x21, #0x0
-lsr	w8, w20, #3
+mov	x20, #0x0
+lsr	w8, w21, #3
 sub	w28, w8, #0x1
 b	+0xfc
-mov	x27, x20
-ubfiz	x0, x20, #2, #32
+mov	x27, x21
+ubfiz	x0, x21, #2, #32
 bl	ADDR ; symbol stub for: __Znam
 mov	x24, x0
@@ -67,8 +67,8 @@
 ldr	x26, [x8, #0xa8]
 cbnz	x26, +0x154
-cbnz	x21, +0x148
-add	x8, sp, #0x8
-bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
-ldr	x21, [sp, #0x8]
+cbnz	x20, +0x148
+add	x8, sp, #0x8
+bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
+ldr	x20, [sp, #0x8]
 ldapur	x8, [x25, #0x8]
 cbnz	x8, +0x108
@@ -76,11 +76,11 @@
 add	x8, sp, #0x8
 bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
-ldr	x21, [sp, #0x8]
+ldr	x20, [sp, #0x8]
 ldr	x26, [x22, #0xa8]
 cbnz	x26, +0x154
-cbz	x21, +0x12c
+cbz	x20, +0x12c
 mov	x8, x22
-mov	x26, x21
-mov	x21, #0x0
+mov	x26, x20
+mov	x20, #0x0
 str	x26, [x8, #0xa8]
 mov	w8, #0x18
@@ -92,6 +92,6 @@
 cmp	w9, #0x8
 mov	w10, #0x8
-csel	w20, w9, w10, hi
-cmp	w27, w20
+csel	w21, w9, w10, hi
+cmp	w27, w21
 b.hs	+0x1f8
 cbz	x24, +0xec
@@ -99,20 +99,20 @@
 bl	ADDR ; symbol stub for: __ZdaPv
 b	+0xec
-mov	w20, #0x1
+mov	w21, #0x1
 mov	x22, x24
-b	+0x3c0
+b	+0x3c8
 ldr	x26, [x22, #0xa8]
 cbz	x26, +0x1bc
-mov	x21, #0x0
+mov	x20, #0x0
 b	+0x248
 mov	x22, #0x0
-mov	x21, #0x0
-mov	w20, #0x1
-b	+0x3c0
-mov	x21, #0x0
-cbnz	x21, +0x238
-add	x8, sp, #0x8
-bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
-ldr	x21, [sp, #0x8]
+mov	x20, #0x0
+mov	w21, #0x1
+b	+0x3c8
+mov	x20, #0x0
+cbnz	x20, +0x238
+add	x8, sp, #0x8
+bl	__ZN20XOffsetDatastructure11XBufferCore15make_free_tableEv
+ldr	x20, [sp, #0x8]
 ldr	x26, [x22, #0xa8]
 cbz	x26, +0x1c0
@@ -125,23 +125,23 @@
 mov	x22, x24
 b	+0x360
-mov	w9, #0x30b0
+mov	w9, #0x30b9
 mov	w10, #0x1
 strb	w10, [x26, x9]
 cbz	w8, +0x334
-mov	x20, x23
-ldr	x9, [x20, #0x8]!
+mov	x21, x23
+ldr	x9, [x21, #0x8]!
 cmp	w8, #0x4
-csel	x1, x9, x20, hi
+csel	x1, x9, x21, hi
 lsl	x2, x8, #2
 mov	x0, x24
 bl	ADDR ; symbol stub for: _memcpy
 ldp	w8, w9, [x23]
-ldr	x10, [x20]
+ldr	x10, [x21]
 cmp	w9, #0x4
 csel	x22, x10, xzr, hi
 b	+0x338
 mov	x8, x22
-mov	x26, x21
-mov	x21, #0x0
+mov	x26, x20
+mov	x20, #0x0
 str	x26, [x8, #0xa8]
 ldr	x22, [sp, #0x10]
@@ -221,5 +221,5 @@
 add	w13, w8, #0x1
 str	w13, [x23]
-ldp	w13, w20, [sp]
+ldp	w13, w21, [sp]
 str	w13, [x12, w8, uxtw #2]
 add	x8, x26, x11, lsl #3
@@ -232,26 +232,28 @@
 orr	w8, w9, w8
 str	w8, [x26, #0x3040]
-mov	w8, w20
-ldr	q0, [x26, #0x30a0]
-mov	w20, #0x1
-dup.2d	v1, x20
-mov.d	v1[0], x8
-add.2d	v0, v0, v1
+mov	w8, w21
+mov	w21, #0x1
+dup.2d	v0, x21
+mov.d	v0[0], x8
+ldr	q1, [x26, #0x30a0]
+add.2d	v0, v1, v0
 str	q0, [x26, #0x30a0]
+mov	w8, #0x30b8
+strb	w21, [x26, x8]
 ldrb	w8, [sp, #0x28]
 cmp	w8, #0x1
-b.ne	+0x3d8
+b.ne	+0x3e0
 ldr	x1, [sp, #0x18]
 add	x0, sp, #0x10
 bl	__ZNSt3__16__treeINS_12__value_typeIjjEENS_19__map_value_compareIjNS_4pairIKjjEENS_4lessIjEELb1EEENS_9allocatorIS6_EEE7destroyEPNS_11__tree_nodeIS2_PvEE
-cbz	x22, +0x3e4
+cbz	x22, +0x3ec
 mov	x0, x22
 bl	ADDR ; symbol stub for: __ZdaPv
-cbnz	x21, +0x438
-cbz	x19, +0x414
+cbnz	x20, +0x440
+cbz	x19, +0x41c
 add	x8, x19, #0x8
 mov	x9, #-0x1
 ldaddal	x9, x8, [x8]
-cbnz	x8, +0x414
+cbnz	x8, +0x41c
 ldr	x8, [x19]
 ldr	x8, [x8, #0x10]
@@ -260,5 +262,5 @@
 mov	x0, x19
 bl	ADDR ; symbol stub for: __ZNSt3__119__shared_weak_count14__release_weakEv
-mov	x0, x20
+mov	x0, x21
 ldp	x29, x30, [sp, #0x80]
 ldp	x20, x19, [sp, #0x70]
@@ -269,25 +271,25 @@
 add	sp, sp, #0x90
 ret
-mov	x0, x21
+mov	x0, x20
 mov	w1, #0x0
 bl	__ZN20XOffsetDatastructure11XBufferCore17FreeTableRecycler7recycleEPNS0_9FreeTableEb
-cbnz	x19, +0x3ec
-b	+0x414
-b	+0x45c
-b	+0x45c
+cbnz	x19, +0x3f4
+b	+0x41c
+b	+0x464
+b	+0x464
 mov	x24, #0x0
-b	+0x46c
+b	+0x474
 mov	x24, #0x0
-b	+0x468
-b	+0x468
-mov	x21, #0x0
+b	+0x470
+b	+0x470
+mov	x20, #0x0
 bl	ADDR ; symbol stub for: ___cxa_begin_catch
-cbz	x25, +0x47c
+cbz	x25, +0x484
 ldapur	x22, [x25, #0x8]
-cbz	x22, +0x484
+cbz	x22, +0x48c
 mov	w8, #0x1
 strb	w8, [x22, #0xb0]
 bl	ADDR ; symbol stub for: ___cxa_end_catch
-mov	w20, #0x0
+mov	w21, #0x0
 b	+0x194
 bl	___clang_call_terminate
```

## void XOffsetDatastructure::detail::VerifiedPathsAccess::save_compact_into<slg::xm::ViewSnapshot>(XOffsetDatastructure::XBufferCore&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1:
```diff
--- anchor
+++ head
@@ -119,5 +119,5 @@
 mov	x9, #0x0
 adrp x10, PAGE
-ldr	q0, [x10, #0xdd0]
+ldr	q0, [x10, #0x80]
 str	q0, [sp]
 str	w8, [sp, #0x10]
```

## void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xm::BattleMsg, slg::xm::Army, slg::xm::Army>(XOffsetDatastructure::XBufferCore&, std::__1::basic_string<char, std::__1::
```diff
--- anchor
+++ head
@@ -131,5 +131,5 @@
 ldr	w8, [sp, #0x3c]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd70]
+ldr	q0, [x9, #0xfc0]
 str	q0, [sp, #0x40]
 str	w8, [sp, #0x50]
```

## void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xmr::BattleMsg, slg::xmr::Army, slg::xmr::Army>(XOffsetDatastructure::XBufferCore&, std::__1::basic_string<char, std::__
```diff
--- anchor
+++ head
@@ -131,5 +131,5 @@
 ldr	w8, [sp, #0x3c]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd70]
+ldr	q0, [x9, #0xfc0]
 str	q0, [sp, #0x40]
 str	w8, [sp, #0x50]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::ShardState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::PlayerPatch>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::PlayerState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::ViewSnapshot>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::Report>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::BattleMsg>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xm::ViewDelta>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::ShardState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::PlayerState>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::Report>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::reload_into<slg::xmr::BattleMsg>(std::__1::span<std::byte const, 18446744073709551615ul>, std::__1::pmr::memory_resource*)
```diff
--- anchor
+++ head
@@ -104,5 +104,5 @@
 ldr	x8, [x19, #0x28]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd90]
+ldr	q0, [x9, #0xfe0]
 str	q0, [x8]
 stp	x23, x21, [x8, #0x10]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::ShardState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOff
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xcf0]
+ldr	q0, [x9, #0xf40]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::PlayerPatch>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOf
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd40]
+ldr	q0, [x9, #0xf90]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::PlayerState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOf
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xcd0]
+ldr	q0, [x9, #0xf20]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::ViewSnapshot>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XO
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd20]
+ldr	q0, [x9, #0xf70]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::Report>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOffsetD
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd10]
+ldr	q0, [x9, #0xf60]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::BattleMsg>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOffs
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd00]
+ldr	q0, [x9, #0xf50]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xm::ViewDelta>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOffs
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd30]
+ldr	q0, [x9, #0xf80]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::ShardState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOf
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xcf0]
+ldr	q0, [x9, #0xf40]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::PlayerState>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XO
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xcd0]
+ldr	q0, [x9, #0xf20]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::Report>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOffset
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd10]
+ldr	q0, [x9, #0xf60]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::XBuffer::finish_verified_load<slg::xmr::BattleMsg>(XOffsetDatastructure::XWireHeaderV2 const&, char const*, unsigned long, std::__1::pmr::memory_resource*, char const*, XOff
```diff
--- anchor
+++ head
@@ -52,5 +52,5 @@
 str	x8, [x19, #0xb8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd00]
+ldr	q0, [x9, #0xf50]
 stur	q0, [x19, #0x8]
 stp	x22, x8, [x19, #0xc8]
```

## void XOffsetDatastructure::detail::VerifiedPathsAccess::save_compact_into<slg::xm::ViewSnapshot>(XOffsetDatastructure::XBufferCore&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1:
```diff
--- anchor
+++ head
@@ -89,10 +89,10 @@
 add	w9, w8, #0x30
 adrp x10, PAGE
-ldr	d0, [x10, #0xe40]
+ldr	d0, [x10, #0x100]
 str	d0, [x19]
 mov	w10, #0x30
 stp	w9, w10, [x19, #0x8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xdc0]
+ldr	q0, [x9, #0x70]
 str	q0, [x19, #0x10]
 mov	x9, #0x5a96
```

## void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xm::BattleMsg, slg::xm::Army, slg::xm::Army>(XOffsetDatastructure::XBufferCore&, std::__1::basic_string<char, std::__1::
```diff
--- anchor
+++ head
@@ -91,10 +91,10 @@
 add	w9, w8, #0x30
 adrp x10, PAGE
-ldr	d0, [x10, #0xe40]
+ldr	d0, [x10, #0x100]
 str	d0, [x19]
 mov	w10, #0x30
 stp	w9, w10, [x19, #0x8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd80]
+ldr	q0, [x9, #0xfd0]
 str	q0, [x19, #0x10]
 mov	x9, #0xbb49
```

## void XOffsetDatastructure::detail::VerifiedPathsAccess::save_composed_into<slg::xmr::BattleMsg, slg::xmr::Army, slg::xmr::Army>(XOffsetDatastructure::XBufferCore&, std::__1::basic_string<char, std::__
```diff
--- anchor
+++ head
@@ -91,10 +91,10 @@
 add	w9, w8, #0x30
 adrp x10, PAGE
-ldr	d0, [x10, #0xe40]
+ldr	d0, [x10, #0x100]
 str	d0, [x19]
 mov	w10, #0x30
 stp	w9, w10, [x19, #0x8]
 adrp x9, PAGE
-ldr	q0, [x9, #0xd80]
+ldr	q0, [x9, #0xfd0]
 str	q0, [x19, #0x10]
 mov	x9, #0xbb49
```

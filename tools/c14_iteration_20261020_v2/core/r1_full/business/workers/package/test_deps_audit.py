"""Synthetic parser regressions only. Author has not executed this module."""
import unittest
from deps_audit import audit_default,sections

OWN=('xoffsetdatastructure.hpp','xoffset_verified_paths.hpp',
 'xoffset_validation_scratch.hpp','validation_scratch.hpp',
 'xoffset_snapshot_index.hpp','xoffset_snapshot_index_budget.hpp',
 'xoffset_core_lifetime.hpp','receive_raw_fwd.hpp','receive_raw.hpp',
 'receive_future.hpp','xoffset_verified_receive.hpp')
DEFAULT=tuple(n for n in OWN if n not in ('validation_scratch.hpp','xoffset_validation_scratch.hpp'))

def sample(names=DEFAULT,state='VALID',object_name='CMakeFiles/test.dir/test.cpp.o'):
    deps=['/clean/'+n for n in names]
    return object_name+': #deps '+str(len(deps))+', deps mtime 123 ('+state+')\n'+''.join('    '+n+'\n' for n in deps)+'\n'

class ParserTests(unittest.TestCase):
    def test_actual_nine_are_enough_but_not_eleven_claim(self):
        result=audit_default(sample(),'/clean',OWN)
        self.assertEqual(result['required_default_count'],9)
        self.assertEqual(set(result['not_observed_own_headers']),{'validation_scratch.hpp','xoffset_validation_scratch.hpp'})
    def test_eleven_observed_allowed_without_changing_required_nine(self):
        result=audit_default(sample(OWN),'/clean',OWN)
        self.assertEqual(result['required_default_count'],9)
        self.assertEqual(result['not_observed_own_headers'],[])
    def test_missing_required_header_rejected(self):
        with self.assertRaisesRegex(ValueError,'missing default'):audit_default(sample(DEFAULT[1:]),'/clean',OWN)
    def test_stale_section_rejected(self):
        with self.assertRaisesRegex(ValueError,'stale'):sections(sample(state='STALE'))
    def test_duplicate_object_rejected(self):
        with self.assertRaisesRegex(ValueError,'duplicate Ninja object'):sections(sample()+sample())
    def test_declared_count_must_match(self):
        with self.assertRaisesRegex(ValueError,'count differs'):sections(sample().replace('#deps 9,','#deps 10,'))
    def test_repeated_dependency_is_counted_raw(self):
        result=sections(sample((DEFAULT[0],DEFAULT[0])))
        entries=result['CMakeFiles/test.dir/test.cpp.o']
        self.assertEqual(len(entries),2)
        self.assertEqual(len(set(entries)),1)
    def test_real_ninja_schema_946_raw_945_unique(self):
        # The actual test_xstring_direct_assign object lists main twice.
        # Keep that schema, without importing/reading the original project.
        names=(*DEFAULT,*(f'external/dependency_{i}.hpp' for i in range(936)),DEFAULT[0])
        text=sample(names,object_name='tests/CMakeFiles/test_xstring_direct_assign.dir/test_xstring_direct_assign.cpp.o')
        objects=sections(text);entries=next(iter(objects.values()))
        self.assertEqual(len(entries),946)
        self.assertEqual(len(set(entries)),945)
        self.assertEqual(entries.count('/clean/xoffsetdatastructure.hpp'),2)
        self.assertEqual(audit_default(text,'/clean',OWN)['required_default_count'],9)
    def test_original_source_rejected(self):
        with self.assertRaisesRegex(ValueError,'original source'):audit_default(sample(),'/clean',OWN,('/clean',))
    def test_orphan_or_empty_rejected(self):
        for text in ('','    /clean/a.hpp\n'):
            with self.assertRaises(ValueError):sections(text)
    def test_nonabsolute_dependency_rejected(self):
        with self.assertRaisesRegex(ValueError,'nonabsolute'):sections(sample().replace('/clean/','relative/'))

if __name__=='__main__':unittest.main()

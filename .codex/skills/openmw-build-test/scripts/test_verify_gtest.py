import tempfile
import unittest
from pathlib import Path

from verify_gtest import inventory, matches, verify


class VerificationTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.list = self.root / 'list.txt'
        self.xml = self.root / 'result.xml'
        self.list.write_text('Running main()\nNative.\n  Check\nNativeParams/Rule. # TypeParam = X\n  Works/0 # GetParam() = 0\n')

    def write(self, cases, count=None):
        self.xml.write_text(f'<testsuites tests="{len(cases) if count is None else count}">'
                            + ''.join(cases) + '</testsuites>')

    def case(self, suite='Native', name='Check', status='run', result='completed', child=''):
        return f'<testcase classname="{suite}" name="{name}" status="{status}" result="{result}">{child}</testcase>'

    def test_exact_parameterized_inventory(self):
        self.write([self.case(), self.case('NativeParams/Rule', 'Works/0')])
        self.assertEqual(verify(self.list, self.xml)['executed'], 2)

    def test_missing_unexpected_and_duplicate(self):
        for cases in ([self.case()], [self.case(), self.case()], [self.case('Wrong')]):
            with self.subTest(cases=cases):
                self.write(cases)
                with self.assertRaises(ValueError): verify(self.list, self.xml)

    def test_failed_skipped_and_not_run(self):
        for case in (self.case(child='<failure/>'), self.case(child='<skipped/>'),
                     self.case(status='notrun', result='suppressed'), self.case(result='skipped')):
            with self.subTest(case=case):
                self.write([case])
                with self.assertRaises(ValueError): verify(self.list, self.xml, 'Native.*')

    def test_empty_selection_and_bad_aggregate(self):
        self.write([self.case()], count=5)
        with self.assertRaises(ValueError): verify(self.list, self.xml, 'Native.*')
        with self.assertRaises(ValueError): verify(self.list, self.xml, 'Absent.*')

    def test_filter_uses_only_gtest_wildcards(self):
        self.assertTrue(matches('Native.Check', 'Native*:Other.*-*.Skip'))
        self.assertFalse(matches('Native.Skip', 'Native*:Other.*-*.Skip'))
        self.assertTrue(matches('Native.Check', '-*.Skip'))
        self.assertFalse(matches('Native.Check', 'Native.[C]heck'))
        self.assertTrue(matches('Native.Check', 'Native.?heck'))

    def test_duplicate_or_empty_list(self):
        for text in ('', 'Native.\n  Check\n  Check\n'):
            with self.assertRaises(ValueError): inventory(text)

    def test_nonfailure_properties_allowed(self):
        self.write([self.case(child='<properties><property name="seed" value="1"/></properties>')])
        self.assertEqual(verify(self.list, self.xml, 'Native.*')['executed'], 1)


if __name__ == '__main__':
    unittest.main()

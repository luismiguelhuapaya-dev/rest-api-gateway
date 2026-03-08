"""
Test runner that discovers and executes all unit tests in the tests/unit/ directory.

Usage:
    python tests/unit/run_unit_tests.py
"""

import sys
import os
import unittest


def main():
    # Ensure the tests/unit directory is on the path
    test_dir = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, test_dir)

    # Discover all test modules in the directory
    loader = unittest.TestLoader()
    suite = loader.discover(start_dir=test_dir, pattern='test_*.py')

    # Run with verbosity
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Print summary
    print("\n" + "=" * 70)
    print(f"Tests run: {result.testsRun}")
    print(f"Failures:  {len(result.failures)}")
    print(f"Errors:    {len(result.errors)}")
    print(f"Skipped:   {len(result.skipped)}")
    print("=" * 70)

    if result.wasSuccessful():
        print("OVERALL RESULT: ALL TESTS PASSED")
    else:
        print("OVERALL RESULT: SOME TESTS FAILED")

    # Exit with non-zero code if tests failed
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Run Python UI integration tests against `tests/ui/apps/*` binaries (built with FLUX_BUILD_UI_TESTS=ON)."""

from __future__ import annotations

import argparse
import os
import sys
import unittest


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--build-dir",
        default=os.environ.get("FLUX_TEST_BUILD_DIR", ""),
        help="CMake binary directory (contains tests/ui/test_*). "
        "Default: env FLUX_TEST_BUILD_DIR.",
    )
    args = p.parse_args()
    if not args.build_dir:
        print("Pass --build-dir or set FLUX_TEST_BUILD_DIR", file=sys.stderr)
        sys.exit(2)

    root = os.path.dirname(os.path.abspath(__file__))
    os.environ["FLUX_TEST_BUILD_DIR"] = os.path.abspath(args.build_dir)
    sys.path.insert(0, root)

    loader = unittest.TestLoader()
    suite = loader.discover(os.path.join(root, "suites"), pattern="test_*.py")
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
    main()

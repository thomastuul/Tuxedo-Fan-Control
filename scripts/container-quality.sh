#!/bin/sh

set -eu

BUILD_DIR=/tmp/tuxedo-fan-control-quality-build
MARKDOWN_FILES="AGENTS.md README.md TECHNICAL_ANALYSIS.md doc/README.md"

rm -rf "$BUILD_DIR"

echo "==> clang-format"
clang-format --dry-run --Werror \
  Tuxedo-Fan-Control.cpp fan_curve.cpp fan_curve.h tests/fan_curve_test.cpp

echo "==> CMake build"
cmake -S . -B "$BUILD_DIR" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

echo "==> clang-tidy"
clang-tidy -p "$BUILD_DIR" \
  Tuxedo-Fan-Control.cpp fan_curve.cpp tests/fan_curve_test.cpp --quiet

echo "==> CTest"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "==> Markdown formatter"
prettier --check $MARKDOWN_FILES

echo "==> Markdown linter"
markdownlint-cli2 $MARKDOWN_FILES

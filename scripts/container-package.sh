#!/bin/sh

set -eu

: "${PACKAGE_VERSION:?PACKAGE_VERSION must be set}"

case "$PACKAGE_VERSION" in
  [0-9]* ) ;;
  * )
    echo "Invalid Debian package version: $PACKAGE_VERSION" >&2
    exit 2
    ;;
esac

BUILD_DIR=/tmp/tuxedo-fan-control-package-build
PACKAGE_DIR=$BUILD_DIR/packages
OUTPUT_DIR=/output

if [ ! -d "$OUTPUT_DIR" ] || [ ! -w "$OUTPUT_DIR" ]; then
  echo "$OUTPUT_DIR must be a writable mounted directory" >&2
  exit 2
fi

rm -rf "$BUILD_DIR"

echo "==> Configure package build"
cmake -S /workspace -B "$BUILD_DIR" -G Ninja \
  -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DSYSTEMD_UNIT_DIR=lib/systemd/system \
  -DTUXEDO_FAN_CONTROL_VERSION="$PACKAGE_VERSION"

echo "==> Build"
cmake --build "$BUILD_DIR"

echo "==> CTest"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "==> Build Debian package"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -B "$PACKAGE_DIR"

found_package=false
for built_package in "$PACKAGE_DIR"/*.deb; do
  if [ ! -f "$built_package" ]; then
    continue
  fi

  found_package=true
  package_name=${built_package##*/}
  package_file=$OUTPUT_DIR/$package_name
  cp "$built_package" "$package_file"
  dpkg-deb --info "$package_file"
  dpkg-deb --contents "$package_file"
  (
    cd "$OUTPUT_DIR"
    sha256sum "$package_name" >"$package_name.sha256"
  )
done

if [ "$found_package" = false ]; then
  echo "CPack did not create a Debian package" >&2
  exit 1
fi

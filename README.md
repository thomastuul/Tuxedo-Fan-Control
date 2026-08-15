# Tuxedo-Fan-Control

[![Debian package](https://github.com/thomastuul/Tuxedo-Fan-Control/actions/workflows/debian-package.yml/badge.svg)](https://github.com/thomastuul/Tuxedo-Fan-Control/actions/workflows/debian-package.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-11-blue.svg)](CMakeLists.txt)

Automatic fan-control daemon for compatible TUXEDO/Clevo Linux laptops.

The daemon reads the laptop embedded-controller temperature and applies a
selectable TUXEDO-style fan profile to the EC fan-control channel. It is meant
for hardware where the required TUXEDO kernel driver stack exposes the EC/fan
interface, installs as a systemd service, and can be distributed as a Debian
`.deb` package.

> **Hardware-specific software:** this is not a generic laptop fan controller.
> Use it only on compatible TUXEDO/Clevo systems, verify driver support first,
> and monitor temperatures after installation.

## Features

- Automatic EC-based fan control for compatible TUXEDO/Clevo laptops
- TUXEDO Control Center profile names: `silent`, `quiet`, `balanced`, `cool`,
  and `freezy`
- systemd service integration with configurable profile selection
- CMake build with CTest unit tests that do not access laptop EC hardware
- Docker-based quality checks for C++, CMake, Markdown, formatting, and tests
- Debian package build with GitHub Actions release artifacts
- Administrator manpage installed as `Tuxedo-Fan-Control(8)`

## Compatibility and safety

Runtime EC and fan access requires the TUXEDO kernel driver stack for the target
laptop. On current systems this is provided by `tuxedo-drivers`; older
installations may use the archived `tuxedo-keyboard` project. These packages are
not part of the default Debian or Ubuntu repositories in a standard installation.
Add the distribution-specific TUXEDO package repository and follow TUXEDO's
driver installation instructions before enabling this service.

Verify that the required modules are installed and loaded, for example
`tuxedo_keyboard` and, where applicable, `tuxedo_io`:

```sh
lsmod | grep -E 'tuxedo_keyboard|tuxedo_io|tuxedo'
```

If Secure Boot blocks unsigned DKMS modules, the EC/fan interface can be
unavailable even though the driver package is installed. Enroll the module
signing key or disable Secure Boot according to the distribution's TUXEDO driver
instructions before starting this fan-control service.

The program opens `/dev/tuxedo_io` as root and uses the Clevo IOCTL interface;
only the signed TUXEDO kernel modules access ACPI/WMI and the embedded
controller. The daemon does not request raw-I/O capabilities and remains usable
with Secure Boot kernel lockdown. Incorrect use on unsupported hardware may
cause ineffective cooling. Check service logs and temperatures after
installation.

## Quick start

### Install Debian package

Download the current `.deb` package and its SHA-256 checksum from
[GitHub Releases](https://github.com/thomastuul/Tuxedo-Fan-Control/releases).

Install the package from the download directory:

```sh
sudo apt install ./tuxedo-fan-control_*.deb
```

Select a fan profile through `/etc/default/tuxedo-fan-control`:

```sh
echo 'TUXEDO_FAN_PROFILE=quiet' |
  sudo tee /etc/default/tuxedo-fan-control
```

Enable and start the service:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now Tuxedo-Fan-Control.service
```

Check the installed version, service status, and selected profile:

```sh
dpkg-query -W tuxedo-fan-control
systemctl status Tuxedo-Fan-Control.service
journalctl -u Tuxedo-Fan-Control.service -n 20 --no-pager
```

The journal should contain a message such as `Using fan profile: quiet`.

### Build and test from source

```sh
sudo apt install -y g++ make cmake
make configure
make compile
ctest --test-dir build --output-on-failure
```

Only install from source after the tests have passed:

```sh
sudo make all
```

The Makefile is a compatibility wrapper around CMake. It configures and builds
the project, installs `Tuxedo-Fan-Control` in `/usr/local/bin`, installs and
enables `Tuxedo-Fan-Control.service`, and starts the service. The final command
is a system installation.

For direct CMake usage:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build --component runtime
sudo cmake --install build --component service
sudo systemctl enable Tuxedo-Fan-Control.service
```

CMake generates the unit with an `ExecStart` path matching the selected
installation prefix. The unit runs as root without Linux capabilities, grants
access only to `/dev/tuxedo_io`, and applies filesystem protection, restart
backoff, and start-rate limits.

## Fan profiles

The daemon provides the TUXEDO Control Center profiles `silent`, `quiet`,
`balanced`, `cool`, and `freezy`. Because the EC interface currently supplies
one local temperature only, the implementation uses the documented CPU curve for
each profile. `balanced` is the default.

![Legacy fan curve and current TCC CPU profiles](doc/fan-curve.png)

The profile can be passed directly:

```sh
Tuxedo-Fan-Control --profile quiet
```

The systemd unit reads `/etc/default/tuxedo-fan-control`. To select a profile,
create that file with, for example:

```sh
TUXEDO_FAN_PROFILE=quiet
```

If the file is absent or its value is invalid, the daemon logs a warning and
uses `balanced`. Changing the active profile requires an intentional service
restart. The percentage is converted with rounding to the EC range 0–255. The
complete tables, their thermal implications, and the EC conversion are
documented in [TECHNICAL_ANALYSIS.md](TECHNICAL_ANALYSIS.md).

## Failure handling

Kernel-interface failures are reported explicitly. The daemon skips fan control
when a temperature read fails or the returned temperature is implausible. It
exits with an error after three consecutive failed control cycles, and the
supplied systemd unit then applies its rate-limited `Restart=on-failure` policy.

During a controlled shutdown or repeated implausible-temperature failure it
attempts to set the fan to the maximum profile value before exiting. Because the
available hardware documentation does not guarantee a model-independent
emergency recovery operation, kernel-interface failures still stop the daemon
without issuing a speculative recovery write.

## Testing and quality checks

The CTest suite checks device and hardware detection, IOCTL temperature
decoding, fan-speed boundary values, preservation of other fan channels,
failure propagation, and the consecutive-failure policy without accessing
laptop hardware. The tests must pass before an installation or release build.

The project provides a Debian-based Docker image containing the C++ and Markdown
quality tools. Build the image and run all checks with:

```sh
docker build -t tuxedo-fan-control-quality:bookworm docker/quality
docker run --rm --network none \
  -v "$PWD:/workspace" \
  -w /workspace \
  tuxedo-fan-control-quality:bookworm
```

The container runs `clang-format`, `clang-tidy`, CMake/CTest, Prettier and
markdownlint. It does not access the laptop EC and does not install the software
on the host.

## Debian package

The Debian package is built in a dedicated Debian Bookworm container. Unit tests
run inside the container before CPack is allowed to create the package. For a
local package build, use:

```sh
docker build -t tuxedo-fan-control-package docker/package
mkdir -p package-output
docker run --rm --network none \
  --user "$(id -u):$(id -g)" \
  -e PACKAGE_VERSION=0.1.0 \
  -v "$PWD:/workspace:ro" \
  -v "$PWD/package-output:/output" \
  tuxedo-fan-control-package
```

The generated `.deb` file and its SHA-256 checksum are written to
`package-output/`. The package installs the executable below `/usr/bin`, the
systemd unit below `/usr/lib/systemd/system`, the administrator manpage and the
project documentation. Installing the package does not automatically enable or
start the hardware-specific service. The package declares `systemd` and generated
shared-library dependencies such as `libc6`, `libgcc-s1`, and `libstdc++6`; it
does not install the hardware-specific TUXEDO kernel drivers.

The GitHub Actions workflow `Debian package` performs the same containerized
build for pull requests and pushes to `master`, for tags beginning with `v`, and
when started manually. Commit builds receive a version of the form
`0.0.0+git<commit>.<run>`. A tag such as `v0.1.0` produces package version
`0.1.0`; a manual run can supply its own Debian-compatible version. Every
workflow run publishes the package and checksum as downloadable artifacts. Tag
builds additionally create a GitHub Release and attach the `.deb` package and
its SHA-256 checksum as permanent release assets.

The CMake install step does not start or restart the service automatically.

The installation also provides the administrator manpage:

```sh
man 8 Tuxedo-Fan-Control
```

It is installed as `Tuxedo-Fan-Control.8` below
`${CMAKE_INSTALL_PREFIX}/share/man/man8`.

Check the service with:

```sh
systemctl status -n20 Tuxedo-Fan-Control.service
```

For verbose logging, use:

```sh
sudo make VERBOSE=ON all
```

To remove the installation:

```sh
sudo make uninstall
```

## Original author and source

This project is based on the original **Clevo-N151ZU-fan-controller** by
**François Kneib**.

Original source repository:

<https://gitlab.com/francois.kneib/clevo-N151ZU-fan-controller>

Please preserve this attribution when redistributing or modifying the software.

## License

This project is licensed under the GNU General Public License v3 or any later
version. See [LICENSE](LICENSE).

# Tuxedo-Fan-Control

Automatic fan control for compatible Tuxedo/Clevo laptops. The controller reads
the embedded-controller temperature and applies the discrete TUXEDO fan curve
to the EC fan-control channel.

## Fan curve

The current implementation uses the original TUXEDO table: below 44 °C it
requests 1 %, then increases the duty cycle in discrete one-degree steps until
100 % at 91 °C. The percentage is converted to the EC range 1–255 with
rounding. The curve is applied immediately; the daemon does not use the
former linear 70/90 °C curve or a peak-value hold time.

![Current TUXEDO fan curve](doc/fan-curve.png)

The graph shows the requested duty cycle, not a calibrated physical fan speed
in revolutions per minute. The complete table and the EC conversion are
documented in [TECHNICAL_ANALYSIS.md](TECHNICAL_ANALYSIS.md).

## Prerequisites

```sh
sudo apt install -y g++ make
```

The program accesses the embedded-controller I/O ports and therefore must run
as root.

## Build and install

```sh
make configure
make compile
ctest --test-dir build --output-on-failure
sudo make all
```

The Makefile is a compatibility wrapper around CMake. It configures and builds
the project, installs `Tuxedo-Fan-Control` in `/usr/local/bin`, installs and
enables `Tuxedo-Fan-Control.service`, and starts the service. The final command
is a system installation and must only be run after the tests have passed.

For direct CMake usage:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build --component runtime
sudo cmake --install build --component service
sudo systemctl enable Tuxedo-Fan-Control.service
```

The CTest suite checks the temperature and fan-speed boundary values without
accessing the laptop's EC. The tests must pass before an installation or
release build.

## Containerized quality checks

The project provides a Debian-based Docker image containing the C++ and
Markdown quality tools. Build the image and run all checks with:

```sh
docker build -t tuxedo-fan-control-quality:bookworm docker/quality
docker run --rm --network none \
  -v "$PWD:/workspace" \
  -w /workspace \
  tuxedo-fan-control-quality:bookworm
```

The container runs `clang-format`, `clang-tidy`, CMake/CTest, Prettier and
markdownlint. It does not access the laptop EC and does not install the
software on the host.

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

Please preserve this attribution when redistributing or modifying the
software.

## License

This project is licensed under the GNU General Public License v3 or any later
version. See [LICENSE](LICENSE).

# Tuxedo-Fan-Control

Automatic fan control for compatible Tuxedo/Clevo laptops. The controller reads
the embedded-controller temperature and adjusts the fan speed gradually.

## Prerequisites

```sh
sudo apt install -y g++ make
```

The program accesses the embedded-controller I/O ports and therefore must run
as root.

## Build and install

```sh
sudo make all
```

The Makefile is a compatibility wrapper around CMake. It configures and builds
the project, installs `Tuxedo-Fan-Control` in `/usr/local/bin`, installs and
enables `Tuxedo-Fan-Control.service`, and starts the service.

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

The CMake install step does not start or restart the service automatically.

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

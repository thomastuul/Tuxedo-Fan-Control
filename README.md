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

This builds `Tuxedo-Fan-Control`, installs it in `/usr/local/bin`, installs and
enables `Tuxedo-Fan-Control.service`, and starts the service.

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

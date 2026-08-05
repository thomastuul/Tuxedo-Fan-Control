Tuxedo-Fan-Control
==================

See README.md for build, installation, and service instructions.

Fan profiles
------------

The implementation provides the silent, quiet, balanced, cool, and freezy
profiles. The systemd service reads TUXEDO_FAN_PROFILE from the optional file
/etc/default/tuxedo-fan-control and defaults to balanced. See README.md and
TECHNICAL_ANALYSIS.md for the complete tables and configuration details.

The administrator manpage is installed as Tuxedo-Fan-Control.8 and can be
read with:

man 8 Tuxedo-Fan-Control

Original author and source
--------------------------

This software is based on the original Clevo-N151ZU-fan-controller by
François Kneib:

https://gitlab.com/francois.kneib/clevo-N151ZU-fan-controller

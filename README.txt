Tuxedo-Fan-Control
==================

See README.md for build, installation, and service instructions.

Fan curve
---------

The implementation uses the discrete TUXEDO fan table. Below 44 °C it requests
1 %, then increases the duty cycle in one-degree steps to 100 % at 91 °C. The
percentage is converted to the EC range 1–255. This is a duty-cycle request,
not a calibrated RPM value. See README.md and TECHNICAL_ANALYSIS.md for the
graph and complete table.

The administrator manpage is installed as Tuxedo-Fan-Control.8 and can be
read with:

man 8 Tuxedo-Fan-Control

Original author and source
--------------------------

This software is based on the original Clevo-N151ZU-fan-controller by
François Kneib:

https://gitlab.com/francois.kneib/clevo-N151ZU-fan-controller

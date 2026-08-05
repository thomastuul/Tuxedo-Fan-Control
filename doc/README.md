# Technische Referenzen

Diese Dateien enthalten die wichtigsten öffentlich verfügbaren Unterlagen zur
EC- und Temperaturhardware des Clevo N151ZU. Die Dateien wurden am 4. August
2026 heruntergeladen.

## Software-Kennlinien

[`fan-curve.png`](fan-curve.png) vergleicht die früher im Projekt verwendete
Einzelkennlinie mit den fünf aktuellen CPU-Profilkurven. Deren vollständige
Tabellen und die Implementierungsdetails stehen in
[`TECHNICAL_ANALYSIS.md`](../TECHNICAL_ANALYSIS.md).

## Manpage

[`Tuxedo-Fan-Control.8`](Tuxedo-Fan-Control.8) beschreibt den daemon-only
Betrieb, den EC-Zugriff, die Kennlinie, die systemd-Unit und die erforderlichen
Rechte. CMake installiert die Manpage nach
`${CMAKE_INSTALL_PREFIX}/share/man/man8`.

## Prüfcontainer

Der Docker-Prüfcontainer liegt unter [`docker/quality`](../docker/quality).
Das Skript [`scripts/container-quality.sh`](../scripts/container-quality.sh)
führt `clang-format`, `clang-tidy`, CMake/CTest, Prettier und markdownlint
aus. Die Prüfungen laufen ohne EC-Zugriffe und ohne Installation auf dem Host.

Der Paketcontainer unter [`docker/package`](../docker/package) erzeugt mit
CPack ein Debian-Paket. Das Skript
[`scripts/container-package.sh`](../scripts/container-package.sh) führt vor
dem Paketbau zwingend die Unit-Tests aus und erzeugt zusätzlich eine
SHA-256-Prüfsumme. Derselbe Ablauf wird von der GitHub Action `Debian package`
verwendet.

## Clevo-Service-Manual

[`Clevo_N150ZU_N151ZU_N152ZU_Service_Manual.pdf`](Clevo_N150ZU_N151ZU_N152ZU_Service_Manual.pdf)

Quelle:

<https://www.e-weekly.co.uk/download/RnD/DRIVERS/CLEVO/N151ZU/ESM.pdf>

Besonders relevant sind die Schaltplanseiten zum **ITE IT8587 (8991)**, zur
`H_PECI`-Verbindung, zum `CPU_FAN`-Signal sowie zum analogen `THERM_VOLT`-
Temperatursensor. Der Sensor ist dort als 100-kΩ-NTC `EWTF02-104F4F-N`
aufgeführt.

## Intel-Prozessor-Datenblatt

[`Intel_8th_Gen_Core_Datasheet_Volume_1.pdf`](Intel_8th_Gen_Core_Datasheet_Volume_1.pdf)

Quelle:

<https://www.intel.co.id/content/dam/www/public/us/en/documents/datasheets/8th-gen-core-datasheet-vol-1.pdf>

Dieses Datenblatt beschreibt die digitalen Temperatursensoren (DTS), PECI und
die Verwendung der PECI-Temperatur für Plattform- und Lüftersteuerung.

## Referenzimplementierung der EC-Zugriffe

[`tuxedo-fan-control_ec_access.cc`](tuxedo-fan-control_ec_access.cc)

Quelle:

<https://github.com/tuxedocomputers/tuxedo-fan-control/blob/master/native/ec_access.cc>

Die Datei dokumentiert anhand von Quellcode die Verwendung der EC-Ports `0x66`
und `0x62` sowie des Clevo-Lüfterbefehls `0x99`. Das TUXEDO-Repository ist
archiviert; die Datei dient hier als historische Referenz.

## Einordnung

Die Unterlagen enthalten keine öffentlich zugängliche vollständige
ITE-EC-Firmware-Spezifikation. Insbesondere ist die konkrete Bedeutung von
`0x9E` mit Index `1` für das N151ZU nicht offiziell dokumentiert. Die
Zuordnung dieses Befehls zur vom EC bereitgestellten CPU-/PECI-Temperatur bleibt
daher eine begründete, aber nicht vollständig verifizierte Schlussfolgerung.

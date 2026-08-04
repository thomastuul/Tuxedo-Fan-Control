# Technische Analyse

## Überblick

`Tuxedo-Fan-Control` kommuniziert direkt mit dem Embedded Controller (EC) des
Laptops. Die Software verwendet weder `/sys/class/thermal` noch `hwmon`,
`lm-sensors` oder ein anderes Linux-Sensorinterface.

Die Kommunikation erfolgt über direkte x86-I/O-Portzugriffe. Der Dienst muss
deshalb als `root` laufen.

## Temperaturauslesung

In `Tuxedo-Fan-Control.cpp` sind folgende EC-Schnittstellen definiert:

| Funktion | Wert |
| --- | ---: |
| EC-Command-Port | `0x66` |
| EC-Daten-Port | `0x62` |
| Temperaturbefehl | `0x9E` |
| Temperaturindex | `1` |

Die Funktion `GetLocalTemp()` arbeitet folgendermaßen:

1. Mit `ioperm()` werden die beiden I/O-Ports für den Prozess freigeschaltet.
2. Der EC-Eingangspuffer wird geleert.
3. Der Befehl `0x9E` wird an den Command-Port gesendet.
4. Der Index `1` wird an den Daten-Port geschrieben.
5. Das Programm wartet auf ein verfügbares Datenbyte.
6. Ein Byte wird vom Daten-Port gelesen und zurückgegeben.

Der gelesene Wert wird unmittelbar als Temperatur in Grad Celsius verwendet.
Der Code geht also davon aus, dass der EC bei Befehl `0x9E` und Index `1` einen
Celsiuswert liefert.

Aus dem Quelltext lässt sich nicht sicher bestimmen, ob dieser EC-Wert die
CPU-Kerntemperatur, die CPU-Package-Temperatur oder einen anderen internen
Temperatursensor repräsentiert. Sicher ist nur, dass kein Linux-Temperatur-
interface abgefragt wird, sondern ein laptopspezifischer EC-Messwert.

## Lüfteransteuerung

Die Lüftersteuerung erfolgt ebenfalls direkt über den EC. Die Funktion
`setFanSpeed()` sendet den herstellerspezifischen Befehl `0x99`:

```text
Command-Port: 0x99
Datenwert:    0x01    (Lüfter-/Kanal-ID)
Datenwert:    speed   (Geschwindigkeit 0 bis 255)
```

Der Geschwindigkeitswert wird als Byte an den EC geschrieben:

- `0`: theoretisch aus
- `255`: maximale Geschwindigkeit
- `40`: im Programm definierter Mindestwert

Die genaue Bedeutung der EC-Befehle `0x99` und `0x9E` ist
laptop- und firmwareabhängig.

## Temperatur- und Lüfterkurve

Die relevanten Konstanten sind:

```cpp
FAN_MIN_VALUE   = 40
FAN_OFF_TEMP   = 70
FAN_MAX_TEMP   = 90
FAN_START_VALUE = 100
```

Die Regelung berechnet die Lüftergeschwindigkeit linear:

- unter `70 °C`: dynamischer Wert `0`
- bei `70 °C`: Startwert `100`
- zwischen `70 °C` und `90 °C`: lineare Steigerung
- ab `90 °C`: maximaler Wert `255`

Anschließend wird jedoch immer mindestens `FAN_MIN_VALUE` geschrieben:

```cpp
setFanSpeed(max(FAN_MIN_VALUE, slidingMaxFanSpeed));
```

Der Lüfter wird bei niedriger Temperatur deshalb nicht zwingend vollständig
abgeschaltet, sondern mindestens mit EC-Wert `40` angesteuert. Der Kommentar,
dass der Lüfter unter `70 °C` aus sei, beschreibt das tatsächliche Verhalten
nicht vollständig.

## Regelzyklus und Spitzenwert-Haltezeit

Die Regelung läuft alle 250 ms. Ein höherer erforderlicher Lüfterwert wird
sofort übernommen. Nach einer Erhöhung bleibt der erkannte Spitzenwert jedoch
mindestens zehn Sekunden erhalten:

```cpp
FAN_PEAK_HOLD_TIME = 10000; // Millisekunden
```

Damit soll verhindert werden, dass der Lüfter bei kurzen
Temperaturschwankungen ständig hoch- und herunterregelt. Zusätzlich wird der
aktuelle Wert spätestens alle zwei Sekunden erneut an den EC gesendet.

## Berechtigungen und Betriebsrisiken

Der systemd-Dienst läuft als `root`, weil `ioperm()`, `inb()` und `outb()` für
direkte I/O-Portzugriffe privilegierte Rechte benötigen.

Die EC-Funktionen prüfen ihre Rückgabewerte nur unvollständig. Außerdem gibt es
keine modellunabhängige Validierung des gelesenen Temperaturwerts und keinen
separaten Notfallmechanismus in der Software. Die Regelung ist daher von der
EC-Implementierung und Firmware des jeweiligen Laptops abhängig. Eine falsche
Port- oder Befehlsbelegung kann zu fehlerhaften Temperaturwerten oder einer
ungeeigneten Lüfteransteuerung führen.

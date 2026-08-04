# Technische Analyse

## Überblick

`Tuxedo-Fan-Control` kommuniziert direkt mit dem Embedded Controller (EC) des
Laptops. Die Software verwendet weder `/sys/class/thermal` noch `hwmon`,
`lm-sensors` oder ein anderes Linux-Sensorinterface.

Die Kommunikation erfolgt über direkte x86-I/O-Portzugriffe. Der Dienst muss
deshalb als `root` laufen.

## Temperaturauslesung

In `Tuxedo-Fan-Control.cpp` sind folgende EC-Schnittstellen definiert:

| Funktion         |   Wert |
| ---------------- | -----: |
| EC-Command-Port  | `0x66` |
| EC-Daten-Port    | `0x62` |
| Temperaturbefehl | `0x9E` |
| Temperaturindex  |    `1` |

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

Die Software verwendet die aus dem
[offiziellen TUXEDO-Fan-Control-Projekt](https://github.com/tuxedocomputers/tuxedo-fan-control)
übernommene Kennlinie. Sie besteht aus diskreten Einträgen je Grad Celsius.
Unter `44 °C`
werden `1 %`, ab `101 °C` werden `100 %` verwendet. Die Prozentwerte werden
mit Rundung auf den EC-Bereich `1` bis `255` abgebildet.

Die Kennlinienlogik ist für CPU- und GPU-Werte identisch. Der aktuelle Daemon
schreibt den Wert jedoch ausschließlich für EC-Kanal `0x01`; die Kurvenlogik
ist damit vorbereitet, aber derzeit nicht auf weitere Kanäle verdrahtet.

Die Darstellung der aktuell implementierten Kennlinie befindet sich in
[`doc/fan-curve.png`](doc/fan-curve.png).

```text
44–46 °C: 10 %     47–49 °C: 12 %     50–52 °C: 15 %
53–55 °C: 17 %     56–58 °C: 19 %     59 °C: 22 %
60 °C: 23 %         61 °C: 24 %         62 °C: 25 %
63 °C: 27 %         64 °C: 29 %         65–66 °C: 35 %
67–68 °C: 37 %      69–70 °C: 42 %      71–73 °C: 45 %
74–75 °C: 50 %      76–77 °C: 55 %      78–79 °C: 60 %
80–81 °C: 70 %      82 °C: 75 %         83–84 °C: 80 %
85–87 °C: 85 %      88–90 °C: 90 %      91–100 °C: 100 %
```

Die frühere lineare Kennlinie mit den Schwellen `70 °C` und `90 °C` wird nicht
mehr verwendet. Die Regelung folgt dem Tabellenwert unmittelbar; ein
zehnsekündiges Halten eines zuvor erreichten Spitzenwerts findet nicht mehr
statt.

## Regelzyklus

Die Regelung liest die Temperatur alle 250 ms und folgt dem zugehörigen
Tabellenwert unmittelbar. Der aktuelle EC-Wert wird bei einer Änderung sowie
spätestens alle zwei Sekunden erneut an den EC gesendet.

## Berechtigungen und Betriebsrisiken

Der systemd-Dienst läuft als `root`, weil `ioperm()`, `inb()` und `outb()` für
direkte I/O-Portzugriffe privilegierte Rechte benötigen.

Die EC-Funktionen prüfen ihre Rückgabewerte nur unvollständig. Außerdem gibt es
keine modellunabhängige Validierung des gelesenen Temperaturwerts und keinen
separaten Notfallmechanismus in der Software. Die Regelung ist daher von der
EC-Implementierung und Firmware des jeweiligen Laptops abhängig. Eine falsche
Port- oder Befehlsbelegung kann zu fehlerhaften Temperaturwerten oder einer
ungeeigneten Lüfteransteuerung führen.

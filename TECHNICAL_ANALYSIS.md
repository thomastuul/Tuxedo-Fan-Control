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

Die Funktion `getLocalTemp()` arbeitet folgendermaßen:

1. Mit `ioperm()` werden die beiden I/O-Ports für den Prozess freigeschaltet.
2. Der EC-Eingangspuffer wird geleert.
3. Der Befehl `0x9E` wird an den Command-Port gesendet.
4. Der Index `1` wird an den Daten-Port geschrieben.
5. Das Programm wartet auf ein verfügbares Datenbyte.
6. Ein Byte wird vom Daten-Port gelesen und zurückgegeben.

Alle Wartevorgänge beim Leeren des Ausgabepuffers, beim Warten auf einen freien
Eingabepuffer und beim Warten auf ein Ausgabebyte sind durch einen zeitbasierten
Timeout von einer Sekunde begrenzt. Die monotone Uhr
`std::chrono::steady_clock` verhindert, dass Systemzeitänderungen die Frist
beeinflussen. Jeder Teilschritt liefert einen expliziten Status. Ein Timeout
beendet die laufende Transaktion; insbesondere wird nach einem Timeout kein
nachfolgendes Command- oder Datenbyte geschrieben. Das gelesene Byte wird
getrennt vom Status zurückgegeben, sodass der gültige EC-Wert `0` nicht mit
einem Timeout verwechselt wird.

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
- `0`: kleinster vom Programm verwendeter Wert; Profile dürfen den Lüfter damit
  unterhalb ihrer ersten aktiven Stufe abschalten

Die genaue Bedeutung der EC-Befehle `0x99` und `0x9E` ist
laptop- und firmwareabhängig.

## TUXEDO-Control-Center-Referenzkurven

Das TUXEDO Control Center (TCC) definiert fünf Standardprofile mit getrennten
CPU- und GPU-Kurven. Die folgenden Werte stammen aus
[`TccFanTable.ts`](https://github.com/tuxedocomputers/tuxedo-control-center/blob/ae003220232ea4f4591789fdb3167065c682bc08/src/common/models/TccFanTable.ts)
des TCC-Commits `ae003220` vom 28. Juli 2026. Die Quelldatei enthält für jedes
ganze Grad von `0 °C` bis `100 °C` einen Sollwert. Zur kompakteren Darstellung
sind hier nur Temperaturen aufgeführt, an denen sich der Wert ändert.

Die Prozentwerte sind normalisierte TCC-Sollwerte und keine kalibrierten
Drehzahlen in Umdrehungen pro Minute. Die tatsächliche Drehzahl hängt von
Laptopmodell, EC, Firmware und Lüfter ab. Diese Referenztabellen ersetzen die
zuvor dokumentierte ältere Einzelkurve und werden von der C++-Implementierung
als auswählbare Profile verwendet. Da der Daemon nur den oben beschriebenen
einzelnen lokalen EC-Temperaturwert liest, wird jeweils die CPU-Kurve
angewendet. Die GPU-Kurven bleiben als Referenz für eine spätere Erweiterung um
eine verlässlich identifizierte GPU-Temperatur.

### Silent

```text
CPU:  0 °C=0 %  61=20 %  66=25 %  70=30 %  72=35 %  74=40 %
     76=45 %    78=50 %  80=55 %  82=60 %  84=65 %  86=70 %
     88=75 %    89=80 %  90=85 %  91=90 %  93=95 %  95=100 %
GPU:  0 °C=0 %  60=20 %  62=22 %  64=23 %  65=24 %  66=25 %
     68=28 %    69=30 %  70=33 %  71=37 %  72=40 %  73=43 %
     74=44 %    75=46 %  76=48 %  77=52 %  79=55 %  81=60 %
     83=65 %    85=70 %  87=80 %  89=90 %  91=100 %
```

### Quiet

```text
CPU:  0 °C=0 %  51=20 %  61=22 %  64=23 %  65=24 %  66=25 %
     68=28 %    69=30 %  70=33 %  71=37 %  72=40 %  73=43 %
     74=44 %    75=46 %  76=48 %  77=52 %  79=55 %  81=60 %
     83=65 %    85=70 %  87=80 %  89=85 %  91=90 %  93=95 %
     95=100 %
GPU:  0 °C=0 %  51=20 %  61=25 %  65=30 %  69=35 %  72=40 %
     73=43 %    74=44 %  75=46 %  76=48 %  77=52 %  79=55 %
     81=60 %    83=65 %  85=70 %  87=80 %  89=90 %  91=100 %
```

### Balanced

```text
CPU:  0 °C=0 %  46=20 %  52=23 %  54=26 %  57=30 %  60=33 %
     63=35 %    65=38 %  66=40 %  67=42 %  68=45 %  69=47 %
     70=50 %    72=52 %  73=53 %  75=57 %  77=60 %  79=63 %
     80=65 %    82=70 %  84=75 %  86=80 %  88=85 %  89=90 %
     92=95 %    95=100 %
GPU:  0 °C=0 %  46=20 %  52=23 %  54=26 %  57=30 %  60=33 %
     63=35 %    65=38 %  66=40 %  67=42 %  68=45 %  69=47 %
     70=50 %    72=52 %  73=53 %  75=57 %  77=60 %  79=63 %
     80=65 %    82=70 %  84=75 %  86=80 %  88=85 %  89=90 %
     91=100 %
```

### Cool

```text
CPU:  0 °C=0 %  40=20 %  46=25 %  51=30 %  56=32 %  57=33 %
     58=34 %    59=35 %  61=37 %  62=40 %  64=42 %  65=45 %
     68=47 %    69=50 %  71=52 %  72=55 %  74=57 %  75=60 %
     77=65 %    79=70 %  81=75 %  83=80 %  85=85 %  87=90 %
     90=95 %    95=100 %
GPU:  0 °C=0 %  40=25 %  45=30 %  50=35 %  55=40 %  60=45 %
     65=50 %    70=60 %  75=70 %  80=75 %  85=85 %  87=90 %
     89=95 %    91=100 %
```

### Freezy

```text
CPU:  0 °C=20 %  30=25 %  40=30 %  46=35 %  50=40 %  56=45 %
     61=50 %     66=55 %  71=60 %  76=65 %  78=70 %  80=75 %
     82=80 %     84=85 %  86=90 %  90=95 %  95=100 %
GPU:  0 °C=25 %  36=30 %  41=35 %  46=40 %  51=45 %  56=50 %
     61=60 %     66=65 %  71=70 %  76=75 %  81=85 %  86=95 %
     91=100 %
```

### Regelungsverhalten des TCC

Der TCC-Lüfter-Worker läuft im Abstand von einer Sekunde. Die Regelung filtert
bis zu 13 Temperaturmessungen, indem sie die Werte sortiert und den gerundeten
Mittelwert aus bis zu sieben mittleren Werten bildet. Auf den Tabellenwert
werden anschließend Benutzer-Offset, konfiguriertes Minimum und Maximum sowie
die Mindestgeschwindigkeit der Hardware angewendet. Beim Absenken oberhalb von
20 % verringert TCC den Sollwert um höchstens zwei Prozentpunkte pro Zyklus.
Zusätzlich erzwingt TCC ab `80 °C` mindestens `30 %` und ab `90 °C` mindestens
`40 %`. Die Berechnung ist in
[`FanControlLogic.ts`](https://github.com/tuxedocomputers/tuxedo-control-center/blob/ae003220232ea4f4591789fdb3167065c682bc08/src/service-app/classes/FanControlLogic.ts#L210-L307)
dokumentiert.

Für die aktuelle `hwmon`-Schnittstelle rechnet TCC den Sollwert auf den
Linux-PWM-Bereich um:

```text
PWM = round(Prozent / 100 × 255)
```

| Sollwert | sysfs-PWM |
| -------: | --------: |
|      0 % |         0 |
|     20 % |        51 |
|     25 % |        64 |
|     30 % |        77 |
|     40 % |       102 |
|     50 % |       128 |
|     60 % |       153 |
|     75 % |       191 |
|     90 % |       230 |
|    100 % |       255 |

Der jeweilige Kernel-Treiber übersetzt diesen Wert weiter in das
hardwareabhängige EC-Format. Deshalb lässt sich daraus ohne Messreihe keine
allgemeingültige Prozent-zu-RPM-Kennlinie ableiten. Der EC-Befehl `0x99` dieses
Projekts erwartet ebenfalls ein Byte von `0` bis `255`; ob dessen Skalierung
mit der aktuellen TCC-`hwmon`-Skala identisch ist, muss jedoch für das konkrete
Laptopmodell und dessen Firmware verifiziert werden.

### Profilauswahl und thermische Auswirkungen

Das Profil wird mit `--profile silent|quiet|balanced|cool|freezy` ausgewählt.
Ohne gültigen Wert wird `balanced` verwendet. Die systemd-Unit setzt diesen
Standard und liest optional `/etc/default/tuxedo-fan-control`; dort kann
beispielsweise `TUXEDO_FAN_PROFILE=quiet` gesetzt werden.

`silent` und `quiet` lassen den Lüfter bei niedrigeren Temperaturen aus und
fordern bei gleicher Temperatur meist weniger Lüfterleistung an. `cool` und
insbesondere `freezy` beginnen früher beziehungsweise mit höherer Leistung und
können dadurch die Bauteiltemperaturen auf Kosten von Geräusch und Verschleiß
senken. Alle Profile erreichen laut Referenztabelle spätestens bei `95 °C`
100 %. Die tatsächliche thermische Wirkung bleibt vom konkreten EC, der
Firmware, dem Kühlsystem und der Bedeutung des gelesenen Temperatursensors
abhängig.

## Regelzyklus

Die Regelung liest die Temperatur alle 250 ms und folgt dem zugehörigen
Tabellenwert unmittelbar. Der aktuelle EC-Wert wird bei einer Änderung sowie
spätestens alle zwei Sekunden erneut an den EC gesendet.

Schlägt eine Temperaturtransaktion fehl, wird aus dem ungültigen Messwert kein
Lüftersollwert berechnet und in diesem Zyklus kein Lüfterbefehl gesendet. Ein
fehlgeschlagener Lüfterbefehl wird ebenfalls als Fehler des Regelzyklus
behandelt. Nach einem vollständig erfolgreichen Zyklus wird der Fehlerzähler
zurückgesetzt. Nach drei aufeinanderfolgenden fehlgeschlagenen Zyklen beendet
sich der Daemon mit einem Fehler und einer Diagnose. Die systemd-Unit startet
ihn wegen `Restart=on-failure` anschließend entsprechend der systemd-Richtlinie
neu.

Der Code versucht vor dem Beenden nicht, eine vermeintliche Notfall-
Lüfterstufe zu setzen. Zwar ist `255` der höchste von den Profilen verwendete
Sollwert, aber die vorliegenden Unterlagen garantieren nicht modell- und
firmwareübergreifend, dass eine weitere, möglicherweise ebenfalls nur teilweise
übertragene `0x99`-Transaktion nach einem Kommunikationsfehler einen sicheren
Hardwarezustand herstellt. Die weitere Schutzwirkung der Firmware und der
Hardware bleibt daher erforderlich.

## Berechtigungen und Betriebsrisiken

Der systemd-Dienst läuft als `root`, weil `ioperm()`, `inb()` und `outb()` für
direkte I/O-Portzugriffe privilegierte Rechte benötigen.

Die EC-Funktionen begrenzen ihre Wartevorgänge und propagieren
Kommunikationsfehler bis in die Regelschleife. Es gibt weiterhin keine
modellunabhängige Validierung eines erfolgreich gelesenen Temperaturbytes und
keinen dokumentierten separaten Notfallbefehl in der Software. Die Regelung ist
daher von der EC-Implementierung und Firmware des jeweiligen Laptops abhängig.
Eine falsche Port- oder Befehlsbelegung kann zu plausibel erscheinenden, aber
fehlerhaften Temperaturwerten oder einer ungeeigneten Lüfteransteuerung führen.

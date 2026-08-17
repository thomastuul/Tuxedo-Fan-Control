# Technische Analyse

## Überblick

`Tuxedo-Fan-Control` kommuniziert über das Gerät `/dev/tuxedo_io` mit dem
signierten TUXEDO-Kerneltreiber. Nur der Kerneltreiber greift auf ACPI/WMI und
den Embedded Controller (EC) des Laptops zu. Der Dienst verwendet weder direkte
x86-I/O-Portzugriffe noch `/dev/mem` oder `/dev/port` und funktioniert deshalb
auch mit dem durch Secure Boot aktivierten Kernel-Lockdown.

Die verwendete IOCTL-API entspricht der Clevo-Schnittstelle von
`tuxedo-drivers` 4.22.3 und der offiziellen Implementierung im TUXEDO Control
Center 3.0.9
([`tuxedo_io_api.hh`](https://github.com/tuxedocomputers/tuxedo-control-center/blob/1a4d39ba5795e08e4e1ff6245ac908e2712262bf/src/native-lib/tuxedo_io_lib/tuxedo_io_api.hh)).
Beim Start muss `R_HWCHECK_CL` das Gerät als unterstützte Clevo-Hardware
identifizieren; andernfalls nimmt der Dienst keine Lüfteränderung vor.

## Temperaturauslesung

Die Funktion `getLocalTemp()` ruft `R_CL_FANINFO1` auf. Das zurückgegebene
32-Bit-Wort ist entsprechend der offiziellen TUXEDO-Control-Center-
Implementierung aufgebaut:

| Bits  | Bedeutung                              |
| ----- | -------------------------------------- |
| 0–7   | aktueller Rohwert von Lüfter 1         |
| 8–15  | erster, uneinheitlicher Temperaturwert |
| 16–23 | bevorzugter lokaler Temperaturwert     |

Der Dienst verwendet ausschließlich Bits 16–23. Fehler beim Öffnen des Geräts,
bei der Hardwareidentifikation oder beim IOCTL werden als explizite Statuswerte
bis in die Regelschleife propagiert.

Der gelesene Wert wird als Temperatur in Grad Celsius interpretiert. Werte
außerhalb von 10 bis 110 °C und Sprünge von mehr als 30 °C gegenüber dem letzten
plausiblen Messwert werden als implausibel verworfen und führen in diesem Zyklus
zu keinem Lüfterbefehl. Der Code geht entsprechend der TUXEDO-API davon aus,
dass dieses Byte einen Celsiuswert liefert.

Aus dem Quelltext lässt sich nicht sicher bestimmen, ob dieser EC-Wert die
CPU-Kerntemperatur, die CPU-Package-Temperatur oder einen anderen internen
Temperatursensor repräsentiert. Sicher ist nur, dass kein Linux-Temperatur-
interface abgefragt wird, sondern ein vom TUXEDO-Treiber gelieferter,
laptopspezifischer EC-Messwert.

## Lüfteransteuerung

Die Funktion `setFanSpeed()` verwendet `W_CL_FANSPEED`. Vor jedem Schreibzugriff
liest sie `R_CL_FANINFO1` bis `R_CL_FANINFO3`, ersetzt nur das niederwertige
Byte für Lüfter 1 und übernimmt die aktuellen Rohwerte möglicher weiterer
Lüfter unverändert in das gepackte 32-Bit-Argument. Damit verändert ein
CPU-Profil keine unabhängigen GPU- oder Zusatzlüfter.

Der Geschwindigkeitswert für Lüfter 1 ist ein Byte:

- `0`: theoretisch aus
- `255`: maximale Geschwindigkeit
- `0`: kleinster vom Programm verwendeter Wert; Profile dürfen den Lüfter damit
  unterhalb ihrer ersten aktiven Stufe abschalten

Der Kerneltreiber validiert Mindestwerte und übersetzt das IOCTL über seine
aktive Clevo-ACPI/WMI-Schnittstelle in den hardwareabhängigen EC-Zugriff. Mit
`W_CL_FANAUTO` kann die automatische Firmware-Regelung für alle drei Kanäle
wieder aktiviert werden.

Der Clevo-Pfad von `tuxedo-drivers` 4.22.3 rundet Werte unterhalb der
Mindestgeschwindigkeit: Werte von 0 bis 30 werden zu 0, Werte von 31 bis 62 zu
63 und höhere Werte bleiben unverändert. Nach `W_CL_FANSPEED` wartet der
Treiber 100 ms, damit ein nachfolgendes `R_CL_FANINFO1` den neuen Wert liefern
kann. Tuxedo-Fan-Control bildet diese Normalisierung bei seiner Rückleseprüfung
nach.

### Rückleseprüfung und maskierte Treiberfehler

Der Clevo-IOCTL-Handler in `tuxedo-drivers` 4.22.3 übernimmt den Rückgabestatus
von `clevo_evaluate_method()` bei mehreren Lesezugriffen, wertet ihn aber nicht
aus. Beim Schreibzugriff `W_CL_FANSPEED` wird der Status nicht gespeichert. Der
Handler gibt anschließend unabhängig davon 0 an den Userspace zurück. Dieses
Verhalten ist auch in der
[aktuellen offiziellen Treiberquelle](https://github.com/tuxedocomputers/tuxedo-drivers/blob/main/src/tuxedo_io/tuxedo_io.c#L285-L394)
sichtbar. Ein nicht angewendeter ACPI-/WMI-Befehl kann deshalb auf
Userspace-Ebene als erfolgreicher IOCTL erscheinen.

Nach jedem Lüfterbefehl liest Tuxedo-Fan-Control daher `R_CL_FANINFO1` erneut.
Der Regelzyklus gilt nur als erfolgreich, wenn der dabei enthaltene
Temperaturwert grundsätzlich plausibel ist und der zurückgelesene Lüfterwert
dem normalisierten Sollwert entspricht. Ein abweichender Wert oder eine
genullte, unplausible Antwort wird wie ein EC-Kommunikationsfehler behandelt.
Die Prüfung kann keinen Fehler erkennen, bei dem sowohl Schreib- als auch
Lesezugriff konsistent falsche, aber plausible Werte liefern.

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
allgemeingültige Prozent-zu-RPM-Kennlinie ableiten. Die Clevo-IOCTL-Schnittstelle
dieses Projekts erwartet ebenfalls ein Byte von `0` bis `255`; der
TUXEDO-Kerneltreiber übernimmt die weitere hardwareabhängige Umrechnung.

### Profilauswahl und thermische Auswirkungen

Das Profil wird mit `--profile silent|quiet|balanced|cool|freezy` ausgewählt.
Ohne gültigen Wert wird `balanced` verwendet und eine Diagnose geloggt, damit
die Lüftersteuerung trotz Fehlkonfiguration weiterläuft. Die systemd-Unit setzt
diesen Standard und liest optional `/etc/default/tuxedo-fan-control`; dort kann
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
Tabellenwert unmittelbar, sofern der Messwert plausibel ist. Der aktuelle
EC-Wert wird bei einer Änderung sowie spätestens alle zwei Sekunden erneut an
den EC gesendet und anschließend zurückgelesen. Temperatur, Sollwert und
zurückgelesener Lüfterwert werden alle 30 Sekunden in das Journal geschrieben
und als systemd-Statustext veröffentlicht.

Schlägt eine Temperaturtransaktion fehl oder liefert der EC einen implausiblen
Temperaturwert, wird aus diesem Wert kein Lüftersollwert berechnet und in diesem
Zyklus kein regulärer Lüfterbefehl gesendet. Ein fehlgeschlagener Lüfterbefehl
wird ebenfalls als Fehler des Regelzyklus behandelt. Nach einem vollständig
erfolgreichen Zyklus wird der Fehlerzähler zurückgesetzt. Nach drei
aufeinanderfolgenden fehlgeschlagenen Zyklen beendet sich der Daemon mit einem
Fehler und einer Diagnose. Die systemd-Unit startet ihn wegen
`Restart=on-failure` mit einer Wartezeit und Start-Limitierung neu.

Die Unit aktiviert zusätzlich einen systemd-Service-Watchdog mit fünf Sekunden
Zeitlimit. Der Daemon sendet nach vollständig durchlaufenen Regelzyklen
`WATCHDOG=1`. Bleibt der einzige Regelthread beispielsweise in einem synchronen
IOCTL hängen, beendet systemd den Prozess mit `SIGABRT` und startet ihn gemäß
`Restart=on-failure` neu. Ein im Kernel ununterbrechbar blockierter ACPI-/WMI-
Aufruf kann durch einen Userspace-Watchdog nicht garantiert gelöst werden; in
diesem Fall bleibt eine Treiber-, Firmware- oder Systemwiederherstellung nötig.

Bei kontrolliertem Stop und vor dem Beenden nach wiederholten Regelungsfehlern
versucht der Code mit dem dokumentierten `W_CL_FANAUTO`, die automatische
Firmware-Regelung für alle Kanäle wieder zu aktivieren. Nur wenn dieser IOCTL
einen Fehler meldet, wird als thermischer Rückfall der höchste von den Profilen
verwendete Sollwert `255` für Lüfter 1 angefordert. Weil der Treiber auch den
Status von `W_CL_FANAUTO` maskieren kann, bleibt die weitere Schutzwirkung der
Firmware und Hardware erforderlich.

## Berechtigungen und Betriebsrisiken

Der systemd-Dienst läuft als `root`, weil `/dev/tuxedo_io` standardmäßig nur für
privilegierte Prozesse zugänglich ist. Er benötigt jedoch keine Linux-
Capabilities; insbesondere ist `CAP_SYS_RAWIO` aus der Bounding Set entfernt.
`DevicePolicy=closed` und `DeviceAllow=/dev/tuxedo_io rw` begrenzen den
Gerätezugriff auf die vorgesehene Kernel-API. Hinzu kommen Dateisystemschutz,
privates `/tmp`, eingeschränkte Address-Families, ein systemd-Watchdog und
Start-Limits.

Die IOCTL-Funktionen propagieren Kommunikationsfehler bis in die
Regelschleife. Erfolgreich gelesene
Temperaturbytes werden nur anhand allgemeiner Plausibilitätsgrenzen bewertet;
es gibt weiterhin keine modellunabhängige Validierung der Sensoridentität und
keinen dokumentierten separaten Notfallbefehl in der Software. Die Regelung ist
daher von der EC-Implementierung und Firmware des jeweiligen Laptops abhängig.
Eine unpassende Treiber-/Firmwarekombination kann weiterhin zu plausibel
erscheinenden, aber fehlerhaften Temperaturwerten oder einer ungeeigneten
Lüfteransteuerung führen.

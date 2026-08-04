# AGENTS.md

## Projektcharakter

- Tuxedo-Fan-Control ist ein nativer Linux-Dienst zur Lüftersteuerung.
- Das Programm kommuniziert direkt mit dem Embedded Controller (EC) des
  Laptops über privilegierte I/O-Portzugriffe. Änderungen an EC-Ports,
  Befehlen oder Datenformaten sind hardware- und firmwareabhängig und müssen
  besonders sorgfältig geprüft werden.
- Die Software läuft als Hintergrunddienst.

## Build und Tests

- Die Entwicklung benötigt `g++` und `make`. Abhängigkeiten sollen bevorzugt
  in einem dedizierten Builder- oder Entwicklungscontainer installiert werden,
  nicht unnötig auf dem Produktivsystem.
- Das Build-System soll vorzugsweise mit CMake realisiert werden. Neue Build-
  Ziele und Installationslogik sind daher bevorzugt in CMake zu ergänzen; das
  vorhandene Makefile bleibt bis zu einer abgestimmten Umstellung kompatibel.
- Das Programm läuft anschließend nativ auf dem Host, weil der EC aus einem
  Container heraus nicht der normale Betriebsweg ist.
- Für eine reine Build-Prüfung kann `make compile` verwendet werden. Die
  erzeugte Binärdatei ist ein Build-Artefakt und gehört nicht in das Repository.
- Unit-Tests müssen erfolgreich ausgeführt werden, bevor ein Release-Artefakt
  ausgeliefert, für die Auslieferung übersetzt oder installiert werden darf.
  Die Tests müssen insbesondere die Boundary-Werte und ungültigen Werte aller
  Schnittstellenargumente abdecken.
- Vor Änderungen an der Installation mindestens eine Syntaxprüfung mit
  `g++ -fsyntax-only Tuxedo-Fan-Control.cpp` und eine Make-Dry-Run-Prüfung mit
  `make -n all` ausführen.
- Nach jeder Softwareänderung müssen `clang-format` und `clang-tidy` im dafür
  bereitgestellten Docker-Container ausgeführt werden. Änderungen gelten erst
  nach erfolgreicher Formatierungs- und Lint-Prüfung als abgeschlossen.
- `make all` installiert die Binärdatei nach `/usr/local/bin`, kopiert die
  systemd-Unit nach `/etc/systemd/system` und aktiviert bzw. startet den
  Dienst. Dieser Befehl darf nur mit Root-Rechten und mit ausdrücklicher
  Absicht zur Systeminstallation ausgeführt werden.
- Tests dürfen den laufenden Lüfterdienst nicht ungefragt stoppen, starten oder
  mit konkurrierenden EC-Zugriffen überlagern. Direkte EC-Zugriffe anderer
  Programme sind während Tests zu vermeiden.
- Wenn keine automatisierten Tests vorhanden sind, muss das in der Übergabe
  ausdrücklich genannt werden. Änderungen an der Regelungslogik sollen nach
  Möglichkeit zusätzlich anhand von aufgezeichneten Temperaturwerten oder
  einer separaten Simulation geprüft werden.

## Service und Betrieb

- Die systemd-Unit ist die maßgebliche Installations- und Betriebsdefinition.
  Änderungen an Binary-Pfad, Unit-Namen, Benutzer, Restart-Verhalten oder
  Priorität müssen in `Tuxedo-Fan-Control.service`, `makefile` und der
  Dokumentation konsistent bleiben.
- Der Dienst läuft als `root`, da `ioperm()`, `inb()` und `outb()` privilegierte
  Hardwarezugriffe benötigen. Eine Umstellung auf einen anderen Benutzer ist
  erst nach einer technischen Prüfung der erforderlichen Rechte zulässig.
- Änderungen an Lüfterkurve, Mindestgeschwindigkeit, Temperaturgrenzen oder
  EC-Befehlen müssen ihre thermischen Auswirkungen dokumentieren.
- Ein Service-Neustart oder eine Änderung der aktiven Lüftersteuerung ist eine
  Systemänderung und darf nicht als bloße Build-Prüfung behandelt werden.

## Dokumentation und Herkunft

- Technische Erkenntnisse zum EC, zur Temperaturmessung und zur
  Lüfteransteuerung gehören in `TECHNICAL_ANALYSIS.md` oder in passende
  Dateien unter `doc/`.
- Der ursprüngliche Autor François Kneib und das Originalprojekt müssen in der
  Dokumentation genannt bleiben:
  <https://gitlab.com/francois.kneib/clevo-N151ZU-fan-controller>
- Heruntergeladene Referenzdokumente unter `doc/` müssen in
  `doc/README.md` mit Quelle und Zweck dokumentiert werden.
- Markdown-Dateien müssen in einem dafür bereitgestellten Docker-Container mit
  dem vorgesehenen Formatter und Linter geprüft werden. Node.js, npm,
  Formatter und Linter werden für die Dokumentationsprüfung nicht auf dem Host
  installiert.

## GitHub CLI

- `gh` muss immer außerhalb der Sandbox ausgeführt werden. Innerhalb der
  Sandbox funktioniert die GitHub CLI in diesem Projekt nicht.
- Repository-, Issue-, Pull-Request- und Remote-Operationen mit `gh` sind vor
  der Ausführung auf Zielrepository und Branch zu prüfen.

## Versionsverwaltung

- Das Projekt besitzt derzeit keine automatische Versionsdatei und keine
  automatische SemVer- oder Paketversionslogik.
- Versionsnummern dürfen daher nicht eigenmächtig in einzelne Quell- oder
  Dokumentationsdateien eingeführt werden. Eine spätere Versionierungslogik
  muss zuerst als zusammenhängende Projektänderung dokumentiert werden.
- Commits sollen den tatsächlichen Änderungsumfang knapp beschreiben.
- Unabhängige oder fremde Arbeitsänderungen dürfen nicht stillschweigend in
  Commits aufgenommen werden.

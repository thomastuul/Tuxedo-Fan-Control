# Schnellinstallation

Das aktuelle Debian-Paket (`.deb`) und seine SHA-256-Prüfsumme stehen unter
[GitHub Releases](https://github.com/thomastuul/Tuxedo-Fan-Control/releases).
Vor dem Start des Dienstes muss der passende TUXEDO-Kernel-Treiber installiert
und geladen sein. Auf aktuellen Systemen ist das `tuxedo-drivers`, auf älteren
Installationen `tuxedo-keyboard`. Prüfen, ob die benötigten Module sichtbar
sind, insbesondere `tuxedo_keyboard` und, falls für das Gerät nötig,
`tuxedo_io`. Der Dienst verwendet dessen `/dev/tuxedo_io`-Schnittstelle und
keine direkten I/O-Portzugriffe:

```sh
lsmod | grep -E 'tuxedo_keyboard|tuxedo_io|tuxedo'
```

Wenn Secure Boot unsignierte DKMS-Module blockiert, kann der EC-/Lüfterzugriff
trotz installiertem Treiberpaket fehlen. Dann muss der Modul-Signaturschlüssel
gemäß TUXEDO-/Distributionsanleitung eingeschrieben werden. Mit signierten und
vertrauenswürdigen Modulen ist der Dienst mit Secure-Boot-Lockdown kompatibel.

Nach dem Herunterladen wird das Paket aus seinem Download-Verzeichnis
installiert:

```sh
sudo apt install ./tuxedo-fan-control_*.deb
```

Das gewünschte Profil (`silent`, `quiet`, `balanced`, `cool` oder `freezy`)
wird über `/etc/default/tuxedo-fan-control` eingestellt:

```sh
echo 'TUXEDO_FAN_PROFILE=quiet' |
  sudo tee /etc/default/tuxedo-fan-control
```

Anschließend die systemd-Konfiguration neu laden und den Dienst aktivieren und
starten:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now Tuxedo-Fan-Control.service
```

Status, installierte Version und tatsächlich geladenes Profil kontrollieren:

```sh
dpkg-query -W tuxedo-fan-control
systemctl status Tuxedo-Fan-Control.service
journalctl -u Tuxedo-Fan-Control.service -n 20 --no-pager
```

Im Journal muss beispielsweise `Using fan profile: quiet` erscheinen.

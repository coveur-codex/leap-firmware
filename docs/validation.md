# Durchgeführte Prüfungen

Stand: 2026-10-02. Referenz-Homeserver `8175c9f`, unverändert.

- Echter Arduino-ESP32-Cross-Build für ESP32-S3 N16R8, Core **3.3.0**, OPI-PSRAM,
  16 MiB Flash, USB CDC und eigene Partitionstabelle; kein Mock-Build.
- Bibliotheken: ArduinoJson 7.4.2, Arduino_GFX 1.6.3, PNGdec 1.1.6, JPEGDEC 1.8.2.
- Native C++-Tests mit `-Wall -Wextra -Werror`: Entprellung, Wiederholung,
  Long-Press, `millis()`-Überlauf, UTC-Konvertierung, numerischer Stable/Beta-
  Versionsvergleich, Funkpaketgrenzen und Deduplikation, Pfad-Traversal,
  Manifestgröße, doppelte Dateien, fremde Downloadpfade, PNG-only-Avatar-Auswahl.
- Echte FastAPI-/SQLAlchemy-Homeserver-Routen in temporärer Datenbank getestet:
  Sync, Konfiguration, versionierte Manifest-/Dateidownloads, SHA-256 und Größen,
  Ablehnung unvollständiger Inventare, Bestätigung/Bereinigung, abgeschlossene
  Pläne und deaktivierte Kommunikation. Keine produktiven Daten geändert.
- Serverfixture mit altem `preview.svg` und neuem 80×80-PNG unter
  `data/pet/idle/frame_01.png`: Firmware-Auswahlregel überspringt SVG und nutzt
  das PNG. Die echte vom Nutzer erwähnte Bilddatei war nicht im Workspace;
  die Test-PNG ist eigens erzeugt und wird nicht als Nutzerasset ausgegeben.
- Partitionstest: keine Überlappung, korrekte App-Ausrichtung, zwei gleiche
  4-MiB-Slots, Dateisystem und Coredump enden exakt bei 16 MiB.
- App-Image-Magic und ESP32-S3-Chip-ID sowie tatsächliche 4-MiB-Slotgrenze geprüft.
- ELF-Symbolprüfung: `verifyRollbackLater` ist eine starke Implementierung (`T`),
  beide ESP-IDF-Bestätigungs-/Rollback-Funktionen sind im gelinkten Programm.
- SHA-256, App-Größe und Build-Zusammenfassung siehe unten.

Die Cloud-Toolchain wurde aus den offiziellen GitHub-Distributionen eingerichtet.
Der normale Arduino-Downloadhost war durch die Netzwerkpolicy nicht zugänglich;
CLI-Discovery-Tools für physische USB-Ports sind in dieser Umgebung nicht installiert.
Die dadurch ausgegebenen `serial-discovery`-/`mdns-discovery`-Initialisierungshinweise
betreffen keine Compiler-/Linkerfehler. Der Build erfolgte erfolgreich. Die CI
installiert die reguläre vollständige Arduino-Toolchain auf GitHub Actions.

**Nicht am Gerät geprüft:** elektrische Verdrahtung, NV3007-Paneloffset/-Farben,
MPU-Achsenorientierung, reale I²S-Ausgabe, Funkkanalwechsel/Reichweite, Flash-
Stromausfallverhalten, physischer OTA-Rollback und Dauerlauf. Hierzu gibt es
[konkrete Abnahmeschritte](acceptance.md). Ein erfolgreicher Cross-Build ersetzt
keinen dieser Hardwaretests.

## Abschließender Build

- App-Binary: **1371712 Byte**, 32.7% des tatsächlichen OTA-Slots.
- Statische RAM-Belegung: **112644 Byte**; dynamische PSRAM-Nutzung separat.
- SHA-256: `fbd80200cbefef7c44bda1bc387545ad4f96445cf3e3c513628c618d8bc6dfa4`.
- `--warnings all`: keine Compilerwarnungen im abschließenden Build.
- CLI-Größenanzeige bei „Custom“ bezieht sich auf 16 MiB Flash;
  `tools/check_image.py` prüft deshalb zusätzlich die echte 4-MiB-Appgrenze.
- Build mit `LocalConfig.example.h`: keine persönlichen WLAN-Zugangsdaten
  enthalten. Vor dem Einsatz die eigene `LocalConfig.h` anlegen und neu bauen.

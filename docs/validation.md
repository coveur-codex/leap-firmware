# Durchgeführte Prüfungen

Stand: 2026-10-02. Referenz-Homeserver `8175c9f`, unverändert.

## PNG-Pufferkorrektur: 1.0.0-beta.7

- PNGdec **1.1.6**, unveränderter Bibliotheksquelltext: 80×80 RGBA erfolgreich;
  synthetisches 428×142 RGBA bricht mit Code 2 nach 11 Zeilen ab, Pixelvergleich
  schlägt fehl. Die intern gepufferten zwei Zeilen überlaufen den Standardpuffer.
- Identischer Test mit global `PNG_MAX_BUFFERED_PIXELS=8768`: alle Pixel korrekt,
  142 Zeilen dekodiert. Zusätzlich RGB/RGBA mit 80 und 1024 Pixeln Breite geprüft.
- Defekte IHDR-CRC und Interlacing werden im Regressionstest weiterhin abgelehnt.
- Der ESP32-Core 3.3.0 reicht `build_opt.h` an alle C-/C++-Übersetzungseinheiten
  weiter. Ein `static_assert` verhindert Builds ohne ausreichend großen Puffer.
- Die Original-PNG des eingebetteten Nutzerbilds war nicht als Datei zugänglich.
  Die Reproduktion nutzt ausdrücklich synthetische Testbilder, nicht das Nutzerbild.
- Gerätetest nach erneutem Flashen und Sync bleibt erforderlich. Keine Änderung
  am Homeserver oder am Asset-Paket für diese Decoderkorrektur erforderlich.

## Asset-Fehlerdiagnose: 1.0.0-beta.6

Der bereitgestellte Serverlog zeigt vier begonnene Paketinstallationen und einen
Abbruch der vierten; Paket-IDs fehlen im alten Eventformat. Die Ursache der letzten
Installation ist damit noch nicht belegbar. Der Inventarwechsel findet erst nach
Prüfung aller Pakete statt, weshalb bereits geprüfte Pakete trotzdem inaktiv bleiben.

- Paket-ID, Version und Fehlerdetails werden auf jedem Installationsabbruch gemeldet.
- Transportfehler unterscheiden Status/Länge, Timeout, Stream-/Schreibfehler und Hash.
- PNG-Dimensionsfehler melden die tatsächlichen Maße, Decoder-/Katalogfehler die Datei.
- Der echte Serververtragstest prüft die Persistenz der neuen Eventfelder und dass
  ein `update_failed` weder Cleanup freigibt noch das aktive Inventar überschreibt.
- Native Tests bestanden; Cross-Build über GitHub Actions. Ein erneuter Geräte-Sync
  mit beta.6 wird benötigt, um den konkreten Paketfehler festzustellen.

## Avatare, Flugzeugpositionen und Artikelbilder: 1.0.0-beta.5

- Standard-PNGs aus den bestehenden SVG-Avataren rasterisiert (CairoSVG 2.7.1,
  80×80, Transparenz); keine neue Laufzeitabhängigkeit des Servers.
- Servertests prüfen alle fünf Avatarbilder, idempotente Aktualisierung originaler
  SVG-Pakete, erhaltene alte Versionen und Schutz eigener Avatarpakete.
- Native Projektionstests prüfen Mittelpunkt, alle vier Himmelsrichtungen,
  Entfernungsmaßstab, Datumsgrenze, Radiusgrenze und ungültige Koordinaten.
- Server-/Firmware-Vertragstest prüft reale Sync-Routen und PNG-Paketmanifeste.
- Wissensbilder werden rechts unter Erhalt des Seitenverhältnisses angezeigt.
- Physische Display-Abnahme und Live-Provider bleiben offen; Cross-Build über CI.

## Sidebar und Regenradar: 1.0.0-beta.4 (2026-10-03)

- Sidebar-Geometrie: x=0–85, Status y=4–11, Uhr y=15–36, Avatar
  x=3–82/y=38–117, Seitenicons y=119–129, Seitentitel y=133–140.
- Hauptinhalt beginnt bei x=94; die bisherige Titelzeile entfällt. Wetterradar
  x=308–419/y=8–119, Quellenhinweis y=122–129.
- Native Firmwaretests bestanden; ESP32-Cross-Build erfolgt im Push-Workflow.
- Homeserver-Radartests prüfen echte PNG-Verarbeitung anhand lokaler Fixtures,
  Cache/Offline-Fallback, feste Providerhosts und Geräte-/Bild-Endpunkte.
- Live-RainViewer-Abruf und Darstellung am physischen Display bleiben zu prüfen.

## LittleFS-Erststart und Diagnose: 1.0.0-beta.3 (2026-10-03)

Das Hardwarelog mit `fsFree=0`, `selftest=0` und `wifi=1` zeigt einen
fehlgeschlagenen Mount bei bereits verbundenem WLAN. Der Sync wartet auf Speicher
und lokale Boot-Bestätigung. Eine vollständig gelöschte Partition wird nun nach
vollständiger Leseprüfung initialisiert; belegte oder nicht lesbare Partitionen
werden weiterhin ausschließlich nach physischer Bestätigung formatiert.

- Native Regressionstests: vollständig leere 8064-KiB-Partition, Daten am Anfang,
  hinter den Superblöcken und am Ende, Lesefehler sowie unvollständiger letzter Block.
- Bestehende Core-/Protokolltests mit ArduinoJson 7.4.2 und Partitionstest bestanden.
- LittleFS-Aufrufreihenfolge gegen die Implementierung des ESP32-Core 3.3.0 geprüft.
- WLAN-Verbindungswechsel melden IP/RSSI/Kanal bzw. Status; Sync meldet Blockaden.
- Lokaler Cross-Build nicht ausführbar: Arduino-Downloadhosts sind durch die
  Netzwerkpolicy gesperrt. Der bestehende GitHub-Actions-Workflow baut beim Push.
- Am Gerät noch zu prüfen: Erstinitialisierung, anschließender automatischer Sync,
  erneuter Start mit erhaltenen Daten sowie Recovery eines beschädigten Dateisystems.

## Display-Korrektur: 1.0.0-beta.2

Abgleich mit dem vom Nutzer als funktionierend bestätigten Hardwaretest:
`nv3007_279_init_operations` statt Standardsequenz, `ips=false` statt `true`,
Spaltenoffsets 12/14 statt 12/12, Zeilenoffsets 0/0. Konstruktor und Sequenz sind
im Quellstand von Arduino_GFX **v1.6.3** vorhanden. SPI bleibt bei 20 MHz,
Panelgröße bei 142×428 und die Firmware-UI bei Rotation 1 (428×142).
Die GPIO-Belegung bleibt wie vom Nutzer bestätigt bestehen.

Der Anhang enthält keine `Pins.h` oder `TestConfig.h`; deren Werte (Rotation,
Backlight-Invertierung, Akku-Spannungsteilerfaktor) lassen sich daraus nicht
zusätzlich ablesen. Die Test-ADC-Abfrage wird daher nicht als kalibrierte
Akkuanzeige übernommen. Tasterentprellung (25 ms), MPU6050-Register für
Messbereiche/Filter und I²S-Stereoformat stimmen überein. Die Firmware verwendet
für den MPU6050 weiterhin 400 kHz/5 ms Timeout gegenüber 100 kHz/20 ms im Test;
dies ist kein aus dem Test belegter Fehler.

Die frühere lokale Arduino-Toolchain ist in dieser Sitzung nicht vorhanden.
Der Cross-Build für diese Änderung erfolgt über den bestehenden GitHub-Actions-
Workflow. Ein visueller Test der vollständigen Firmware am Gerät bleibt offen.
Die nachfolgenden Build-Messwerte gehören zum vorherigen Stand **1.0.0-beta.1**.

## Bisherige Prüfungen: 1.0.0-beta.1

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

# Podcasts 1.7.0 · Prüfung und Geräteabnahme

Stand 14.11.2026. Vergleichsbasis: unverändertes `origin/main` **7914b3b**
(Firmware 1.6.4). Beide universellen Images mit Arduino CLI 1.3.1,
Arduino-ESP32 3.3.0, ArduinoJson 7.4.2, GFX 1.6.3, PNGdec 1.1.6,
JPEGDEC 1.8.2 und WebSockets 2.7.0, FQBN:

```text
esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=custom
```

| Größe | 1.6.4 | 1.7.0 | Zusätzlich |
|---|---:|---:|---:|
| Universelle App-Binary | 1.782.864 B | 1.837.120 B | **54.256 B (52,98 KiB)** |
| Programmbelegung laut Compiler | 1.782.723 B | 1.836.975 B | 54.252 B |
| Statische globale Variablen | 96.028 B | 96.412 B | **384 B** |

Die App belegt rund 43,8 % eines unveränderten 4-MiB-OTA-Slots. Version,
ESP32-S3-Header, universeller NVS-Marker und Slotgrenze bestehen die Imageprüfung.
SHA-256 der universellen App:

```text
71451e851e10730c027c211834e24e97aca9c0ef9f5c47e95116c37bdf748c14
```

Native Suite und Serververtrag bestanden; Homeserver: **412 Tests bestanden**.
Docker-Prüfimage mit denselben Requirements und vorgeladenen Wheels gebaut;
Migration, Verwaltungsseiten, Geräte-Konfiguration, Episoden und Resume im Image
geprüft. Der unveränderte reguläre Docker-Build konnte hier wegen fehlender
DNS-Auflösung im Build-Container keine PyPI-Abhängigkeiten laden.

Dynamische Allokationen sind keine gemessene Heap-Spitze: der bestehende
24-KiB-MP3-Task und der 155-KiB-Sitzungsarbeitsbereich werden weiterverwendet.
Zusätzlich: 8.200 B PSRAM für den 512-Punkte-Suchindex im bestehenden Worker,
32 Zeiger für die Fortschrittsqueue sowie begrenzte, mit `jsonRam` allokierte
Metadaten-/Journal-Dokumente. Aktuelle Liste höchstens 20 Episoden, regulär zwölf;
Journal höchstens 128 Episodenstände. Kein weiterer Task oder Decoder.

## Automatisiert

- Vollständige native Suite `tools/test.sh`, inklusive bestehender Spiele,
  News-/Konfigurationsverträge, NVS, OTA, Benachrichtigungen, WAV und Synthese.
- Beide Arduino-Test-Dummys werden zusätzlich mit `-O2 -D_FORTIFY_SOURCE=2`
  gebaut und auf Stringkopie, Kürzung und Nullterminierung geprüft. Vorhandenes
  libc-`strlcpy` (glibc ab 2.38) wird wiederverwendet; nur ältere Hosts erhalten
  einen Ersatz. So kollidiert der Testcode nicht mit Ubuntu-Fortify-Inlines.
- Echte Mono-/Stereo-Frames und synthetisches 75-Sekunden-VBR-MP3:
  absolute Samples, Byte-Range/If-Range, ignoriertes Range, vollständiger EOF mit
  Puffer-Ausgabe, Seek ans Ende ohne Abschluss, abgeschnittene Antwort und Pause.
- Tatsächliche Podcast-Controller und Inbox: 30 Episoden mit Seitengrenze,
  teilweise gehörte Folge, -15/+30 Sekunden, Pause, durchgehende Wiedergabe beim
  Blättern, Seitenende, persistierter Neustart-Cache, Offline-Auswahl aus einer
  bekannten Liste, Upload, Auto-Weiter, Freigabeentzug und veraltete Antworten.
- Tatsächlicher GPIO-Sampler plus Debouncer: kurzer Rechts-Links-Druck erst bei
  Freigabe; langer Druck exakt einmal ohne kurz/repeat. Übrige Tasten unverändert.
- Begrenzte/typisierte Konfiguration und Episodendaten; ungültige IDs, URLs,
  Positionen, Dauern und übergroße Listen werden verworfen.
- Realer Homeserver-Vertragstest: RSS-Import, Sync-Konfiguration und paginierte
  Episoden-Antworten werden an die nativen C++-Validatoren übergeben.
- `tools/test_png.sh` mit echten gepinnten PNGdec/JPEGDEC-Bibliotheken.
- ESP32-S3 Universal- und Kabel-Provisionierungsbuild; Version/Marker, ESP-Header,
  SHA-256 und 4-MiB-Slotgrenze mit `tools/check_image.py` geprüft.
- Homeserver-Suite ergänzt Datenmodell-/Lösch-, RSS-/Atom-, Redirect-, MP3-Filter-,
  Freigabe-, Pagination-, Resume-, Revisions-, Abschluss- und Migrationstests.

Reproduzieren:

```sh
LEAP_HOMESERVER=/path/to/leap-homeserver python tests/server_contract.py /tmp/leap-fixtures.json
ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src tools/test.sh /tmp/leap-fixtures.json
ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src PNGDEC_SRC=/path/to/PNGdec/src JPEGDEC_SRC=/path/to/JPEGDEC/src tools/test_png.sh
```

## Am ESP32-S3 noch zu prüfen

- Beide Steuerkreuze, Lautstärkewechsel/-persistenz, identischer Balken und
  unveränderte 86-Pixel-Sidebar am realen 428×142-Display.
- Echte Lautstärke am MAX98357A, kontinuierlicher Klang bei WLAN/TLS und
  Speicher-/Stack-Minimum über einen langen Podcast.
- Kalter Resume und lange VBR-Seeks mit/ohne Range, Zeit bis zur Wiedergabe,
  Mono/Stereo, große ID3-Tags und wechselnde Provider-URLs.
- Schnelle Radio-/Podcast-/Folgenwechsel, Pause über längere Zeit, WLAN- und
  Homeserver-Ausfall, OTA und Stromverlust beim Journal-Rename.
- Migration vorhandener Produktionsdaten nach Backup; Aktivierung/Entzug am
  Server und Übernahme im regulären Gerätesync.

Klang, reale Displaypixel und physischer Stromverlust können in dieser Umgebung
nicht abgenommen werden. Für Seek-/Feed-/Offline-Grenzen siehe `podcasts.md`.

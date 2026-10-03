# LEAP Firmware

Neue Arduino-Firmware für **ESP32-S3 N16R8**, abgestimmt auf
[`leap-homeserver`](https://github.com/coveur-codex/leap-homeserver), Stand `8175c9f`.
Version: `1.0.0-beta.5`. Keine Übernahme alter Firmware: Das Zielrepository war leer.

Das Gerät startet aus LittleFS, zeigt Inhalte ohne WLAN und synchronisiert im
Hintergrund. Der Homeserver bestimmt Seiten, Reihenfolge, Identität, Alter,
Avatar, Quiz-Zuordnungen, Kommunikationsfreigabe, Asset-Auswahl und OTA-Kanal.
Fehlendes WLAN unterbricht die Bedienung nicht. Es gibt keinen erfundenen Akkustand.

**Status:** Implementierung mit Arduino-Build und Host-/API-Prüfungen; eine reale
Hardware-Abnahme ist vor einem regulären Geräte-Rollout erforderlich. Insbesondere
Display-Offset/Farben, Audio-Verdrahtung, Funkreichweite und Power-Cut-Rollback können
in einer Cloud-Umgebung nicht gemessen werden. Siehe [Abnahme](docs/acceptance.md).

## Arduino IDE: Einrichtung und Upload

1. Arduino IDE 2.x installieren. Boardverwalter-URL hinzufügen:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. **esp32 by Espressif Systems 3.3.0** installieren. Abweichende Core-Versionen
   müssen wegen I²S-, ESP-NOW-, LEDC- und Rollback-APIs neu geprüft werden.
3. Bibliotheken über den Library Manager installieren:

   | Library-Manager-Name | Version |
   | --- | --- |
   | ArduinoJson | 7.4.2 |
   | GFX Library for Arduino | 1.6.3 |
   | PNGdec | 1.1.6 |
   | JPEGDEC | 1.8.2 |

   WiFi, HTTPClient, LittleFS, Preferences, TLS und ESP-IDF-Treiber kommen aus
   dem ESP32-Core. Der MPU6050 wird direkt über Wire/I²C angesprochen.
4. `LEAP/LocalConfig.example.h` nach `LEAP/LocalConfig.h` kopieren und WLAN,
   Homeserver-URL und **exakt die im Homeserver angelegte Geräte-ID** eintragen.
   `LocalConfig.h` ist von Git ausgeschlossen. Ohne diese Datei lässt sich der
   Sketch bauen, startet aber ohne WLAN-Zugangsdaten.
5. `LEAP/LEAP.ino` öffnen. Board **ESP32S3 Dev Module**, Flash **16MB**,
   Flash-Modus **QIO**, Flash-Frequenz **80MHz**, PSRAM **OPI PSRAM**,
   USB CDC On Boot **Enabled**, USB Mode **Hardware CDC and JTAG**,
   Arduino läuft auf Core 1, **Partition Scheme: Custom**. `partitions.csv` neben dem Sketch wird vom Core
   automatisch übernommen. **Erase All Flash: Disabled** bei späteren Uploads.
6. Initial komplett über USB hochladen. Das installiert auch den zum Core
   gehörenden Bootloader und die Partitionstabelle. Serielle Ausgabe: **115200**.
7. Eine vollständig gelöschte LittleFS-Partition wird beim ersten Start automatisch
   initialisiert. Nach einem Mount-Fehler wird dafür die **gesamte Partition** auf
   `0xFF` geprüft. Vorhandene Daten oder Lesefehler verhindern die automatische
   Formatierung. Für eine bewusste Recovery: **beide Mitteltasten beim Einschalten
   3 Sekunden halten**. Jede Taste innerhalb dieser Frist loslassen bricht ab.
   Dies löscht lokale Inhalte, keine serverseitigen Daten.
8. Gerät im Homeserver aktivieren, Inhalte zuordnen und synchronisieren lassen.

Arduino CLI ist optional; PlatformIO wird nicht benötigt:

```sh
arduino-cli compile --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=custom' \
  --output-dir build/export LEAP
```

Die IDE-Option „Kompilierte Binärdatei exportieren“ erzeugt die App-Datei
`LEAP.ino.bin`. **Nur diese App-Binary** im Homeserver als Firmware hochladen,
keine `.merged.bin`, `.bootloader.bin` oder `.partitions.bin`. Vor einem Release
`FirmwareVersion` in `LEAP/src/Config.h` ändern; die hochgeladene Versionsnummer
muss exakt dazu passen.

## Finale Pinbelegung

Die zentrale Quelle ist [`LEAP/src/Hardware.h`](LEAP/src/Hardware.h).
Keine belegte abweichende Verdrahtung war im leeren Firmware-Repo oder im aktuellen
Homeserver vorhanden. Daher gilt unverändert die vom Auftraggeber vorgegebene
Zielbelegung:

| Funktion | GPIO |
| --- | --- |
| NV3007 SCK, MOSI, RESET, DC, CS | 12, 11, 13, 10, 9 |
| Backlight via AO3400A, PWM 20 kHz | 14 |
| Links: UP, DOWN, LEFT, RIGHT, CENTER | 4, 5, 6, 7, 8 |
| Rechts: UP, DOWN, LEFT, RIGHT, CENTER | 2, 15, 16, 21, 47 |
| MAX98357A BCLK, WS, DIN | 39, 40, 41 |
| MPU6050 SDA / SCL | 17 / 18 |
| Native USB D− / D+ — nicht anderweitig verwendet | 19 / 20 |

Beide Schalter: COM an GND, interne Pull-ups, aktiv LOW, 25 ms Entprellung.
Die I²S-Pins sind Soll-Belegung; physisch noch zu bestätigen. GPIO1 wird **nicht**
als Batterie-ADC eingerichtet: Es gibt keinen Nachweis für Messleitung oder
Spannungsteiler. Eine Lade-/Boost-Platine allein genügt nicht. Der MPU6050 ist laut ergänzender
Hardwarebestätigung eingebaut: I²C GPIO17/18, WHO_AM_I-Prüfung an 0x68/0x69,
±2 g und ±250 °/s. Achsentausch/Vorzeichen sind zentral in `Hardware.h` einstellbar.

Das Display ist nativ **142 × 428**, im Betrieb wie die Homeserver-Vorschau
**428 × 142** (Rotation 1). Die funktionierende Test-Firmware für das **2,79″-Panel**
liefert die Initialisierung: `nv3007_279_init_operations`, `ips=false`,
Spaltenoffsets **12/14**, Zeilenoffsets **0/0**. Arduino_GFX 1.6.3 enthält diese
Sequenz bereits. Am realen Panel Randmarkierungen prüfen.
Bei anderer Panelvariante ausschließlich `Hardware.h`/Display-Konstruktor anpassen.
SPI zunächst konservativ 20 MHz.

## Bedienung

| Bedienung | Wirkung |
| --- | --- |
| Mitte am Sperrbildschirm | Entsperren / Hauptmenü |
| Links LEFT / RIGHT | Vorige / nächste aktivierte Seite |
| Links CENTER | Menü / zurück; laufendes Spiel verlassen |
| Links CENTER 0,9 s halten | Sperren |
| Rechts UP / DOWN | Auswahl bzw. Text scrollen |
| Rechts LEFT / RIGHT | News/Flugzeug/Quiz wechseln, Einstellungen ändern |
| Rechts CENTER | Bestätigen, Nachricht senden, Quiz beantworten |

Nach einer Minute wird gedimmt, nach drei Minuten ohne Eingabe gesperrt.
Die Sidebar ist exakt **86 Pixel** breit. Sie zeigt kleine WLAN-/Gruppenfunk- und
Sync-Symbole, eine 22 Pixel hohe Uhr und den unskalierten 80×80-Avatar. Das
Akku-Symbol mit Strich bedeutet „unbekannt“ (keine angeschlossene Messleitung).
Unten stehen alle Seitenicons in der konfigurierten Reihenfolge, die aktuelle
Seite ist hervorgehoben; ihr Titel steht darunter statt im Hauptbereich. Ein
kleiner Punkt rechts neben der Uhr kennzeichnet einen noch nicht bestätigten Zeitstand. Ohne RTC kennt das Gerät eine stromlose Zeitspanne nicht.
Nach einem Check-in läuft die Uhr lokal weiter; Sommerzeit über POSIX-Zeitzone.

**Gruppenchat:** Rechts UP/DOWN wählt Vorlagen, MITTE sendet, LINKS/RECHTS
blättert durch die letzten acht Chatzeilen. Historie bleibt nur im RAM.

**Regenradar:** Die Wetterseite zeigt rechts eine 112×112-Pixel-Aufnahme von
RainViewer, zentriert auf den im Homeserver konfigurierten Standort (Zoom 7,
Nord oben, weißer Standortpunkt). Zeitstempel in UTC; „alt“ kennzeichnet Cache,
Offlinebetrieb oder Aufnahmen älter als 30 Minuten. Kein Bild wird als „kein Regen“
ersetzt: Fehlen Daten, steht dort „nicht verfügbar“. Das letzte Bild bleibt offline
verfügbar. Radar-Dateien werden separat gespeichert; nur die beiden aktuellen
Snapshots werden aufbewahrt. Hierfür muss auch der Homeserver auf den Stand mit
`GET /api/v1/devices/{id}/weather/radar` aktualisiert werden. Der Radarabruf erfolgt
mit dem regulären Sync (15 Minuten), keine Animation oder Vorhersage.

**Avatar:** Die Standard-Avatare werden im Homeserver als 80×80-PNG-Paket ausgeliefert.
Unveränderte alte SVG-Standardpakete erhalten beim Serverstart eine neue Version;
eigene Pakete bleiben unverändert. Nach dem Sync zeigt die Sidebar den zugewiesenen
Avatar, bis dahin „Avatar wartet auf Sync“. PNG-Animationen eigener Pakete bleiben
unterstützt; Standard-Avatare sind statische Bilder.

**Flugradar:** Rechts zeigt eine 112×112-Ansicht die gemeldeten Flugzeugpositionen
um den Gerätestandort (`aircraft.center` aus der Server-API), Nord oben. Der äußere
Ring entspricht `radiusNm`, der innere der halben Entfernung. Weiß markiert den
Standort, Gelb das links ausgewählte Flugzeug. Symbole zeigen die gemeldete
Flugrichtung; bei fehlender Richtung erscheint ein Punkt. Rechts LINKS/RECHTS
wechselt die Auswahl. Es ist eine Positionsansicht ohne Straßenkarte; Daten
entsprechen dem angezeigten Abrufzeitpunkt, keine Live-Verfolgung. Dafür ebenfalls
den Homeserver aktualisieren.

**Wissen:** Lesen, Suche, Zufall und Artikelverweise. Im Suchfeld rechts LINKS/
RECHTS Buchstaben wählen, OBEN anhängen, UNTEN löschen, MITTE suchen. Text, Quelle,
Original-URL und Lizenzhinweis sind scrollbar. Offline bleiben letzter Artikel
und letzte Suchergebnisse erhalten; neue Suchanfragen erfordern den Server.

**Quiz:** Altersfilter auch bei Paketkatalogen, zufällige Antwortpositionen,
Richtig/Falsch-Rückmeldung und Erklärung. Die Serverantwort an Index 0 bleibt die
richtige. Rechts LINKS öffnet die ganze Frage, Rechts RECHTS die ausgewählte Antwort
zum Lesen mit UP/DOWN; MITTE kehrt zur Auswahl zurück. Nach dem Beantworten
UP/DOWN für die Erklärung. Maximal 200 lokal zusammengestellte Fragen.

**Spiele:** `hot_potato` = 15-Sekunden-Weitergabe-/Tastenspiel,
`simon_motion` = Richtungsfolge merken und durch Kippen/Schalter nachspielen,
`tilt_maze` = Kipp-Labyrinth mit zusätzlicher Schaltersteuerung. Bei Hot Potato
zählt auch eine Schüttelbewegung. Die IMU wird mit 50 Hz gelesen, inklusive
Neutralstellungserkennung für Simon. Bei I²C-Ausfall bleiben die Schalter nutzbar.
Bewusste lokale Spielregeln für die Server-IDs; kein vernetzter Spielzustand.

**Chill:** Bei zugeordnetem Chill-Paket wird „Ruhezeit“ ergänzt. Atemanimation,
Paketvorschau und Start/Stopp des ersten ausgewählten WAV-Sounds. Der aktuelle
Server hat noch keine eigenständige Chill-Seitenkonfiguration.

## Offline, Assets und Speicher

16 MiB Flash: NVS 20 KiB, OTA-Daten 8 KiB, **zwei App-Slots à 4 MiB**,
**LittleFS 8064 KiB**, Coredump 64 KiB. `spiffs` ist die Partition-Subtype-Bezeichnung;
verwendet wird ausschließlich LittleFS.

- Zwei JSON-Snapshots mit atomarem NVS-Auswahlzeiger; bei ungültigem aktuellen
  Snapshot wird der andere geprüft. Schreiben überschreibt nie den aktiven Slot.
- Konfiguration wird vor optionalen Downloads übernommen. Vorhandene Assets bleiben
  bis zur vollständig validierten Aktivierung verfügbar.
- Paketdateien werden unter ihrem SHA-256 gespeichert, Manifeste pro ID/Version.
  Nur `assetUpdates` aus dem Geräteplan werden geladen, vorhandene identische
  Dateien werden nach Hash-Prüfung wiederverwendet.
- Größe, Hash, Paketidentität, sichere Pfade, Definition und Referenzen werden
  geprüft. Alle benötigten Versionen werden vor dem Inventarwechsel validiert.
- Alte Paketversionen werden ausschließlich nach `boot_success` und konkreter
  serverseitiger `removeVersions`-Freigabe entfernt. Gemeinsame Blobs bleiben
  solange ein behaltenes Manifest sie referenziert.
- Fehlende Providerdaten lassen den bisherigen Snapshot unverändert. Wetter und
  Flugradar werden regelmäßig erneuert, auch wenn deren Versionszähler gleich sind.
- **Avatare ausschließlich PNG mit 80 × 80 Pixeln**, ohne Skalierung in der
  Sidebar. Animationen über `animations.idle`; anschließend `preview`, sofern
  PNG, oder der bestätigte Pfad `data/pet/idle/frame_01.png` (auch mit tatsächlichem
  `files/`-Präfix im Manifest unterstützt). Die Download-URL lautet normalerweise
  `/api/v1/packages/{id}/versions/{v}/files/data/pet/idle/frame_01.png`.
- SVG-Avatar-Vorschauen werden **weder übertragen noch gerendert**. Bei gemischten
  Avatarpaketen wird die PNG-/JSON-Geräterepräsentation derselben Paketversion
  installiert; nicht benötigte Webdateien bleiben ausschließlich auf dem Server.
  Ein reines SVG-Avatarpaket wird vor Dateiübertragung zurückgestellt. Im Homeserver
  das passende PNG-Paket zuweisen. Der konkrete Nutzerdatensatz lag hier nicht vor;
  Pfad und 80×80-PNG wurden zusätzlich mit einem Vertragstest geprüft.
- Andere Bildinhalte: PNG/JPEG bis 1024 × 1024, PNG ohne Interlacing gemäß Decoder.
  SVG, GIF und WebP werden nicht unterstützt.
- PCM-WAV, 16 Bit little-endian, mono/stereo, 8–48 kHz. MP3, GIF, WebP und beliebige
  Game-Engines werden nicht als unterstützt vorgetäuscht: Paket wird zurückgestellt.
- JSON-Antworten und einzelne JSON-Dateien maximal 256 KiB, 64 Pakete, 256 Dateien
  pro Paket, 128 KiB Sicherheitsreserve. Server-Upload-Limits sind größer als das
  Gerät. Pakete entsprechend klein halten; alte aktive Inhalte werden bei
  Platzmangel nicht gelöscht.

News-Bilder (bis zu 20 je Abruf) und Wissensbilder werden offline gecacht.
Die Wissens-Leseseite zeigt das Artikelbild rechts in einem 112×100-Pixel-Bereich
mit erhaltenem Seitenverhältnis. Ohne Bild nutzt der Text die ganze Breite. Generische Common-/Weather-/Game-
Paketdaten werden synchronisiert, besitzen aber ohne eindeutige Server-Metadaten
noch keine automatische Zuordnung zu jedem UI-Element. Details und konkrete
API-Ergänzungen: [Serverabgleich](docs/server-contract.md).

## OTA und Recovery

Der festgelegte Core 3.3.0 liefert `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1`.
`verifyRollbackLater()` verhindert seine sonst vorzeitige Arduino-Bestätigung.
Downloads werden blockweise in den inaktiven Slot geschrieben; Größe, SHA-256,
S3-Chip-ID und vollständige ESP-Image-Validierung müssen vor Boot-Auswahl bestehen.
Der Sync-Bezug wird vorher in NVS gesichert.

Erst lokale Speicher-/Asset-/Display-Initialisierung, passende Flash-/PSRAM-Größe,
Netzwerk-Task-Erzeugung und fünf Sekunden laufende UI bestätigen das Image.
**WLAN oder Serverzugang sind kein Selbsttest-Kriterium.** Der Loop-Watchdog löst
bei Hängen einen Neustart aus; ein unbestätigtes Image wird vom Bootloader verworfen.
Bestätigung bzw. Rollback wird bei erreichbarem Server nachgemeldet. Danach erfolgt
ein neuer Plan mit der tatsächlich laufenden Firmwareversion.

Ein frei konfiguriertes `LEAP_OTA_ENABLED=0` verhindert automatische App-Wechsel.
Eine App allein kann einen alten Bootloader oder falsche Partitionierung nicht
reparieren. Initialer USB-Upload der richtigen Basis ist zwingend. Bei defektem
Dateisystem bleibt USB-Recovery möglich; beide Mitteltasten formatieren nur nach
bewusster Haltefrist. Inhalte kommen danach erneut vom Homeserver.

Der Homeserver arbeitet im vertrauenswürdigen Heimnetz ohne Geräteauthentifizierung.
HTTPS wird mit konfiguriertem CA-Zertifikat unterstützt, niemals mit `setInsecure()`.
SHA-256 ist eine Integritätsprüfung, keine Firmware-Signatur. ESP-NOW-Broadcast
ist unverschlüsselt und ohne Identitätsnachweis; nur lokal bekannte Vorlagen-IDs
desselben Paketstands werden angezeigt. Kein Freitext, kein Relay, keine
Direktnachrichten. Alle Geräte/AP müssen auf demselben Funkkanal arbeiten.

## Struktur, Diagnose und Tests

- `LEAP/src/Hardware.h`, `Config.h`: Pins, Version, Grenzen.
- `Storage`, `Transport`, `Assets`, `Network`, `Ota`: persistente Daten und Sync.
- `Input`, `Ui`, `Media`, `Games`, `Audio`, `Radio`, `Motion`: lokale Gerätefunktionen.
- `Core.h`, `Protocol.h`: plattformunabhängig getestete Regeln.
- `tests/`, `.github/workflows/build.yml`: Tests und reproduzierbarer Arduino-Build.

Serial-Tags: `BOOT`, `STORE`, `DISPLAY`, `WIFI`, `HTTP`, `SYNC`, `ASSETS`, `RADIO`,
`AUDIO`, `IMU`, `OTA`, `HEALTH`. Keine WLAN-Passwörter oder Inhaltspayloads in Logs.
Min-Heap, freier PSRAM und Dateisystemplatz werden einmal pro Minute ausgegeben.
`wifiConnected=1` bedeutet verbunden (auch das frühere `wifi=1` war ein Boolean,
kein Arduino-WLAN-Statuscode). Beim Verbindungsaufbau erscheinen IP, RSSI und Kanal.
`SYNC` nennt fehlendes LittleFS, fehlgeschlagene Asset-Prüfung oder ausstehende lokale
Boot-Bestätigung als Blockade; `fsReady` und `bootConfirmed` stehen im Health-Log.
Bei `Corrupted dir pair` auf einer benutzten Partition bleibt die manuelle Recovery
nötig. Danach werden Konfiguration und Inhalte erneut vom Homeserver geladen.

```sh
ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src tools/test.sh
# Optionaler echter Serververtragstest, mit Homeserver-Python-Abhängigkeiten:
LEAP_HOMESERVER=/path/to/leap-homeserver python tests/server_contract.py /tmp/leap-fixtures.json
ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src tools/test.sh /tmp/leap-fixtures.json
```

Siehe [Architektur](docs/architecture.md), [Serververtrag](docs/server-contract.md),
[Hardware-Abnahme](docs/acceptance.md) und [Prüfergebnis](docs/validation.md).

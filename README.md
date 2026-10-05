# LEAP Firmware

Neue Arduino-Firmware für **ESP32-S3 N16R8**, abgestimmt auf
[`leap-homeserver`](https://github.com/coveur-codex/leap-homeserver), Stand `8175c9f`.
Version: `1.0.0-beta.17`. Keine Übernahme alter Firmware: Das Zielrepository war leer.

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
   `LEAP/build_opt.h` muss neben dem Sketch bleiben: Es vergrößert den PNGdec-
   Scanline-Puffer konsistent für Firmware und Bibliothek auf 1024-Pixel-RGBA.
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

## Startlogo

Ab beta.16 erscheint unmittelbar nach der Displayinitialisierung ein lokaler
LEAP-Schriftzug mit „Startet…“, bevor LittleFS, gespeicherte Inhalte und Funk
initialisiert werden. Das reduziert die schwarze Wartephase; es macht die
anschließende Speicherprüfung nicht überflüssig. `BOOT` protokolliert die Dauer
von Speicherinitialisierung, erstem Snapshot-Laden und UI-Start für die Abnahme.
Ab beta.17 wird der beim Setup geladene Zustand in die UI verschoben, statt ihn
noch zweimal zu kopieren. Die UI hält eine unveränderliche, gemeinsam besessene
Manifestansicht, die auch nach einem neuen Sync gültig bleibt. Das Paketlogo wird
vor der restlichen UI-Vorbereitung übertragen. Neue `STORE`-Zeitmessungen trennen
Dateilesen, JSON-Parsing, Inventarprüfung und RAM-Kopieren; reale Zeiten am Gerät
sind damit prüfbar, ohne die Integritätsprüfungen abzuschalten.


Ab beta.8 zeigt das Gerät beim Start **zwei Sekunden** lang
`bootscreen/leap-boot.png` aus der aktiven, lokal installierten Version des
Common-Pakets **`system`**. Vollbild ohne Sidebar, Seitenverhältnis bleibt erhalten.
Die Zeit beginnt erst nach Bildübertragung und Einschalten der Beleuchtung.
Netzwerk und Watchdog laufen währenddessen weiter; Tastendrücke verkürzen die
Anzeige nicht. Anschließend erscheint der normale Sperrbildschirm.

Fehlt das Paket/Bild oder kann es nicht dekodiert werden, startet direkt die
normale Oberfläche. Nach der ersten erfolgreichen Paketinstallation erscheint
das Logo beim nächsten Neustart. Es wird nicht bei jedem Sync erneut eingeblendet.

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
Avatar, bis dahin „Avatar wartet auf Sync“. Die Sidebar zeigt pro Seite das
80×80-PNG aus `data/pet/pagestatics/`: `home`, `news`, `weather`, `flightradar`,
`quiz`, `games`, `communication`, `knowledge` oder `settings`. Die interne Seite
`aircraft` verwendet `flightradar.png`; Menü und Sperrbildschirm verwenden
`home.png`. Pfade mit einem äußeren Uploadordner werden ebenfalls erkannt.
Fehlt das Seitenbild, wird `home.png` verwendet, danach der erste Idle-Frame
bzw. die PNG-Vorschau eines alten Pakets, jeweils ohne Animation. Das Haustierspiel
verwendet weiterhin seine animierten Zustände. Die vorhandene Manifest-/Datei-API
des Homeservers reicht aus; eine Serveränderung ist dafür nicht erforderlich.

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

**Quiz:** Die Seite beginnt mit einer Katalogauswahl (rechts OBEN/UNTEN,
MITTE zum Starten). Danach erscheinen ausschließlich altersgerechte Fragen aus
diesem Katalog. Bei mehr als 200 Fragen wird eine Zufallsstichprobe dieses Katalogs
gezogen; die Fragen und die vier Antwortpositionen werden gemischt. Nach einem
Durchlauf wird erneut gemischt. Ein unveränderter Hintergrund-Sync erhält die
aktuelle Frage und Auswahl. Rechts LINKS öffnet die ganze Frage, RECHTS die
gewählte Antwort zum Lesen mit OBEN/UNTEN; MITTE schließt die Detailansicht.
Nach dem Beantworten: OBEN/UNTEN für die Erklärung, LINKS/RECHTS für die nächste
Frage, MITTE zurück zur Katalogauswahl.

**Mathe-Quiz:** Steht zusätzlich in der Katalogauswahl bereit und erzeugt jede
Aufgabe lokal zufällig, auch offline. Im Homeserver unter Geräte → Quiz →
Mathe-Quiz werden Rechenart und Grenze je Gerät eingestellt: Addition oder
Subtraktion im Zahlenbereich 0 bis zur Grenze (3–1000), Multiplikation mit
Faktoren 1 bis zur Grenze (3–20; 10 für das kleine Einmaleins).
Vier verschiedene Antworten mit genau einem richtigen Ergebnis; nach der
Antwort folgen ein Rechenweg und eine scrollbare Stellenwerttafel mit
Tausendern, Hundertern, Zehnern und Einern. Ohne neue Serverkonfiguration gilt
Addition bis 20. Das neue Quiz benötigt keine Aufgabendatei.

**Neue Spiele (beta.10):** Im vorhandenen Spielebereich stehen „Mein Haustier“
(`tamagotchi`) und `snake` zusätzlich zur Verfügung. Dafür auch den Homeserver
aktualisieren, der die Spiel-IDs in der Geräte-Konfiguration anbietet.

Das Haustier nutzt das gewählte Avatarpaket. Links bleibt die 86-Pixel-Navigation,
in der Mitte Hintergrund (256×142) und Tier (80×80), rechts vier Aktionen.
Rechts UP/DOWN wählen, CENTER bestätigt: Füttern, Spielen, Waschen, Schlafen.
Die Aktion dauert optisch drei Sekunden, Frames mindestens 250 ms (Standard 400 ms).
Statusmeldungen und vier farbige Balken zeigen Sättigung, Spaß, Sauberkeit und
Energie. Die vier Bedürfnisse starten bei 85, sinken alle 30 Sekunden um 1–2
Punkte und nie unter 35. Ohne Pflege wird das Tier nach etwa acht Betriebsminuten
hungrig; der niedrigste Wert bestimmt das dringendste Bedürfnis. Kleine Hüpf- und
Spielbewegungen ergänzen die Paketanimationen. Schlafen zeigt die Nachtszene.
Während stromloser Zeit gibt es keinen Abzug. NVS speichert Aktionen und Änderungen
beim Verlassen sofort, passive Änderungen ansonsten gebündelt alle fünf Minuten.
Avatarwechsel wechselt die Grafik, behält aber das Haustier.
Tag-/Nachtbilder aus dem Manifest haben Vorrang. Solange ein Bild fehlt oder lädt,
zeichnet die Firmware eine Landschaft mit Sonne/Mond. Fehlgeschlagene Bildladevorgänge
werden nach zwei Sekunden erneut versucht, damit ein vorübergehender Fehler den
Hintergrund nicht dauerhaft ausblendet.
Tageshintergrund 07:00–19:59, Nachthintergrund 20:00–06:59 gemäß der bestehenden
Geräte-Zeitzone; ohne bekannte Uhrzeit Tag. Fehlende Grafiken verhindern das
Spielen nicht; vorhandenes Avatarbild oder einfache Zeichnung dient als Ersatz.

Snake: 40×13 Felder mit 8-Pixel-Zellen. Vor jeder Runde rechts UP/DOWN die
Geschwindigkeit wählen: Langsam (450 ms), Mittel (300 ms), Schnell (220 ms);
CENTER startet. Langsam ist vorausgewählt. Drei Futterpunkte liegen gleichzeitig
auf verschiedenen freien Feldern; gefressenes Futter wird einzeln ersetzt. Bei
weniger als drei freien Feldern sinkt die Futteranzahl entsprechend. Rechter Schalter
steuert; unmittelbare Gegenrichtung und weitere Richtungswechsel vor dem nächsten
Schritt werden ignoriert. Punkte und Rekord stehen oberhalb des Feldes.
Wand/eigener Körper beendet die Runde; rechts CENTER startet neu.
Jeder neue Rekord wird sofort in NVS gespeichert. Links CENTER verlässt jedes
Spiel, Links LEFT/RIGHT wechselt weiterhin die Seite. Sperren und Seitenwechsel
beenden die laufende Runde. Das Haustier bleibt gespeichert.

**Vier Gewinnt (beta.15):** Neues Spiel `connect_four` auf der Spieleseite.
Bei jedem Spielbeginn rechts UP/DOWN zwischen Leicht, Mittel und Schwer wählen,
CENTER startet. Du beginnst mit Gelb, das Gerät spielt Rot. Rechts LEFT/RIGHT
wählt eine der sieben Spalten, CENTER wirft den Stein ein. Vier Steine waagerecht,
senkrecht oder diagonal gewinnen; ein volles Feld ohne Gewinner ist unentschieden.
Volle Spalten verbrauchen keinen Zug. Während des Gegnerzugs werden Spieleingaben
ignoriert. Nach Spielende öffnet rechts CENTER wieder die Schwierigkeitswahl.
Links CENTER verlässt das Spiel; Seitenwechsel und Sperren beenden die Runde.
Leicht spielt zufällig, Mittel gewinnt/blockiert direkte Vierer und plant zwei
Halbzüge, Schwer sucht bis zu sechs Halbzüge mit Alpha-Beta und begrenztem Aufwand.
Das Spiel benötigt weder WLAN noch Assets. Auch den Homeserver aktualisieren,
damit die neue ID beim nächsten Sync in `config.games` angeboten wird.

**Spiele:** `hot_potato` = 15-Sekunden-Weitergabe-/Tastenspiel,
`simon_motion` = Richtungsfolge merken und durch Kippen/Schalter nachspielen,
`tilt_maze` = zufällig erzeugtes, zusammenhängendes 9×9-Kipp-Labyrinth mit
zusätzlicher Schaltersteuerung. Jede neue Runde generiert ein neues Layout;
Start oben links, Ziel unten rechts, alle Gänge erreichbar. Bei Hot Potato
zählt auch eine Schüttelbewegung. Die IMU wird mit 50 Hz gelesen, inklusive
Neutralstellungserkennung für Simon. Bei I²C-Ausfall bleiben die Schalter nutzbar.
Bewusste lokale Spielregeln für die Server-IDs; kein vernetzter Spielzustand.

**Chill (beta.19):** Homeserver → Gerät → Firmware & Inhalte → eine Chill-Szene
(Weltraum, Lagerfeuer, Schnee) auswählen. Nach dem regulären Sync im Hauptmenü
„Ruhezeit“ starten. Ganze 428×142-Fläche ohne Sidebar/Texte; MITTE an einem der
beiden Schalter zurück zum Hauptmenü, rechts LINKS/UNTEN bzw. RECHTS/OBEN blenden den
Slider ein und ändern ihn in Fünferschritten (0–100). Nach vier Sekunden verschwindet
der Slider. Der Wert bleibt je Szene lokal gespeichert. Dimmung und automatische
Sperre pausieren während der Szene; links MITTE lange drücken sperrt weiterhin.
Weltraum: prozedurale Parallax-Sterne und gelegentlich ein Paket-Sprite. Feuer: sechs
echte Flammenframes, Holz/Glut, wenige ruhige Funken. Schnee: Landschafts-Sprites und
maximal 72 sanft driftende Flocken. Details: [Chill](docs/chill.md).
## Offline, Assets und Speicher

16 MiB Flash: NVS 20 KiB, OTA-Daten 8 KiB, **zwei App-Slots à 4 MiB**,
**LittleFS 8064 KiB**, Coredump 64 KiB. `spiffs` ist die Partition-Subtype-Bezeichnung;
verwendet wird ausschließlich LittleFS.

- Zwei JSON-Snapshots mit atomarem NVS-Auswahlzeiger; bei ungültigem aktuellen
  Snapshot wird der andere geprüft. Schreiben überschreibt nie den aktiven Slot.
  Ab beta.16 hält Storage den aktiven Zustand samt Manifesten zusätzlich im RAM.
  Die UI kopiert diesen Zustand ohne Flash-Lesen oder erneute Paketprüfung.
  Neue Daten werden nach erfolgreicher Speicherung und NVS-Aktivierung unter einem
  kurzen Mutex veröffentlicht; Flash-Schreiben erfolgt außerhalb dieses Mutex
  in 2-KiB-Blöcken mit Task-Yield. Auch der Health-Log nutzt den im Hintergrund
  ermittelten freien Speicher statt einer LittleFS-Scan-Operation auf dem UI-Task.
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
- **Avatar-Tiere ausschließlich PNG mit 80 × 80 Pixeln**, ohne Skalierung in der
  Sidebar. Explizit unter `tamagotchi.backgrounds.day/night` referenzierte oder eindeutig
  als `background_day`/`background_night` benannte
  Erkannte Haustier-Hintergründe dürfen ab beta.17 beliebige PNG-Maße von
  1 bis 1024 Pixeln je Achse haben (z. B. 264×142); sie werden proportional
  in die 256×142-Spielfläche eingepasst. Tier-PNGs werden mit einer Alpha-Maske
  über den Hintergrund gezeichnet (Schwellwert 128). Animationen über `animations.idle`; anschließend `preview`, sofern
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
- HTTP-/Kontroll-JSON und Manifeste maximal 256 KiB; vollständige Quiz-Katalogdateien
  und kombinierte Offline-Snapshots jeweils maximal 1 MiB (beta.16). Der Homeserver
  liefert für installierte Quiz-Pakete nur Katalogmetadaten am Legacy-Endpunkt,
  sonst maximal 200 zufällig ausgewählte altersgerechte Fragen je Katalog. Die
  versionierten Pakete behalten alle Fragen. Dafür auch den Homeserver aktualisieren.
  Speicher-/HTTP-Fehler nennen Pfad, Größe, Limit und Prüfungsstufe. 64 Pakete, 256 Dateien
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

### Diagnose eines blockierten Asset-Syncs (beta.6)

`download_started` und `update_failed` enthalten jetzt `packageId` und `version`.
Die Fehlermeldung nennt Paket, gegebenenfalls Datei und konkrete Prüfungsstufe:
Manifest, Download (HTTP/Länge/Timeout/Schreibfehler/SHA-256), Bildmaße/Decoder,
WAV/JSON/Katalogschema oder Speicherung. Seriell erscheinen `ASSETS` und `DOWNLOAD`.
Ein bestandenes Paket wird als „verified and staged“ protokolliert; es ist damit
noch nicht aktiv. Die Firmware aktiviert weiterhin erst das vollständig geprüfte
Inventar. So kann ein Fehler in einem anderen Paket auch den Avatar zurückhalten.
Diese Version verbessert die Diagnose, ohne unbekannte Paketfehler zu übergehen.

### Watchdog, Eingaben und PNG-Transparenz ab beta.9

PNG/JPEG-Bilder werden während des Betriebs in einem eigenen `leap-media`-Task
dekodiert. Die UI wartet damit auch während der Asset-Prüfung im Netzwerk-Task
nicht mehr auf den gemeinsamen Decoder. Avataranimationen behalten währenddessen
den letzten fertigen Frame; Artikelbilder zeigen keine Bilder des vorherigen
Artikels. Das einmalige Startlogo wird weiterhin vor dem Netzwerkstart synchron
geladen und anschließend zwei Sekunden angezeigt.

Asset-Manifeste liegen für das Rendering im RAM, statt bei jedem Frame erneut
aus LittleFS geladen zu werden. JSON wird in Blöcken gelesen und geschrieben,
statt Dateisystemzugriffe für einzelne Zeichen auszulösen. Bei einer laufenden
Snapshot-Speicherung behält die UI ihren Zustand und versucht das Nachladen im
nächsten Durchlauf erneut. Der Watchdog bleibt aktiv, mit 30 Sekunden Timeout.

Ein eigener Tasten-Task liest alle zehn GPIOs alle 5 ms und puffert Ereignisse,
auch während eines Displaytransfers. Entprellung (25 ms), Wiederholung und
Long-Press bleiben erhalten. Eingaben fordern einen früheren Frame an; die
zusätzliche Wartezeit des bisherigen festen 100-ms-Renderintervalls entfällt.

PNG-Alpha wird gegen die jeweilige Hintergrundfarbe verrechnet: beim Avatar
gegen die Sidebar, bei Inhaltsbildern gegen die Seitenfarbe und beim Startlogo
gegen Schwarz. Halbtransparente RGBA-Kanten bleiben erhalten; deckendes Schwarz
bleibt Schwarz. Die Farbkonvertierung berücksichtigt PNGdec 1.1.6s
Hintergrundformat `0x00BBGGRR`. Bereits in der Bilddatei deckend gespeicherte
schwarze Flächen können dadurch nicht transparent werden.

### PNG-Decodierung ab beta.7

PNGdec 1.1.6 reserviert standardmäßig nur Platz für zwei Zeilen eines etwa
320 Pixel breiten RGBA-Bildes. Größere PNGs konnten beim Dekodieren den internen
Puffer überschreiben, obwohl die Firmware sie als zulässig einstufte. Ein
428×142-RGBA-Testbild reproduziert den Fehler. `LEAP/build_opt.h` setzt nun global
`PNG_MAX_BUFFERED_PIXELS=8768`: zwei 1024-Pixel-RGBA-Zeilen inklusive Alignment
und optionaler Palette. Nur ein lokales `#define` in `Media.cpp` wäre falsch,
weil Bibliothek und Aufrufer dann unterschiedliche Objektgrößen hätten.

Der Test `ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src PNGDEC_SRC=/path/to/PNGdec/src tools/test_png.sh` verwendet die echte
Bibliothek und prüft alle Pixel von RGB/RGBA-Bildern mit 80, 428 und 1024 Pixeln
Breite. Defekte PNG-Header und Interlacing werden weiterhin abgelehnt. Es wird
kein Paket übersprungen und keine Integritätsprüfung abgeschaltet.

### Kinderfreundliches Flugradar ab beta.13

Ausgeschriebene Flugzeugtypen und bekannte Start-/Zielflughäfen kommen vom
Homeserver. Die Seite zeigt km, m und km/h. Radar und Entfernung werden alle
zwei Sekunden aus Tempo und Flugrichtung geschätzt; bei sichtbarer Seite holt
der Netzwerktask alle 30 Sekunden neue Daten in den RAM. Nach zwei Minuten
endet die Fortschreibung. Alte und geschätzte Positionen sind gekennzeichnet.
Für Namen und verlässliches Cache-Alter wird die passende Homeserver-Änderung
benötigt. Weitere Details: [Serververtrag](docs/server-contract.md).

### Wetter mit Icons und Morgen-Vorschau ab beta.14

Die Wetterseite zeigt „Jetzt“ und „Morgen“ neben dem bestehenden Regenradar.
Sonne/Mond, Wolken, Nebel, Nieselregen, Regen/Eisregen, Schnee, Schauer und
Gewitter/Hagel werden direkt aus Kreisen, Linien, Rechtecken und Dreiecken
gezeichnet. Sie brauchen keine Bildpakete und funktionieren offline.
Die Morgen-Karte zeigt Datum, Wetterlage, Temperaturspanne und
Regenwahrscheinlichkeit. Fehlende Werte bleiben „?“, alte Wetterdaten sind
gekennzeichnet. Ein älterer Homeserver ohne `tomorrow` zeigt „Vorhersage fehlt“.

## Lokaler Kücheneditor

„Meine Kueche“ steht als eingebautes Spiel in der Spieleauswahl zur Verfügung.
24 Rasterplätze mit horizontalem Scrollen, 22 detailliert gezeichnete Objekte
inklusive Esstisch und Stuhl, drei Ebenen und vier Farben lassen sich frei einrichten
und werden automatisch lokal gespeichert. Links wird der Aktionsmodus gewählt,
rechts der Ort gesteuert und die Aktion bestätigt. Alte Küchen werden übernommen. Links-Mitte zwei Sekunden
halten oder „Zurueck“ auswählen führt zur Spieleauswahl. „Kueche leeren“ setzt die
Küche nach Bestätigung zurück. Bedienung, Platzierungsregeln
und Hardware-Prüfung: [Kücheneditor](docs/kitchen.md).

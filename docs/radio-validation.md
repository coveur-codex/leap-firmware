# Radio 1.6.0 – Build und Abnahme

Vergleich mit unverändertem Firmware-`main` **98c65ee** (1.5.2), durchgeführt
am 13.11.2026. Beide universellen Images wurden mit Arduino CLI 1.3.1,
Arduino-ESP32 3.3.0, ArduinoJson 7.4.2, Arduino GFX 1.6.3, PNGdec 1.1.6,
JPEGDEC 1.8.2 und WebSockets 2.7.0 gebaut. FQBN:

```text
esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=custom
```

| Größe | 1.5.2 | 1.6.0 | Zusätzlich |
|---|---:|---:|---:|
| App-Binary (`LEAP.ino.bin`) | 1.746.352 B | 1.774.976 B | **28.624 B (27,95 KiB)** |
| Programmbelegung laut Compiler | 1.746.203 B | 1.774.835 B | 28.632 B |
| Statische globale Variablen | 165.652 B | 165.700 B | **48 B** |

Beide Apps passen in die unveränderten **4-MiB-OTA-Slots**. Das Radio-Image
belegt rund 42,3 % eines Slots. `tools/check_image.py` bestätigt ESP32-S3,
Version 1.6.0, universelle NVS-Konfiguration und Slotgrenze. SHA-256:

```text
1edc07f2ed70bd21b822c0dd78df13f621731d7b4aedbf9ab1d4b9a4da0299d5
```

Der zusätzliche USB-Provisionierungsbuild besteht ebenfalls: App-Binary 1.775.584 Byte, Version 1.6.0, Provisionierungsmarker vorhanden und universeller
Marker abwesend. Dieses Image ist ausschließlich für Kabelinstallation gedacht.

Die Linkerzahlen enthalten keine dynamischen Task-Stacks oder Sitzungspuffer.
Zusätzlich werden 24 KiB interner Task-Stack und etwa 1 KiB Queue-Nutzdaten
reserviert. Während einer Sitzung werden **158.732 Bytes (155,0 KiB) PSRAM** für
PCM, MP3-Eingang, Decode-Ausgabe und Decoderzustand reserviert und anschließend
freigegeben. HTTP/TLS sowie Konfigurations-JSON kommen hinzu. Es liegt keine
Messung des tatsächlichen Heap-Maximums oder Stack-Minimums auf Hardware vor.

## Automatische Prüfungen

- `tools/test.sh`: native Firmware-Suite einschließlich MP3-Frame-Dekodierung
  mit Stereo-/Mono-Testsignalen, Redirects, HTTP-Chunking, ICY-Metadaten,
  URL-/Konfigurationsgrenzen und persistiertem Radio-Snapshot.
- Zusätzlicher Test führt den tatsächlichen ESP32-Radiozweig von `Audio.cpp`
  mit deterministischer PCM-Quelle aus: Lautstärke, Benachrichtigungsmischung,
  Sperre alter PCM-Blöcke bei Verlassen/Senderwechsel und anschließender
  Systemklang. Netzwerk/Decoder werden dabei bewusst separat geprüft.
- Bestehende WAV-, Synth-, Spiele-, Eingabe-, NVS- und OTA-Regressionen.
- `tools/test_png.sh` mit echtem PNGdec 1.1.6: Bild-/Avatar-Pfad unverändert.
- Homeserver: automatisierte Tests und Chromium-Formularprüfung für Sender-CRUD,
  Gerätefreigaben, Deaktivierung, Löschung, Sortierung und Schema-Migration.

Compilerwarnungen aus den vorhandenen PNG-/JPEG-/GFX-/WebSocket-Bibliotheken
und bestehenden ArduinoJson-Schleifen treten auch im Basisbuild auf. Die
optionalen USB-Discovery-Tools fehlen in der Cloud-Umgebung; das Kompilieren
und die Prüfung der erzeugten Images sind davon unabhängig.

## Noch am Zielgerät abzunehmen

In dieser Umgebung ist kein ESP32 mit MAX98357A angeschlossen. Daher sind
reale Stream-/WLAN-/TLS-Laufzeit, Lautsprecher, Eingabereaktionszeit unter Last
und Speicherhöchstwerte keine gemessenen Ergebnisse. Abnahmefolge:

1. Zwei direkte MP3-Sender pro Gerät freigeben, Radio öffnen, Start/Pufferstatus
   und Audio prüfen; zusätzlich ein Gerät mit anderer/keiner Freigabe verwenden.
2. Rechts links/rechts mehrfach während Verbindungsaufbau und Wiedergabe
   drücken, hoch/runter bis 0/100 %, Mitte zum erneuten Verbindungsaufbau.
3. Seite verlassen, Menü öffnen, Gerät sperren und schnell wieder öffnen:
   kein alter Stream, letzte weiterhin erlaubte Auswahl und Lautstärke erhalten.
4. Senderfreigabe entziehen, zentral deaktivieren/löschen, Radio-Seite abschalten
   und jeweils synchronisieren: Stop/Fallback/Seitenentfernung prüfen.
5. Homeserver ausfallen lassen, WLAN trennen und wiederherstellen, ungültigen
   Stream, AAC-/Playlist-Endpunkt und langsamen/abbrechenden Stream testen:
   kurzer Fehlerstatus, weiterhin bedienbare UI, erneuter Start über Mitte.
6. Öffentliche HTTPS-Kette, Redirect HTTP→HTTPS, ICY und einen Chunked-Stream
   prüfen; abgelaufenes Zertifikat muss abgewiesen werden.
7. Nach Radio Spiele, WAV, Benachrichtigungen und Ruhezeit prüfen. OTA während
   Radio anfordern: sofort stumm, keine Hintergrundwiedergabe beim Flashen.
8. Wiederholt wechseln und öffnen/schließen; freien internen Heap/PSRAM und
   Stack-High-Water-Mark protokollieren. Normaler Leerlauf sperrt nach den
   bestehenden drei Minuten auch die Radio-Seite und beendet die Wiedergabe.

## 1.6.1 – Verbindungsdiagnose

Gleiche Toolchain/FQBN wie oben. Universeller ESP32-S3-Build und Imageprüfung:
1.778.752 Bytes (gegenüber 1.6.0 zusätzlich 3.776 B); statische Variablen
165.708 Bytes (zusätzlich 8 B). Image-SHA-256:

```text
f7394b016c1a724cbc4771d65e84d9bd00a723d6baeedab41e612d01c77077e1
```

Neue native Tests führen den echten `Radio.cpp`-Worker mit deterministischen
DNS-/HTTP-/TLS-Fakes aus: HTTP 403/404/503, TLS-Fehlercode, DNS-Ausfall,
HTTP-Antwort nach 2,5 Sekunden, Timeout/Speichermangel, Redirects, Abbruch und
Ersetzen laufender Anfragen, URL-Token-Redaktion sowie Freigabe aller
Sitzungspuffer. Die bestehende native Suite bleibt unverändert Teil der Prüfung.
Die Prüfung ersetzt keinen erfolgreichen Verbindungsversuch auf dem ESP32.
Der vom Nutzer gemeldete WDR-Endpunkt konnte aus der Cloud-Netzwerkpolicy
heraus nicht kontaktiert werden; Ursache und Erfolg auf dem Zielgerät müssen
mit den neuen `[RADIO]`-Logs geprüft werden.

## 1.6.2 – Sync-Wiederaufnahme und HTTP-Diagnose

Universeller N16R8-Build: 1.781.520 B (+2.768 B gegenüber 1.6.1), statische
Variablen unverändert 165.708 B. ESP32-S3-, Versions-, NVS-Marker- und
4-MiB-Slotprüfung bestanden. SHA-256:

```text
fa12d9b6bc7bfa950f2e73568f3006cbb8c34c29692f977919340c98562149a9
```

Native Tests des tatsächlichen `Transport.cpp` prüfen erfolgreichen JSON-POST,
HTTP-/TLS-/Socket-/Größen-/Parsefehler, Zielhost/Port, Query-Redaktion, echte
Zeilenumbrüche und Diagnose vor Schließen des Clients. Die Wiederaufnahme-
Entscheidung wird für gleiche Firmware, USB-Versionswechsel, OTA-Plan und
abweichendes Inventar geprüft. Der Test `tests/server_contract.py` verwendet die
echten Homeserver-Routen und bestätigt: Ein Check-in aktualisiert die laufende
Version bei noch offenem altem Sync, bestätigt keine OTA-Firmware und gibt keine
Dateilöschung frei. Ein falsches boot_success zum alten Plan wird abgewiesen,
ohne die gemeldete aktuelle Firmware zurückzusetzen.

Eine Ursache der beobachteten TCP-/TLS-Verbindungsfehler am Zielgerät ist damit
noch nicht bestätigt. Bei in 1.6.0 blockiertem OTA ist die erste Installation
von 1.6.2 per USB erforderlich; erst danach können die neuen HTTP-/Check-in-Logs
zur Prüfung der Verbindung verwendet werden.

## 1.6.3 – Interner Heap statt Decoder-BSS

Identische gepinnte N16R8-Toolchain, Vergleich zu 1.6.2:

| Größe | 1.6.2 | 1.6.3 | Differenz |
|---|---:|---:|---:|
| Universelle App-Binary | 1.781.520 B | 1.782.272 B | +752 B |
| Statische globale Variablen | 165.708 B | 96.020 B | **−69.688 B** |

Im ELF sind `leap::png` und `leap::jpeg` nur noch je 4-Byte-Besitzerzeiger;
vorher enthielten sie 51.808 B bzw. 17.884 B Decoderzustand im internen BSS.
Die Zustände werden jetzt in PSRAM initialisiert. Zusätzlich liegen 16.640 B
Chat-Empfangsqueue-Nutzdaten samt Queue-Verwaltung in PSRAM. Dies verschiebt
mindestens **86.328 B (84,3 KiB)** aus dem internen RAM. Ein HTTP-Request erzeugt
keinen TLS-Client mehr. Die konkrete Heap-Spitze und der größte freie Block
nach einem erfolgreichen TLS-Aufbau müssen auf dem Gerät gemessen werden.

Imageprüfung (ESP32-S3, 1.6.3, universelle NVS-Konfiguration, 4-MiB-Slot) besteht.
SHA-256:

```text
12925d4c2ce1346649885b7395475464dd5a3e3b92d791f566838bfa5c436435
```

Prüfungen:

- Vollständige native Suite, insbesondere Chat-Zustellung/Backpressure sowie
  Transport- und Radio-Pfade. Der Chat-Test kompiliert den ESP32-Caps-Zweig,
  prüft Queue-Kapazität/-Speicherflags und die passende Freigabe bei Taskfehler.
- Tatsächlicher Transport: TLS-Client wird für HTTPS erzeugt/freigegeben und bei
  nachfolgendem HTTP-Request nicht neu angelegt.
- `tools/test_png.sh` mit echten PNGdec- und JPEGDEC-Quellen: Decoder-Arbeits-
  bereiche explizit in PSRAM, Fehler der ersten/zweiten Allokation, Aufräumen,
  einmalige Initialisierung, transparente PNG-Szenen, RGB565-JPEG und Bildwechsel.
- CI führt dieselbe Bildprüfung mit beiden bereits gepinnten Bibliotheken aus.

Die Änderungen adressieren den im Gerätelog belegten kritischen Heap-Engpass.
Eine erfolgreiche Verbindung zum echten Sender oder Homeserver ist ohne das
Zielgerät weiterhin kein gemessenes Ergebnis.

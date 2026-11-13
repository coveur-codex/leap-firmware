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

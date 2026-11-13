# Radio ab 1.6.0

Die Seite `radio` wird über `config.pages` aktiviert. Rechts bleibt die bisherige
Hauptfläche, links bleiben Avatar und Statusleiste erhalten. Rechts links/rechts
wechselt den Sender und startet ihn direkt, hoch/runter ändert die gemeinsame
Lautstärke um 5 Prozentpunkte. Mitte startet den gewählten Sender erneut, z. B.
nach einem WLAN- oder Streamfehler. Auswahl und Lautstärke verwenden die bestehende
UI-NVS-Persistenz (gebündeltes Speichern nach fünf Sekunden).

## Konfiguration und Lebenszyklus

Der Homeserver liefert `radioStations` als Teil von `/devices/{id}/config`:

```json
{"radioStations":[{"id":42,"name":"Mein Radio","url":"https://example.org/live.mp3"}]}
```

Maximal 32 Einträge, positive eindeutige IDs, bis zu 120 UTF-8-Bytes je Name und
1000 Bytes je URL. Keine eingebauten Sender. Alte Konfigurationen ohne das Feld
bleiben gültig. Der bestehende atomare LittleFS-Snapshot speichert auch die
Sender, ohne einen separaten Abruf oder Cache. Beim Anwenden einer Konfiguration
bleiben Seitenidentität und Sender-ID erhalten; ein entfernter Sender wird durch
den ersten verfügbaren ersetzt. Bei deaktivierter Seite oder leerer Auswahl
endet die Wiedergabe.

`Ui::syncRadio` wird nach Eingaben, Snapshot-Wechseln und der automatischen Sperre
aufgerufen. Nur eine tatsächlich sichtbare Radio-Seite darf einen Stream
anfordern. Menü, Sperre, andere Seiten und OTA invalidieren die Sitzung sofort.
Ein Generationszähler verhindert Audio aus alten Verbindungen und gepufferten
Sendern, auch beim schnellen Wiederöffnen. Netzwerkaufrufe werden ausschließlich
im Radio-Task ausgeführt; ihr Abbruch und die Ressourcenfreigabe folgen nach dem
laufenden, zeitlich begrenzten Aufruf. Die UI wartet nicht auf DNS/TCP/TLS.

## Audio und Transport

- Vendored **minimp3**, Commit `ea99364f61c14656440e8d77e9c233ccf3124633`,
  CC0; `MINIMP3_ONLY_MP3`, `MINIMP3_NO_SIMD`. Nur MPEG Layer III.
- HTTP/HTTPS mit verifizierten Zertifikaten aus dem vorhandenen ESP-IDF-Mozilla-
  Bundle und Hostnamenprüfung; kein `setInsecure`. Ungültige Gerätezeit kann
  HTTPS verhindern. Keine Anmeldung oder individuellen Stream-Zertifikate.
- Bis zu fünf Weiterleitungen, einschließlich HTTP/HTTPS-Wechsel, relative
  Ziele und protokollrelative Ziele. HTTP/1.0 wird bevorzugt; Chunked-Bodies
  werden inkrementell entpackt. ICY-Metadaten werden übersprungen, nicht angezeigt.
- MIME-Prüfung plus tatsächliche MP3-Frame-Erkennung. Playlists, HLS, AAC, Ogg,
  FLAC und WAV sind nicht Teil dieser Erweiterung. Abweichende Samplerate im
  laufenden Sender wird mit Formatfehler beendet; mono/stereo bis 48 kHz.
- Ab 1.6.1: Verbindungs-Timeout 8 s, TLS-Handshake 10 s, Header-Lese-Timeout 8 s,
  Daten-/Decoder-Fortschrittslimit 8 s. DNS und langsam eintreffende Header können
  die Freigabe verzögern, bleiben aber vom UI-Task getrennt.
- Separater Netzwerk-/Decoder-Task und 128 KiB Stereo-PCM-Ring in PSRAM;
  halber Ring vor Erststart/Wiederanlauf (ca. 372 ms bei 44,1 kHz).
- Ausschließlich der vorhandene Audio-Task besitzt den I²S-Kanal und die
  unveränderten MAX98357A-Pins. Er liest höchstens 128 Stereo-Frames je Aufruf,
  skaliert die Lautstärke und mischt Benachrichtigungstöne hinzu. Die bestehende
  WAV-/Spiel-/Ruhezeit-Ausgabe bleibt nach Verlassen der Radio-Seite verfügbar.

## Speicher und Prüfung

Feste Task-Stack-Reservierung 24 KiB plus Queue/Mutex (ca. 1 KiB Nutzdaten).
Pro aktiver Sitzung: 128 KiB PCM + 16 KiB Eingang + 4,5 KiB Decode-Ausgabe +
6668 Bytes Decoderzustand = **155,0 KiB PSRAM**, zusätzlich dynamische HTTP-/TLS-
Puffer und Konfigurations-JSON. minimp3 benötigt ca. 16 KiB temporären Stack,
der in der 24-KiB-Taskreservierung enthalten ist. Alle Sitzungspuffer werden beim
Beenden freigegeben. Das ist ein Quellcode-Budget, keine gemessene Heap-Spitze.

Native Tests prüfen Chunk-/ICY-Framing, Weiterleitungen, Konfigurationsgrenzen,
Snapshot-Persistenz und echte Mono-/Stereo-MP3-Dekodierung. Die bestehenden
WAV-/Synth-/Benachrichtigungs-, Spiele- und OTA-Tests bleiben Teil von
`tools/test.sh`. Build-Vergleich und Hardware-Abnahme siehe `radio-validation.md`.

## Verbindungsdiagnose ab 1.6.1

Bei Problemen Firmware 1.6.1 installieren, seriellen Monitor mit 115200 Baud
öffnen und auf der Radio-Seite mit der rechten Mitte einmal neu verbinden.
Die eigenen `[RADIO]`-Logs erscheinen auch bei Arduino **Core Debug Level: None**.

Die Ausgabe enthält DNS-Adresse und Dauer, HTTP-Status bzw. negativen
HTTPClient-Code, ggf. den ursprünglichen mbedTLS-Fehlertext, Unix-Zeit, freien
Heap samt größtem internen Block, PSRAM und einen Sitzungszähler. Erfolgreiche
Wiedergabe ergänzt Samplerate, Kanäle, Bitrate und den freien Task-Stack;
Abbrüche ergänzen Ursache, Byte- und Frame-Zähler. Pro Verbindung werden nur
wenige Zeilen ausgegeben, nicht jedes Audiopaket. Pfade und URL-Parameter
werden ausgelassen, damit signierte Stream-URLs keine Token in Logs hinterlassen.

| Anzeige / Log | Bedeutung / nächster Schritt |
|---|---|
| `DNS fehlgeschlagen` / `DNS failed` | Hostname konnte am Gerät nicht aufgelöst werden; WLAN/DNS prüfen. |
| `HTTP-Fehler 403`, `404`, `503` … | Server hat geantwortet; Zugriff, direkten Stream-Endpunkt oder Senderausfall prüfen. |
| `TLS-Verbindung fehlgeschlagen` / `TLS=… detail=…` | Fehlercode/Text unterscheidet z. B. Zertifikat, Handshake oder Speicher. Unix-Zeit mitprüfen. |
| `HTTP=-11` | Zeitlimit beim Warten auf die HTTP-Antwort überschritten. |
| `HTTP=-1`, kein TLS-Fehler | HTTPClient konnte keine Verbindung aufbauen; das SDK fasst mehrere TCP-Ursachen so zusammen. |
| `peer-closed`, `body-timeout` | Verbindung nach der Antwort geschlossen bzw. keine weiteren Stream-Daten. |
| `no-mp3-frames`, `unsupported MIME` | Keine unterstützten MP3-Daten empfangen. |

Die zusätzlichen Zeitbudgets verhindern vorschnelle Abbrüche auf langsameren
Endpunkten. Sie sind kein Nachweis für die Ursache eines konkreten Gerätefehlers.
Ein laufender DNS-/TCP-/TLS-Aufruf kann das endgültige Schließen einer Verbindung
verzögern; Audio wird beim Verlassen der Seite weiterhin sofort invalidiert.
Zertifikats- und Hostnamenprüfung bleiben aktiv. Einen Radio-Stream aus einer
Cloud-Umgebung zu erreichen, beweist zudem keine Erreichbarkeit aus dem WLAN
des Geräts; der gemeldete WDR-Endpunkt war hier durch die Cloud-Netzwerkpolicy
nicht prüfbar.

## Wenn auch Homeserver-Sync oder OTA scheitert (1.6.2)

`[HTTP] status=-1 bytes=-1` ist ein negativer HTTPClient-Verbindungsfehler, keine
HTTP-Antwort und kein Beleg für eine zu große JSON-Datei. Arduino bezeichnet ihn
allgemein als „connection refused“; bei HTTPS kann auch ein TLS-Aufbaufehler
zugrunde liegen. Ein erfolgreicher GET dazwischen zeigt nur, dass mindestens
eine Anfrage funktioniert hat. Bis 1.6.1 waren Zielhost und Erfolgs-Pfad im Log
nicht erkennbar, obwohl der Server-Kandidat nach Fehlern wechseln konnte.

Ab 1.6.2 zeigen HTTP-Logs Host, Port, TLS, API-Pfad ohne Query, Laufzeit und bei
Fehlern Socket-/TLS-Diagnose sowie internen Heap. `[CHECKIN] running=1.6.2
reported=1 HTTP=200` bestätigt die Übertragung der Versionsmeldung. Der Check-in
läuft vor dem Wiederaufnehmen alter Pläne; ein falscher alter Plan darf nicht
den Versionsbericht blockieren. Ein Schema- oder Datenbankwechsel ist dafür
nicht nötig. Eine fehlgeschlagene Verbindung selbst kann damit noch nicht als
behoben gelten.

Ist ein Gerät noch auf 1.6.0 und erreicht wegen dieses Fehlers den OTA-Download
nicht mehr, muss 1.6.2 einmal per USB installiert werden. Die vorhandene NVS-
Gerätekonfiguration wird weiterverwendet. Anschließend Monitor mit 115200 Baud
öffnen, einmal synchronisieren und die `[CHECKIN]`-/`[HTTP]`-/`[SYNC]`-Zeilen
prüfen; für den Stream zusätzlich `[RADIO]`.

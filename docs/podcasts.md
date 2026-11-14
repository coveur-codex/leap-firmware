# Podcasts und gemeinsame Audiosteuerung · 1.7.0

Zuerst Homeserver 1.7.0 mit Migration 0025, danach Firmware 1.7.0 installieren.
Unter **Podcasts** RSS-Feeds anlegen und unter **Geräte → Podcasts** Kanäle
freigeben sowie die Seite aktivieren. Die Konfiguration läuft über den vorhandenen
Snapshot-/Asset-/OTA-Sync. Ohne Freigabe erscheint kein Kanal.

## Bedienung

| Taste | Radio | Podcast-Kanäle | Episodenliste | Podcast-Wiedergabe |
|---|---|---|---|---|
| Links Hoch/Runter | Lautstärke ±5 | Lautstärke ±5 | Lautstärke ±5 | Lautstärke ±5 |
| Links Links/Rechts | Vorige/nächste LEAP-Seite | gleich | gleich | gleich |
| Links Mitte | Menü, halten: sperren | gleich | gleich | gleich |
| Rechts Hoch/Runter | Nächster/voriger Sender | Voriger/nächster Kanal | Vorige/nächste Folge | Vorige/nächste Folge starten |
| Rechts Links | ohne Funktion | Zur vorhandenen Wiedergabe | Kanäle | 15 Sekunden zurück |
| Rechts Rechts | ohne Funktion | Kanal öffnen | Folge starten | 30 Sekunden vor |
| Rechts Mitte | Pause/Weiter, bei Fehler erneut verbinden | Kanal öffnen | Folge starten | Pause/Weiter, bei Fehler URL neu abrufen |
| Rechts Links halten | ohne Funktion | ohne Funktion | ohne Funktion | Episodenliste öffnen |

Der lange Links-Druck hat in der Wiedergabe eine eigene Freigabe im vorhandenen
GPIO-Sampler: kurz erst beim Loslassen, lang einmal nach 900 ms ohne kurze Aktion.
Die übrigen Seiten behalten Press-/Repeat-/Hold-Verhalten. Radio-Sender wechseln
zyklisch in der Homeserver-Reihenfolge; Podcasts verwenden die Feed-Sortierung
(neueste zuerst). Listen zeigen drei Einträge und laden jeweils zwölf nach.
Vorige/Nächste beim Hören wird online anhand der stabilen Episoden-ID bestimmt,
sodass neue Feed-Einträge die Auswahl nicht verschieben. Alle Server-Episoden
sind erreichbar. Leerer Kreis = ungehört, kleiner Balken =
teilweise gehört, Häkchen = vollständig gehört. Gehörte Folgen können erneut
gewählt werden und beginnen dann bei null; teilweise gehörte Folgen werden fortgesetzt.

Beide Seiten nutzen denselben vertikalen Balken bei x=91, y=25, 10×94 Pixeln.
Die Sidebar x=0…85 inklusive Avatar, Uhr und Status bleibt unverändert.
Es gibt einen Lautstärkewert im vorhandenen NVS, in 5er-Schritten von 0 bis 60 %.
Alte höhere Werte werden begrenzt. Gebündelte Speicherung nach fünf Sekunden;
wie bei den bisherigen Einstellungen kann ein sofortiger Stromausfall davor den
letzten Tastenschritt verlieren. Die tatsächliche Lautstärke ist am Lautsprecher
zu prüfen; die Begrenzung ist keine Messung des Schalldrucks.

## Wiedergabe und Lebenszyklus

Beim ersten Öffnen: Kanäle. Später: letzte gültige Folge an der gespeicherten
Position; online wird die Freigabe und aktuelle URL vor dem Start geprüft.
Bei regulärem Dateiende wird der Fortschritt als abgeschlossen gespeichert und
die nächste nicht abgeschlossene Folge desselben Kanals gesucht. Sind alle gehört,
endet die Wiedergabe. Springen in die letzten Sekunden gilt nicht als Abschluss. Eine so am Ende
gespeicherte teilweise gehörte Folge startet beim erneuten Öffnen wieder bei null.

Interne Ansichtswechsel unterbrechen die Wiedergabe nicht. Menü, Sperre,
Seitenwechsel oder Konfigurationsentzug invalidieren Audio sofort. Veraltete
Netzwerkantworten können es danach nicht erneut starten. Radio und Podcasts
teilen denselben vorhandenen `Radio`-MP3-Worker, minimp3-Decoder und I²S-Consumer;
es gibt keinen zweiten Decoder, I²S-Kanal oder konkurrierenden Audio-Task.
Die MAX98357A-Pins bleiben unverändert. OTA suspendiert den Worker und sichert
wartenden Fortschritt vor Neustart. Radio behält die 30-Minuten-Inaktivitätsfrist;
Podcasts laufen bis zum Folgenende oder einer bewussten Navigation weiter.
Die normale Abdunklung bleibt, automatische Sperre/Benachrichtigungsnavigation
unterbrechen eine sichtbare Audio-Seite nicht.

## Fortschritt und Offline-Betrieb

Der bestehende Network-Worker bearbeitet Metadaten und Fortschritt; UI-Handler
öffnen keine HTTP-Verbindung und schreiben kein Podcast-Journal. Änderungen
werden etwa alle 20 Sekunden, bei Pause/Folgenwechsel und beim Verlassen in
`/podcast-progress.json` atomar gespeichert. Unveränderte Positionen erzeugen
keine zusätzlichen Schreibvorgänge. Nach Neustart wird der lokale Stand geladen.
Uploads verwenden steigende Revisionen und können nach einem Verbindungsabbruch
wiederholt werden. Der lokale letzte Stand wird bei einem älteren Serverstand
bevorzugt und online erneut auf Verfügbarkeit geprüft.

Das begrenzte Journal hält 128 Episodenstände, darunter alle noch nicht
bestätigten. Bestätigte alte Stände dürfen verdrängt werden. 32 Metadaten-Dateien
mit je höchstens 64 KiB halten bereits besuchte Listen/Details bereit; im UI-RAM
liegt nur die aktuelle Liste. Offline sind bekannte MP3-URLs auswählbar, solange
sie beim Anbieter funktionieren. Unbekannte Listen lassen sich erst mit Server
laden. Auto-Weiter sucht offline nur in bekannten Listen. Der Server hält den
vollständigen Fortschritt ohne diese lokalen Cachegrenzen. Bei vollem Journal
bleiben unbestätigte Einträge erhalten, neue Änderungen warten mit Speicherhinweis.
Bei abruptem Stromverlust können etwa 20 Sekunden plus die Wartezeit auf den
Network-Worker fehlen.

## MP3 und Spulen

Positionen werden aus tatsächlich ausgegebenen Samples berechnet. Der Worker
hält für die aktuelle URL maximal 512 Frame-/Sample-Punkte im 30-Sekunden-Abstand.
Ein HTTP-Range-Sprung beginnt vor der Zielstelle mit zwei Sekunden Vorlauf für
den MP3-Bitreservoir; Samples vor dem Ziel werden verworfen. ETag/Last-Modified
und `If-Range` sichern die Wiederverwendung, Content-Range wird geprüft. Bei
ignoriertem Range, geändertem Validator oder fehlendem Validator wird korrekt
ab dem Anfang dekodiert und bis zur Zielposition übersprungen. Das ist bei
variabler Bitrate genauer als ein durchschnittlicher Byte-/Sekundenfaktor.

**Grenzen:** Der Suchindex ist nur im RAM. Der erste Resume nach Neustart oder
Sprünge in noch nicht durchlaufene Bereiche können entsprechend lange brauchen.
Fehlende/ignorierte Range-Unterstützung nutzt denselben langsameren Fallback;
die Oberfläche bleibt bedienbar und ein Seitenwechsel stoppt Audio sofort.
Sampleratenwechsel innerhalb einer Datei sind nicht unterstützt. Dauer kann
unbekannt sein (`?:??`); nach regulärem Ende wird sie aus Samples bestimmt.
Ein reguläres Ende erfordert einen vollständigen Content-Length-Body oder einen
korrekten Chunk-Abschluss. Bei ungerahmtem Verbindungsende wird vorsorglich kein
Abschluss gesetzt. MP3-Layer III, Mono/Stereo bis 48 kHz; kein AAC/Ogg/HLS/DRM
oder Konvertierung. Defekte Frames, Timeouts und unerreichbare/abgelaufene URLs
zeigen einen Status; Rechts Mitte lädt die URL erneut. HTTPS-Prüfung bleibt aktiv.

Tests und Speichervergleich: [podcasts-validation.md](podcasts-validation.md).

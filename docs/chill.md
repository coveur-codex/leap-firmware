# Chill V1 (1.0.0-beta.18)

Das NV3007 bleibt wie alle bestehenden Seiten im Querformat 428×142. Chill verwendet
den vorhandenen Canvas; keine zusätzliche Vollbildkopie. Nur der Zurück-Pfeil und
während Eingaben ein Slider bleiben sichtbar. Rechte Richtungstasten blenden den
Slider ein und ändern den generischen Wert 0–100 in Fünferschritten. Ausblenden nach
4 s. MITTE an jedem der beiden Schalter öffnet das Hauptmenü (ab beta.19); langes
Drücken der linken Mitteltaste sperrt. Die automatische Inaktivitätssperre/Dimmung
pausiert hier.

`Chill` übernimmt Paket-Sprites über die vorhandenen Manifest-/SHA-Blob-Daten und
`Media`-Worker. Pro Sprite/Frame wird ein kleiner dekodierter Bildpuffer gecacht,
mit Transparenz über die Szene gezeichnet und beim Verlassen freigegeben. Keine
Flash-Lesezugriffe im Renderpfad. Die sechs Flammenframes behalten jeweils ihren
Cache; Intensitätsänderungen lassen nur deren Zielgröße neu dekodieren.
Die reguläre Rendering-Rate bleibt 10 Hz; Eingaben können einen früheren Frame
anfordern. Partikelbewegung nutzt begrenzte Zeitdeltas, statt pro Frame feste
Schritte. Langsamere Transfers beschleunigen die Szene dadurch nicht.

`ChillMotion` enthält maximal 72 Partikel (etwa 1,2 KiB), ohne Bilddaten:

- space: 56 Sterne, zwei Größen und abgestufte Helligkeiten über Tiefe, Tempo 0–26 px/s;
  höchstens ein dekoratives Sprite gleichzeitig, 8 s Pause pro 40-s-Zyklus.
- fire: Holz und Glut statisch, Flammen 55–100 % ihrer Größe, 4–10 Frames/s;
  0–12 langsam aufsteigende Funken/Glutpunkte und dezentes pulsierendes Licht.
- snow: 0–72 Flocken, unterschiedliche Tiefe/Größe, etwa 2–19 px/s und leichte
  sinusförmige Seitwärtsbewegung; kein Schneesturm.

NVS-Namespace `leap-chill`, Schlüssel `space`, `fire`, `snow`. Speicherung nach
2 s ohne Wertänderung sowie sofort beim Verlassen/Wechseln/Sperren. Ein
unveränderter Hintergrund-Sync erhält Simulation, Slider und Bildcache.
Die drei V1-Pakete benötigen ungefähr 50–140 KiB dekodierte Sprite-Daten je
aktiver Szene (RGB565 plus Alpha-Maske), zusätzlich zum vorhandenen Canvas.

## Paketvertrag

Homeserver liefert den bestehenden versionierten Asset-Vertrag: `definition`,
`files`, SHA-256 und einzelne Datei-URLs; das Gerät lädt kein ZIP. `definition`
enthält `scene` (`space`/`fire`/`snow`), `slider` (min=0, max=100, default),
`sprite`-Beschreibungen unter `sprites` mit `role`, `size`, optional `x`/`y` und
entweder `file` oder einer expliziten `frames`-Liste. Vordergrund-Schneehügel
werden auf die Displaybreite skaliert. Animationen nutzen die vorhandenen
Flammenframes; Partikel entstehen ausschließlich in der Simulation.

Maximal 16 Sprites, 8 Frames pro Sprite, 32 Frames insgesamt, native Sprite-Maße
bis 142×142. Fehlende/ungültige Referenzen verhindern die Paketaktivierung.
Der Homeserver importiert auch die ursprüngliche `pattern`/Framezahl-Syntax und
übersetzt sie in explizite Referenzen. `minFirmware=1.0.0-beta.18` schützt ältere
Firmware. Paket-IDs sind `chill-space`, `chill-fire`, `chill-snow`.

Weitere Szenen: Renderer/Interpretation im gemeinsamen `Chill`/`ChillMotion`
ergänzen, Validatoren auf Server/Gerät erweitern und ein Paket mit demselben
Sprite-/Slidervertrag veröffentlichen. Keine Änderungen an Sync oder NVS-System
nötig. Bestehende serverfreigegebene Bereinigung alter Versionen bleibt erhalten.

## Prüfung am Gerät

Jede Szene bei 0/50/100 mindestens fünf Minuten beobachten; Slider-Ausblenden,
kurze Eingaben, Zurück, langes Sperren und Neustart prüfen. Pro Szene verschiedene
Werte speichern und nach Neustart vergleichen. Während einer laufenden Szene
regulären Sync auslösen, dann im Homeserver Szene wechseln; keine gleichzeitigen
Dekorationen im Weltraum und keine hektischen Partikel bei 100. Dies ergänzt
die nativen Motion-/NVS-/Pakettests, Browserprüfung und den ESP32-Cross-Build;
reale Display-Laufzeit/Framerate benötigt die Hardware.

## 1.1.0: Generative Musik und Fotos

`ChillMusic.h` trennt Profile, Komposition und den begrenzten `ChillSynth`.
Der vorhandene Audio-Task besitzt alle musikalischen Zustände; die UI sendet nur
atomare Profil-/Intensitätswerte und Bildereignisse. Acht Stimmen, kleine
Sinus-Wavetables mit Obertonvarianten, ADSR, individuelle Instrumentenpegel,
Übersteuerungsschutz und Gesamtlautstärke erzeugen Mono-16-Bit bei 22.050 Hz.
Für den vorhandenen Stereo-I2S-Vertrag wird Mono auf beide Slots kopiert. WAVs
behalten ihre bisherigen Abtastraten; der Synth läuft dann mit derselben Rate.
Benachrichtigungen und WAV-Jobs werden in jedem Audioblock berücksichtigt.
Der bestehende Audio-Task hat 6 KiB Stack; es entsteht kein weiterer Task.

Die Komposition verwendet diatonische Tonleitern (Weltraum in Moll), funktionale
Harmonie mit Dominant-/Tonika-Auflösung, kleine Melodieschritte, gezielte Pausen
und ein achtstufiges variierendes Motivgedächtnis. Mehrtaktige Harmonien entwickeln
sich im Weltraum besonders langsam. Zufallszustand und Motive werden fortlaufend
weiterentwickelt; es gibt keine Musikdatei, vorgegebene Trackdauer oder Loop-Grenze.
Profilwechsel blenden zuerst 0,7 s aus und danach 1,5 s ein; erst bei Null werden
Stimmen gelöscht. Intensität, Tempo und Bild-Öffnung folgen geglätteten Zielwerten.

- Lagerfeuer: warme Saiten mit zurückhaltender Fläche, Glocken und Bass.
- Weltraum: lange schwebende Mollflächen, seltener Sternenklang, langsame Harmonien.
- Schnee: reduzierte klare Glocken-/Kalimbaklänge und dezente Fläche.
- LoremFlickr: ruhiges E-Piano-/Saitenmotiv, Fläche, Glocken und weicher Bass.

`ChillPhotos` nutzt den bestehenden Netzwerk-Task, `Transport` mit SHA-256-Prüfung
und den durch Codec-Mutex geschützten `Media::decode`. Die UI besitzt nur die
vollständig vorbereiteten RGB565-Puffer und rendert eine abgedunkelte Zeile nach
der anderen in den vorhandenen Canvas. Maximal zwei Vollbilder benötigen zusammen
243.104 Bytes PSRAM, dazu 856 Bytes Scanline auf dem UI-Stack. Decode-Arbeit und
Downloads laufen nicht im Audio- oder Renderpfad. Das letzte erfolgreiche JPEG
liegt begrenzt unter `/chill-photo.jpg`, ein Download unter `/chill-photo.part`.
Die Flash-Schreibmenge entspricht dem angeforderten JPEG je erfolgreichem Bild;
es werden weder PCM-Audio noch gerenderte Frames in Flash geschrieben.

Foto-Paket: `scene=loremflickr`, `minFirmware=1.1.0`, leere `sprites`, Slider
`min=0,max=300,default=200`. Dieser interne Maßstab ermöglicht exakt lineare
`IntervallMillis = 300000 - Wert * 900`: 300/165/30 Sekunden bei 0/150/300,
120 Sekunden bei erstmaliger Nutzung. Der physische Regler ändert weiter in
5-%-Schritten (hier 15); NVS verwendet nur für den neuen Schlüssel `loremflickr`
einen Integer. Die drei vorhandenen UChar-Schlüssel bleiben unverändert.

15 Sekunden vor dem geplanten Wechsel wird ein Bild angefordert. Ein gültiges
neues Bild ist Voraussetzung für alle Bild-/Musikereignisse. 1,2 Sekunden vor
Fade-out folgt `Prepare`; danach `FadeOut` (1,5 s), `Black` (0,3 s), `FadeIn`
(1,5 s) und `Complete`. Die Musik reagiert harmonisch mit variierender Vorbereitung,
reduzierter Aktivität, optionaler Glocke und vorsichtiger Öffnung; sie wird dabei
nicht neu gestartet. Zeitabstände werden zwischen Fade-out-Starts gemessen.
Bei verspätetem Bild beginnt erst dann die Vorbereitung. Nach Reglerverkürzung
erhält ein überfälliger Wechsel 15 Sekunden Vorlauf. Fehler wiederholen nach
10/20/40/80/160/300 Sekunden; ohne fertiges Bild gibt es keinen Fade.

`IdlePolicy` erhält die bestehende Ausnahme für Chill und die normale Helligkeit.
Manuelle Mitteltasten und Sperren funktionieren weiter; beim Verlassen werden
Musikziel, Bildpuffer und Anfragen freigegeben/invalidiert. Laufende Downloads
können noch bis zum Timeout fertig werden, ihr Sitzungsergebnis wird verworfen.
Andere Seiten verwenden weiter die bisherigen 60-/180-Sekunden-Idle-Regeln.

### Validierung 1.1.0

Native Suite: Profile und Audio über 30 simulierte Minuten je Profil, WAV plus
Synth plus Benachrichtigung, sanfte Profilwechsel/Stop, Motivfortentwicklung,
Stimmen-/Pegelgrenzen, 5-/30-Minuten-Idle-Ausnahme, 59 weitere Bildwechsel,
Millis-Rollover, echtes Foto-Lifecycle/Rendering mit gemocktem Transport,
Offline-/Backoff, ungültiges JPEG-Ergebnis, Cache-Restore und Download-Abbruch.
Die bisherige Spiele-/PNG-/Storage-/Eingabesuite bleibt enthalten.

Offene Hardware-Abnahme: jede Szene 5 und 30 reale Minuten bei 0/50/100 %, alle
Profilwechsel per Homeserver-Sync, Rückkehr ins Hauptmenü und erneuter Start,
WLAN-Unterbrechung/-Rückkehr und Homeserver-Neustart. Am NV3007 Fades auf Flackern,
10-Hz-Animationen auf Stalls und I2S auf Aussetzer/Knacken prüfen. Freien Heap/PSRAM,
Task-Stack und reale CPU-Auslastung protokollieren. Es ist keine Hardware verbunden.
Der Live-Test von LoremFlickr ist in dieser Umgebung durch HTTP-403-Netzwerkregeln
blockiert; Redirect-, Cache- und Fehlerverhalten sind mit HTTPX-Mocks geprüft.
Lokaler ESP32-Cross-Build: Toolchain-Installation durch gesperrtes
`downloads.arduino.cc` blockiert; der bestehende GitHub-Arduino-Workflow ist der
verbindliche Cross-Build einschließlich Provisionierungsvariante.


## Eigene Bilder (1.1.2)

Im Homeserver je Gerät über „Eigene Bilder auswählen / hochladen“ eine Auswahl
von 1–24 Bildern speichern. Uploads dürfen je 8 MiB und 20 Megapixel enthalten.
Der Server richtet EXIF-gedrehte Bilder aus, schneidet auf 428×142 zu und speichert
nur ein optimiertes Baseline-JPEG ohne EXIF; Originale werden nicht aufbewahrt.
Identische Ergebnisse teilen einen Inhalts-Hash in der Bibliothek. Ein kopiertes
Gerät erhält eine unabhängige Auswahl mit denselben Bilddateien.

Das geräteeigene Paket trägt `scene: album`, `photos: ["photos/<hash>.jpg", …]`,
einen Slider 0–300 und `minFirmware: 1.1.2`. Das normale Installationsverfahren
prüft Dateien/Hashes und entfernt ältere Gerätepakete erst nach erfolgreichem Sync.
Die Fotos liegen vor dem Öffnen des Albums vollständig in LittleFS. Kein WLAN oder
Homeserver ist für die Wiedergabe nötig. `ChillPhotos`/`PictureTransition` nutzen
weiterhin den Netzwerk-Hintergrundtask zum Dekodieren; lokale Aufträge erzeugen
keine HTTP-Anfragen und überschreiben nicht den LoremFlickr-Cache. Nur aktuelles
und nächstes Bild liegen als RGB565-Puffer im RAM (~238 KiB zusammen), unabhängig
von der Albumgröße. Eigene Bilder haben einen separaten gespeicherten Bildabstand.
Bei einem einzigen Bild entfällt ein unnötiger wiederholter Bildwechsel.

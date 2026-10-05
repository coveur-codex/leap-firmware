# Chill V1 (1.0.0-beta.18)

Das NV3007 bleibt wie alle bestehenden Seiten im Querformat 428×142. Chill
verwendet den vorhandenen Canvas; keine zusätzliche Vollbildkopie. Nur der
Zurück-Pfeil und während Eingaben ein Slider bleiben sichtbar. Rechts MITTE
blendet den Slider ein; Richtungstasten ändern den generischen Wert 0–100 in
Fünferschritten. Ausblenden nach 4 s. Links MITTE öffnet das Hauptmenü, langes
Drücken sperrt. Die automatische Inaktivitätssperre/Dimmung pausiert hier.

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

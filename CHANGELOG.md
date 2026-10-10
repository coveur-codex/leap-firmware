# Changelog

## 1.0.7 – 2026-10-10

- ä, ö, ü, Ä, Ö, Ü und ß werden als echte Zeichen der eingebauten Displayschrift dargestellt.
- Deutsche Menü-, Spiel-, Status- und Küchentexte verwenden normale Umlaute und ß.
- UTF-8-Inhalte vom Homeserver werden erst für die Anzeige in Schriftzeichen umgewandelt;
  Zeilenumbruch, Kürzung und vergrößerte Artikelüberschriften zählen sichtbare Zeichen.
- Auch é, è und ° nutzen vorhandene Glyphen; unbekannte Unicode-Zeichen bleiben ein einzelnes Fragezeichen.

## 1.0.6 – 2026-10-10

- Flugradarringe alle 10 km: 10/20 km bei gleicher 50-km-Kartenbreite wie Regenradar.
- Kommunikation: 48 Kinder-Icons als 3×16-Raster, über rechts HOCH/RUNTER erreichbar; LINKS/RECHTS wählt die Spalte, MITTE sendet ein einzelnes Icon.
- Vergrößerte Einzel-Icons im Verlauf, lokale Glyphen ohne zusätzliche Assets.
- Kipp-Labyrinth mit 9 Zeilen × 21 Spalten, entsprechend größerem Generatorstack und Ziel unten rechts.
- Homeserver 1.0.3 ergänzt Einzel-Icon-Relay und korrigierte Regenradarringe; zuerst den Server aktualisieren.

## 1.0.5 – 2026-10-10

- **Dragon Run** heißt jetzt **Drachenrennen**. Spiel-ID und gespeicherter Rekord bleiben erhalten.
- Sprünge dauern 1,6 statt 1,2 Sekunden, bei unveränderter Höhe von 72 Pixeln.
- Feuer reicht 64 statt 48 Pixel weit und brennt 0,42 statt 0,24 Sekunden.
- Ruhigerer Start mit 60 statt 78 Pixel/s; die Höchstgeschwindigkeit bleibt 150 Pixel/s.
- Hindernisabstände berücksichtigen die längere Sprungdauer; Zeichen- und Kollisionsfläche des Feuers passen zusammen.

## 1.0.4 – 2026-10-10

- Neues Offline-Einzelspielerspiel **Dragon Run** im konfigurierten Spielemenü.
- Rechter Schalter: HOCH springt, RUNTER duckt beim Halten, MITTE speit Feuer
  oder startet nach Game Over erneut. Sprung und Feuer funktionieren gleichzeitig.
- Felsen, feuerfeste Säulen, Fledermäuse, zwei Holzbarrikadengrößen, Münzen und
  Edelsteine; zufällige Folgen mit geschwindigkeitsabhängigen Sicherheitsabständen.
- Programmatisch gezeichneter Drache, Lauf-/Flügel-/Schwanz-/Feueranimation,
  Treffer- und Schatzpartikel sowie drei Landschaften mit Parallax-Scrolling.
- Entfernungspunkte, Schatz-/Feuerboni und dauerhafter lokaler Highscore mit
  Wiederholung fehlgeschlagener NVS-Schreibvorgänge.
- Vollständiger 342×142-Spielbereich neben der bestehenden 86-Pixel-Sidebar,
  Canvas-Rendering mit Zielintervall 33 ms und begrenztem Simulationsspeicher.

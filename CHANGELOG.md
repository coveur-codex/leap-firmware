# Changelog

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

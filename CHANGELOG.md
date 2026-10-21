# 1.3.2 – OTA-Erststart mit fehlendem Netzwerk-Namespace

- Behebt sofortigen Rollback beim OTA-Update von 1.2.x: Der neue Namespace
  `leap-network` wird vor der ersten Profilmigration lesend/schreibend geöffnet.
  ESP-IDF kann einen noch fehlenden Namespace nicht nur lesend öffnen.
- Ursprüngliche Gerätekonfiguration und vorhandene Netzwerkprofile bleiben
  erhalten. Ungültige bestehende Profile werden weiterhin nicht überschrieben;
  fehlgeschlagene Erst-Speicherung wird beim nächsten Start erneut versucht.
- Getrennte CONFIG-Diagnosen für Geräte- und Netzwerkeinstellungen; kein
  irreführender USB-/NVS-Löschhinweis bei einem Netzwerkfehler.
- Regressionstest bildet den fehlenden Namespace des ESP32 nach und prüft
  Erststart, Neustart ohne erneute Migration sowie Erhalt der Provisionierung.

# 1.3.0-beta.1 – WLAN, externer Server und Zahlenzerlegung

- Externer HTTP-Server `http://ndk.tstromi.de:8080` mit der bisherigen
  Serveradresse als Rückweg; HTTP und Chat verwenden dieselbe aktive Adresse.
- Vier lokale und vier serverseitige WLAN-Profile je Gerät; Eingabe, Änderung
  und Löschen lokaler Profile unter Einstellungen → WLAN einrichten.
  Getrennte NVS-Speicherung erhält die ursprüngliche OTA-Gerätekonfiguration.
- Mathe-Labor: „Fehlenden Teil finden“ mit Zielzahlen je Gerät als Liste oder
  Bereich (1–100), einschließlich 0 als vorgegebenem oder fehlendem Summanden.
- Finanzanzeige und Vorschau zeigen kurze Symbole und Handelsplätze für die
  optionalen Frankfurter EUR-Notierungen des Homeservers.
- Benötigt Homeserver 1.3.0-beta.1 mit Migration 0020. ESP32-Build und Hosttests
  bestanden; reale OTA-/WLAN-Abnahme und Live-Kursprüfung stehen noch aus.

# 1.1.4 – Zeit-Modul

- Zeit-Modul: Hintergrundtimer mit Akzentfarben-Ring, 1/3/10/60 Minuten und
  eigener Zeit, Stoppuhr mit 20 Zwischenzeiten, priorisierter Ablaufhinweis und
  vierfacher Warnton. Benötigt Homeserver-Migration 0018 für die Seitenfreigabe.

# 1.1.3 – Größere Helvetica-Bold-Überschriften

- Helvetica Bold von 8 auf 10 pt für News-Titel, Flugzeugkennungen und Quizfragen
  vergrößert (innerhalb 11 ±1 pt); echte Bitmap-Glyphen ohne Skalierung.
- Quizantworten beginnen passend zu den größeren Fragezeilen bei y=52.
- Deutsche Glyphen, Umbrüche, Schriftmetriken und alle vier Antwortzeilen geprüft.

# 1.1.2 – Farben, kompakte Überschriften, eigene Chill-Bilder und Chatavatare

- Mathe-Labor verwendet die Geräteakzentfarbe für Menü, Mengen, Bauwerke und Markierungen.
  Kontrastabhängige Menüs und dunkelviolette Würfelaugen ersetzen Gelb auf Weiß.
- Helvetica Bold 8 für News-Titel, Flugzeugkennungen und Quizfragen: kleinere, echte fette Glyphen.
- Eigene Offline-Chill-Alben verwenden die normale Asset-Verteilung und bestehende
  Überblendungs-/Musiklogik; höchstens zwei dekodierte Fotos gleichzeitig im RAM.
- Rechtsbündige 48px-Absenderbilder im Chat; neues gemeinsames Paket mit unveränderlichen
  Porträts statt vollständiger fremder Avatarpakete. Ältere Nachrichten/Server bleiben nutzbar.

# 1.1.1 – Mathe-Labor

- Ein Spieleintrag mit vier offline verfügbaren Lernaktivitäten und frei wählbaren Stufen.
- Würfelbilder, unabhängige Zufallswürfe, Häufigkeiten, Mengenfelder, Zahlzerlegung,
  Mengenvergleich und Stellenwertbaukasten bis 100.
- Isometrische Bauwerke zum Nachbauen, Merken und Wiederansehen; Vergleich der Stapelhöhen.
- Geometrische Kantenrätsel und eindeutig passende Draufsichten mit visuellen Hilfen.
- Gemeinsame mathematische Logik, prozedurale Grafik und Host-Tests inklusive Bildschirmgrenzen.

# 1.1.0

- Gemeinsame regelbasierte Chill-Komposition mit acht Synthesizer-Stimmen, vier
  Profilen, Motivgedächtnis, ADSR, Instrumentenpegeln und weichen Profilwechseln.
- LoremFlickr über den Homeserver, Prefetch und persistentes letztes Bild,
  Übergänge 1,5 s → 0,3 s Schwarz → 1,5 s mit harmonischen Ereignissen.
- Pro Szene gespeicherter Slider; für Fotos linear 30–300 s, initial 120 s.
- Gemeinsame überprüfbare Idle-Ausnahme für den gesamten Chill-Bereich.
- WAV- und Benachrichtigungs-Mixing bleiben im vorhandenen I2S-Task verfügbar.

# Changelog

## 1.0.11 – 2026-10-14
- News-Titel, Flugzeugkennungen, Quizfragen und Mathe-Aufgaben nutzen Helvetica Regular 10 ohne Fettzeichnung oder Pixelverdopplung.
- Proportionaler Zeilenumbruch mit Umlauten, ß, é, è und °; lange Quizfragen bleiben in der scrollbaren Vollansicht lesbar.
- Schrift direkt in der Firmware enthalten, ohne zusätzliche Library oder Download am Gerät.

## 1.0.10 – 2026-10-13
- Kommunikationsverlauf zeichnet Absender, Text und Icon in der Absenderfarbe.
- News- und Flugzeughinweise dauern 10 Sekunden, Kommunikation weiterhin 5 Sekunden.
- Benachrichtigungen öffnen bei abgedunkeltem Display das Modul und wählen den Artikel beziehungsweise das Flugzeug per stabiler ID.
- Optionale Nachtruhe für Benachrichtigungstöne nach lokaler Gerätezeit, auch über Mitternacht.
- Homeserver 1.0.6 mit Migration 0015 für die Nachtruhe erforderlich.

## 1.0.9 – 2026-10-12
- Gemeinsame 5-Sekunden-Benachrichtigungen im Avatarbereich mit Warteschlange und News-Bündelung.
- Modul-Markierungen bis zum Öffnen ersetzen den gelben Brief; Kommunikation zeigt Absenderfarbe.
- Pling, Plong und Zisch; pro Dienst im Homeserver abschaltbar.
- Räumliche Flugzeugnähe bis 3 km, Standorthöhe, 3,5-km-Pufferzone und stille Startbestände.
- Radarabfrage alle 15 Sekunden im Hintergrund und sekündliche begrenzte Positionsvorhersage.
- Homeserver 1.0.5 mit Migration 0014 erforderlich.

## 1.0.8
- Chat über WebSocket-Push, Cursor-Nachladen, idempotente Bestätigungen und begrenzte Queues; Homeserver 1.0.4 erforderlich.
- WLAN-Minimum-Modem-Sleep, Keepalive alle 30 Sekunden und Wiederverbindung mit Backoff.
- Akzentfarbe pro Gerät vom Homeserver; bisheriges Türkis bleibt Standard.
- Künstliche Fettzeichnung und verzerrte Skalierung der Überschriften/Flugzeugkennungen entfernt.
- Regen- und Flugradar auf 60 × 60 km mit drei Ringen bei 10/20/30 km; N oben mittig.


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

# Spiele: Regeln, Asset-Vertrag und Abnahme

## Auswahl durch den Homeserver

`config.games` steuert die sichtbaren und startbaren Spiele einschließlich `kitchen` und `crab_journey`.
Nur bekannte IDs mit `enabled: true` erscheinen, ohne automatische Zusatzspiele.
Eine leere, fehlende oder vollständig deaktivierte Liste zeigt „Keine Spiele
freigegeben“. Anzeige und Start verwenden dieselbe gefilterte Liste.
Die lokal gespeicherte Konfiguration gilt auch ohne WLAN. Eine geänderte
Konfiguration schließt beim Sync ein laufendes Spiel und übernimmt die neue Auswahl.
Die Spielprogramme und lokalen Spielstände bleiben erhalten. Homeserver und
Firmware zusammen aktualisieren: ältere Server führen Küche und Krabbenreise nicht in der Liste.

## Vier Gewinnt (beta.15)

`connect_four` wird zusätzlich vom Homeserver in `config.games` angeboten.
Offline-Spiel gegen das Gerät auf 7×6 Feldern, keine Assets und keine Speicherung
laufender Runden. Vor jeder Runde die Stufe mit rechts UP/DOWN wählen, CENTER
bestätigt. Gelb (Mensch) beginnt; Rot (LEAP) antwortet nach kurzer Denk-Anzeige.
Rechts LEFT/RIGHT wählt die Spalte, CENTER setzt. Volle Spalten lassen den Zug
beim Menschen. Vier waagerecht/senkrecht/diagonal gewinnen, 42 Steine ohne Vierer
ergeben ein Unentschieden. CENTER nach dem Ende öffnet die Stufenwahl erneut.
Links CENTER, Seitenwechsel und Sperren verlassen das Spiel wie bisher.

Leicht wählt zufällig unter gültigen Zügen. Mittel nimmt direkte Gewinne,
blockiert direkte gegnerische Gewinne und bewertet zwei Halbzüge. Schwer nutzt
zusätzlich Alpha-Beta bis sechs Halbzüge; je Kandidat maximal 2000 Suchknoten,
insgesamt maximal 14000. Kein Anspruch auf einen unbesiegbaren Gegner.

`tests/test_connect_four.cpp` prüft Schwerkraft, volle Spalten, alle vier
Gewinnrichtungen, Unentschieden, Priorität Gewinn vor Blockade, unveränderte
Spielfelder bei der Suche und vollständige Partien auf allen Stufen.
`test_game_lifecycle.cpp` prüft die echte Games-Implementierung mit Stufenwahl,
Eingabesperre im Gegnerzug, Spielende, Neustart, voller Spalte und Verlassen
während eines ausstehenden Gegnerzugs.

Am Gerät noch prüfen: Lesbarkeit der 18-Pixel-Zellen, Farben und Spaltenmarker,
alle Stufen, Seitenwechsel/Sperren und Tastenreaktion während der schweren Suche.

## Snake und Kipp-Labyrinth (beta.16)

Snake startet mit der Auswahl Langsam/Mittel/Schnell über rechts UP/DOWN/CENTER.
Schrittintervalle 450/300/220 ms, Standard Langsam. Während der Auswahl bewegt
sich nichts; nach dem Start gilt das volle gewählte Intervall für den ersten
Schritt. Drei Futterpunkte liegen auf verschiedenen Feldern außerhalb des Körpers;
nur der gefressene wird ersetzt. Bei zwei/einem freien Feld gibt es entsprechend
weniger Futter. Rekord und bisherige Gegenrichtungs-/Kollisionseigenschaften bleiben.

Das Kipp-Labyrinth generiert beim Start per randomisiertem Tiefensuchverfahren
ein zusammenhängendes 9×9-Labyrinth (25 Räume und 24 Verbindungsgänge). Start
(0,0), Ziel (8,8), Kipp- und Schaltersteuerung. Die 12-Pixel-Zellen passen ins
Display. Keine gespeicherten Runden, keine zusätzlichen Assets oder WLAN-Abhängigkeit.

`test_games.cpp` prüft alle drei Futterplätze, Ersatz, überlappungsfreie Platzierung
und beinahe volle Spielfelder. `test_game_lifecycle.cpp` prüft alle drei Intervalle
mit der echten Games-Implementierung und Stillstand bei Auswahl/Verlassen.
`test_maze.cpp` prüft 100 Seeds: unterschiedliche Layouts, alle Gänge verbunden,
Ziel mit regulären Spielzügen erreichbar, Ränder und Neustart.

Am Gerät prüfen: alle Geschwindigkeiten mit Kindern, Futterdarstellung, Rekord,
Rundenende und neue Auswahl; zufällige Labyrinthe, Lesbarkeit, Kippsteuerung und
Links-Mitte/Seitenwechsel/Sperren bei beiden Spielen.

## Haustier und Snake

Firmware `1.0.0-beta.12` und zugehöriger Homeserver. Beide neuen IDs werden in
`config.games` angeboten; bestehende Spiele bleiben erhalten. Kein neues Netzwerk-
oder Speicherprotokoll. Pet-Zustand und Snake-Rekord sind lokal im NVS-Namespace
`leap-games` (`pet`, versionierter 5-Byte-Wellbeing-Datensatz; `snake-best`, Integer).
Ungültige/fehlende Datensätze erhalten die kinderfreundlichen Standardwerte.
Ein Sync ersetzt diese Spielstände nicht.

## Avatarpaket

Das gewählte `avatar-*`-Paket enthält wie bisher `definition.json`, PNG-Dateien
und optional Web-Vorschauen. Das Beispiel-ZIP war bei der Implementierung nicht
hochgeladen. Der Import unterstützt die beschriebene Struktur mit äußerem Ordner,
`data/pet/STATE/*.png` oder `STATE/*.png` sowie
`background_day.png`/`background_night.png` oder gleichnamigen Hintergrundordnern.
Pfade bleiben unverändert. Uploads ergänzen die reguläre Paketversion; keine
separaten Tamagotchi-Downloads. Ein PNG-Idle wird auch für die Sidebar referenziert,
wenn diese bislang keine Idle-Animation hatte. Bestehende Sidebar-Animationen und
explizite Metadaten bleiben erhalten. Pro Version gelten weiterhin alle Hash-,
Pfad-, Speicher- und Aktivierungsprüfungen.

```json
{
  "tamagotchi": {
    "backgrounds": {"day": "background_day.png", "night": "background_night.png"},
    "animations": {
      "idle": {"frames": ["data/pet/idle/frame_01.png"], "frameDurationMs": 400},
      "eating": {"frames": ["data/pet/eating/frame_01.png"], "frameDurationMs": 400}
    }
  }
}
```

Weitere Zustände: `happy`, `sad`, `hungry`, `tired`, `dirty`, `playing`, `sleeping`.
Üblich sind je vier 80×80-Frames; variable Frameanzahlen funktionieren ebenfalls.
Waschen nutzt `happy`. Fehlende Zustandsanimationen fallen auf Pet-Idle, dann
Sidebar-Idle/Preview bzw. eine Zeichnung zurück. Ab beta.17 dürfen Hintergrund-PNGs beliebige Maße zwischen 1 und 1024 Pixeln
je Achse besitzen, etwa 264×142. Die Darstellung passt sie mit erhaltenem
Seitenverhältnis in die 256×142-Spielfläche ein. Alle anderen Avatar-PNGs bleiben
80×80. Größe/SHA-256 im Manifest und vollständige PNG-Decoderprüfung bleiben
unverändert; bestehende Pakete benötigen keinen erneuten Upload. Hintergründe werden über explizite Referenzen oder die eindeutigen Namen
`background_day`/`background_night` erkannt. Andere Avatar-PNGs bleiben auf
80×80 beschränkt. Bereits hochgeladene Pakete benötigen keine neue Definition:
Die Firmware erkennt Zustandsordner und die bisherigen `animations`-Einträge
aus dem unveränderten Manifest. Der Homeserver ergänzt fehlende Metadaten
einmalig als neue Paketversion beim Start bzw. nächsten Sync. Dateien, vorhandene
Paketversionen und explizite Konfiguration bleiben erhalten.
Die Firmware nutzt gecachte Manifeste und den bestehenden asynchronen Media-Worker.
Eine zusätzliche Maske lässt transparente Bereiche des Tieres frei.

## Einfaches Tamagotchi (beta.12)

Vier Balken („Satt“, „Spass“, „Sauber“, „Kraft“) zeigen die Bedürfnisse dauerhaft:
grün ab 80, gelb ab 55, orange darunter. Eine Statuszeile benennt das dringendste
Bedürfnis und bestätigt jede Pflegeaktion. Alle 30 Betriebssekunden sinken Essen,
Spaß und Energie um zwei Punkte, Sauberkeit um einen. Der Mindestwert bleibt 35;
das Tier stirbt nicht. Nach etwa acht Minuten ohne Pflege wird ein neues Tier
hungrig. Pflege füllt den jeweiligen Wert auf 100. Die unveränderten gespeicherten
5-Byte-Spielstände bleiben gültig. Passive Änderungen werden alle fünf Minuten
und beim Verlassen gespeichert, Pflege sofort. Ein harter Stromverlust kann bis
zu fünf Minuten passiver Änderungen verlieren; ausgeschaltete Zeit zählt nicht.

Die vier Paketframes werden durch leichtes Hüpfen, seitliche Spielbewegung,
Glitzern und Schlaf-„Z“ ergänzt. Schlafen nutzt während der dreisekündigen Aktion
den Nachthintergrund. Sonst gilt die lokale Uhr (20–7 Uhr, ohne Uhr Tag).
Fehlende/ladebereite Hintergründe erhalten eine lokale Landschaft als Ersatz;
sobald das Paketbild dekodiert ist, übernimmt es. Ein fehlgeschlagener Decode wird
nach zwei Sekunden erneut versucht, auch wenn sich der Blob-Pfad nicht ändert.

## Automatisierte Prüfungen

`tools/test.sh` prüft Regeln und die echte `Games.cpp` mit simulierten GPIOs,
Preferences, Zeichnen und Zeit. Dabei: vier Aktionsframes, Rückkehr zum Gemüt nach
3 s, alle Aktionen, persistenter Zustand nach neuer Games-Instanz, korrupter
Speicher, Snake-Runde/Beenden/Neustart und persistenter Rekord bereits beim Fressen.
Dies ist kein Beleg für physische NVS-Power-Cut-Eigenschaften.
`tools/test_png.sh` nutzt PNGdec 1.1.6 mit echten 80×80-, 256×142- und 264×142-Fixtures,
Alpha-Masken und Decoderfehlern. `tests/server_contract.py` erzeugt reale
Homeserver-Manifeste mit Pet-Hintergründen für `tests/test_core.cpp`.
Homeserver-Tests prüfen Import, Sync, Versionsschutz, Frame-Erweiterung, Löschen,
ungültige Referenzen und die Geräte-Vorschau.

## Auf dem Gerät prüfen

- Spieleliste: neue und bisherige Spiele erreichbar, Navigation links unverändert.
- Haustier: rechts UP/DOWN/CENTER, alle vier Aktionen und vier Frames pro Zustand;
  alle Gemüter mit passenden Testwerten; Tier transparent über Tag-/Nachtbild.
  Statuszeile, vier Balken und Bewegungen prüfen; acht Minuten ohne Pflege warten.
- Uhr auf 06:59/07:00 und 19:59/20:00 setzen, auch nach Offline-Neustart prüfen.
- Haustier verlassen, sperren, Seite wechseln und Gerät neu starten: Bedürfnisse
  bleiben erhalten. Mehrere Stunden ausgeschaltet verursacht keinen Abzug.
- Snake: alle vier Richtungen, Gegenrichtung, zwei schnelle Kurven, Futter,
  Wand-/Körperkollision, Neustart, Linke Mitte, Seitenwechsel und Sperren.
- Rekord übertreffen, sofort neu starten: Rekord bleibt; Runde beginnt neu.
- Während beider Spiele einen regulären Sync und eine neue Avatarversion anbieten:
  unveränderter Sync erhält Runde, Konfigurations-/Assetwechsel beendet sie sauber.
  Spielstände bleiben. Fehlende Bilder/offline: Ersatzdarstellung, bedienbar.
- Reale Bildübertragung, Tastenlatenz, NVS bei Stromverlust und Sounds abnehmen.

## Krabbenreise

`crab_journey` ist wie die Küche lokal in der Spieleauswahl verfügbar, auch ohne
Homeserver-Update. Ein gleichnamiger Servereintrag wird nicht doppelt angezeigt.
Die rechte Richtungstaste halten: freie Bewegung mit 92 Pixeln/s (ca. 3,7 Pixel
pro 40-ms-Frame), ohne Bewegungsraster; diagonale Bewegung ist normalisiert.
Die entprellten gehaltenen Richtungen werden unabhängig von der Ereignisqueue
übergeben. Loslassen stoppt die Bewegung. Linke Navigation, Zurück, Sperren und
Konfigurationswechsel behalten ihren bisherigen Ablauf. Rechts Mitte ist nicht
zum Bewegen erforderlich. Bei einem neuen Spiel beginnt die Reise auf Level 1.

Die volle Fläche rechts der unveränderten 86-Pixel-Sidebar wird genutzt: 342×142.
Grafiken entstehen ausschließlich aus Zeichencode: Comic-Krabbe, gewölbte Quallen
mit Tentakeln, Schnecken mit Spiralgehäuse, stachelige Seeigel, schwingender Seetang,
Strömungspartikel, Spiralstrudel, Muscheln, Sand, kleine Steine und Blasen.
Bodenobjekte und Krabbe werden nach ihrer Y-Position gezeichnet. Die Animationen
laufen zeitabhängig; der Spielbereich fordert einen Frame alle 40 ms an (Ziel
25 FPS, tatsächliche Bildrate hängt vom Displaytransfer ab).

Jeder Start und Levelwechsel erzeugt eine neue Karte aus einem ESP-Zufallsseed.
Ein zusammenhängender, unsichtbarer Korridor aus fünf Segmenten bleibt vollständig
frei: Hindernisradius, Krabbenradius, maximaler Bewegungsausschlag und 16 Pixel
zusätzliche Korridorbreite werden beim Platzieren berücksichtigt. Damit können
auch pendelnde Tiere und Strudeleinflüsse diesen Weg nicht blockieren. Start und
Ziel haben eigene Schutzzonen. Bewegungs-/Einflussbereiche verschiedener Elemente
überlappen nicht. Begrenzte Platzierungsversuche lassen bei wenig Platz Budget
ungenutzt, statt eine unfaire Karte zu erzwingen. Drei optionale Muscheln liegen
auf dem Weg bzw. auf freien Umwegen; auch ihre Zugänge vom Hauptweg werden
freigehalten. Sie bleiben nach einem Rücksetzen gesammelt.

Level 1 enthält Seeigel; Level 2 führt Schnecken ein, Level 3 Quallen, Level 4
Seetang, Level 5 Strömung und Level 6 Strudel. In diesen Levels werden Seeigel und
der neu eingeführte Typ kombiniert; ab Level 7 werden bis zu drei Typen gewählt.
Das Budget wächst von 3 um 2 je Level bis maximal 28. Seeigel/Schnecke kosten 1,
Seetang 2, Qualle/Strömung 3, Strudel 5. Anzahl, Kombinationen, Bewegungsachsen,
Wegverlauf und leicht wachsendes Quallentempo erzeugen die Schwierigkeit.

Seeigel-/Quallenberührung löst ein kurzes Erschrecken aus und setzt 23 Pixel auf
den sicheren Weg zurück. Schnecken und Seetang schieben freundlich zur Seite.
Strömungen versetzen die Krabbe langsam; die Strudelanziehung steigt zum Zentrum
hin. Im Zentrum folgt eine Wirbelanimation und ein Rücksetzen um 38 Pixel.
Nach Rücksetzen gelten 1,8 Sekunden Schutz. Kleine Kollisionsradien erlauben
knappe sichtbare Berührungen. Keine Leben, kein Zeitlimit, kein Game Over.
Das Ziel bestätigt den Erfolg und zeigt die gesammelten Muscheln; nach 2,5 Sekunden
beginnt automatisch das nächste Level. Sammeln und Erfolg nutzen die vorhandene
Audioqueue. Lautstärke und Stummschaltung bleiben unverändert.

`CrabJourney.h` enthält plattformunabhängige Generierung/Simulation und zentrale
Parameter (Tempo, Budget, Kapazität, Elementkosten, Radien, Bewegung, Schutzzeit).
`CrabJourneyDraw.cpp` enthält die Darstellung. Weltbreite und Kameraversatz sind
von den Bildschirmkoordinaten getrennt; V1 scrollt nicht und erzeugt keine
Endless-Abschnitte. Feste Arrays begrenzen Speicher und Renderaufwand; kein
Grafikpaket, keine neue Speicherung und keine Netzwerkabhängigkeit.

Automatisch geprüft: 4.500 Karten auf 30 Stufen mit echtem Durchlaufen des freien
Wegs, Budget/Abstände, Zufallsvariation, kontinuierliche Bewegung, Sammeln,
Rücksetzen, Strömung, Strudel und Erfolg ohne Muscheln. Lifecycle-Tests prüfen
die echte Games-Integration und Renderergrenzen mit Kameraversatz; Input-Tests
prüfen gehaltene Tasten bei voller Queue und im Polling-Fallback.
Am Gerät bleiben Bildrate, Tastenlatenz, Sounds, Lesbarkeit und die Schwierigkeit
mit einem Grundschulkind zu prüfen.

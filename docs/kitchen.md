# Meine Kueche (v2)

Der lokale Singleplayer-Kücheneditor steht am Ende der Spieleauswahl. Die Küche
hat 24 Rasterplätze, drei Ebenen und vier Farben. 12 Plätze sind gleichzeitig
sichtbar; eine Scrollleiste und „Sicht 1–12“ beziehungsweise „Sicht 13–24“ zeigen
den Ausschnitt. Der Editor funktioniert offline ohne Grafikpakete oder
Homeserver-Änderungen. Die Spiele-Seite muss in der LEAP-Konfiguration aktiv sein.

## Bedienung auf dem Gerät

Links wird die Aktion gewählt, rechts der Ort gesteuert und die Aktion bestätigt.
Der aktuelle Modus und eine passende kurze Bedienhilfe stehen im Bild.

| Taster | Aktion |
| --- | --- |
| Links: hoch / runter | Zwischen Scrollen, Bauen, Verschieben, Abreißen, Farbe und Menü wechseln |
| Links: Mitte kurz | Zum nächsten Modus wechseln; verändert keine gespeicherten Objekte |
| Links: links / rechts | Im Baumodus Objekt wählen, im Farbmodus Farbe wählen, im Menü Eintrag wählen |
| Rechts: links / rechts | Ort wählen; im Scrollmodus den Bildausschnitt verschieben, im Menü Eintrag wählen |
| Rechts: hoch / runter | Unten, Arbeitsplatte oder Oben wählen; im Menü Eintrag wählen |
| Rechts: Mitte | Die gewählte Aktion ausführen |
| Links: Mitte mindestens 2 Sekunden halten | Zur Spieleauswahl zurückkehren |

Der Startmodus ist Bauen. Beim Bearbeiten folgt der Bildausschnitt automatisch dem
Cursor. Der Scrollmodus verschiebt ihn schrittweise und nimmt den Cursor mit.
An beiden Enden stoppt die Bewegung; sie springt nicht auf die andere Seite.
Scrollen und die Auswahl von Ort, Objekt, Modus oder Farbe verändern keine Objekte.

- **Bauen:** Objekt links wählen, Ort und Ebene rechts wählen, rechts bestätigen.
  Beim Ebenenwechsel wird bei Bedarf der erste passende Objekttyp ausgewählt.
  Grün markiert eine gültige Vorschau, Pink fehlenden Platz oder Unterstützung.
  Bestätigen am Ort eines vorhandenen Objekts ersetzt dieses, sofern die Küche
  danach gültig bleibt.
- **Verschieben:** Vorhandenes Objekt rechts auswählen und mit rechts-Mitte aufnehmen.
  Rechts zum Ziel bewegen und erneut bestätigen. Die Ebene bleibt während des
  Verschiebens fest. Jeder Moduswechsel bricht die Vorschau ab. Bis zum Bestätigen
  bleibt das Original unverändert, auch beim Verlassen des Spiels.
- **Abreißen:** Ort und Ebene rechts wählen und mit rechts-Mitte entfernen. Auch
  ein innerer Rasterplatz eines breiten Objekts kann gewählt werden.
- **Farbe:** Farbe links wählen, vorhandenes Objekt rechts auswählen und bestätigen.
  Die Objektart bleibt erhalten; auch breite Objekte werden vollständig umgefärbt.
  Die Farbfelder zeigen die gewählte Farbe.
- **Menü:** „Zurueck“ oder „Kueche leeren“ wählen und rechts bestätigen.
  Leeren öffnet eine Bestätigung mit voreingestelltem „Abbrechen“. Rechts-rechts
  oder rechts-runter wählt „Leeren“, rechts-Mitte bestätigt. Links-Mitte bricht
  immer ab. Erst die Bestätigung entfernt und speichert die Küche.

Der linke lange Mitteldruck liefert zunächst ein kurzes Ereignis, dann das normale
900-ms-Ereignis und schließlich ein einmaliges 2000-ms-Ereignis. Der kurze Druck
wechselt nur den Modus. Die Küche ignoriert die Halte-Ereignisse bis 2000 ms und
meldet dann den Ausgang an die UI, bevor die globale Sperrbehandlung greift.
Andere Seiten behalten ihre bisherigen 900-ms-Aktionen.

## Objekte und Platzierungsregeln

28 semantische Objekttypen werden direkt aus Flächen, Linien und kleinen Details
gezeichnet, ohne zusätzlichen Framebuffer oder externe Grafikdateien:

| Ebene | Objekte |
| --- | --- |
| Unten | Unterschrank, Schubladen, Spüle, Herd mit Ofen, Geschirrspüler, Kühlschrank, Hochschrank, **Esstisch, schmaler Tisch, Stuhl, Stuhl nach links, Stuhl nach rechts** |
| Arbeitsplatte | Mikrowelle, Kaffeemaschine, Toaster, Pflanze, **Wasserkocher, Messerblock, Obstschale, Küchenmaschine, Kerzenständer mit Kerze** |
| Oben | Hängeschrank, breiter Schrank, Regal, Abzugshaube, **Gewürzregal, Wanduhr, Tellerregal** |

Der Esstisch ist drei Plätze breit und hat eine farbige Platte, Holzmaserung,
Zarge und Beine. Der schmale Tisch ist zwei Plätze breit und trägt ebenfalls
Dekoration. Die seitlichen Stühle können links oder rechts zum Tisch zeigen;
ihre Richtung bleibt beim Umfärben erhalten. Der Stuhl hat eine farbige Rückenlehne und Sitzfläche sowie
Holzbeine und eine Querstrebe. Weitere Details sind unter anderem Griff, Deckel
und Sockel des Wasserkochers, drei Messer im Holzblock, Früchte mit Stielen,
beschriftete Gewürzgläser und das Zifferblatt mit Zeigern der Wanduhr. Die
Küchenmaschine hat einen Motorarm, Geschwindigkeitsregler, Rührbesen und eine
Stahlschüssel. Das Hängeregal zeigt sechs Teller mit farbigen Rändern auf zwei
Ablagen. Der Kerzenständer zeigt Fuß, Schaft, Kerze, Docht und Flamme und passt
auf beide Tische sowie auf Arbeitsplatten.

Spüle, Herd, Kühlschrank, Hochschrank und einige Wand- oder Plattenobjekte sind
zwei Plätze breit. Kühlschrank und Hochschrank blockieren alle drei Ebenen.
Arbeitsplattenobjekte benötigen über ihre ganze Breite Unterschränke,
Schubladen, Geschirrspüler oder einen der beiden Tische. Stühle, Spüle und Herd tragen
keine weiteren Objekte. Wandobjekte benötigen keine Unterbauten.

Verschieben und Ersetzen werden auf einer Kopie geprüft. Bei Kollisionen oder
fehlender Unterstützung bleibt die alte Küche vollständig erhalten. Getragene
Dekoration muss deshalb vor dem Verschieben ihres Unterbaus versetzt werden.
Beim ausdrücklichen Abreißen eines Unterbaus wird seine Dekoration mit entfernt;
Wandobjekte bleiben erhalten. Angeschnittene Objekte werden am Bildausschnitt
für alle Zeichenprimitive begrenzt, einschließlich Vorschau und Markierung.

## Speicherung und Kompatibilität

Der erste Start ist leer. Jede bestätigte Änderung schreibt sofort einen einzelnen
368-Byte-Datensatz in die Preferences-Namespace `leap-games`, Schlüssel `kitchen`.
Version 2 enthält bis zu 72 Objekte mit Typ, Position, Ebene, Breite und Farbe sowie
eine Prüfsumme. Es werden keine Zeiger, Struktur-Padding oder Bildschirmdaten
gespeichert. Laden prüft Version, Größe, Prüfsumme, Typen, Grenzen, Kollisionen
und Unterstützung. Fehlende oder ungültige Daten ergeben eine leere Küche.

Vorhandene 188-Byte-Speicherstände der Version 1 werden mit ihren bisherigen
Positionen und Farben geladen. Sie dürfen nur die alten 15 Typen innerhalb der
alten 12 Plätze enthalten. Erst eine bestätigte Änderung speichert Version 2.
Unbestätigte Aktionen oder bloßes Verlassen schreiben keine Migration.
Eine ältere Firmware kann Version 2 nicht lesen.

Fehlgeschlagene Schreibversuche werden als „Speichern...“ angezeigt und nach fünf
Sekunden sowie beim Verlassen erneut versucht. Identische Bestätigungen schreiben
nicht erneut in den Flash. `KitchenType` verwendet stabile IDs; neue Typen werden
hinten ergänzt. Kochfunktionen, Sounds, Punkte, Zeitlimits und Multiplayer sind
nicht enthalten.

## Prüfung

`tools/test.sh` enthält native Regressionen für alle Modi, beide Taster,
Scrollgrenzen und automatische Cursorverfolgung, breite Objekte, atomare
Änderungen, Unterstützung und Entfernung, Farben, beide Ausgänge und bestätigtes
Leeren. Alle 28 Typen in vier Farben werden an beiden Scrollrändern und in
angeschnittenen Ausschnitten auf Zeichengrenzen geprüft. Die Logiktests prüfen
auch eine volle Küche mit 72 Objekten, beschädigte Daten und v1-Migration.
Die Games-Lifecycle-Prüfung verwendet echtes Laden und Speichern über die
Preferences-Schnittstelle, inklusive Neustart, Migration und Schreibfehlern.
Der CI-Workflow baut zusätzlich die ESP32-S3-Firmware mit gepinnten Bibliotheken.

Auf echter Hardware noch prüfen: Lesbarkeit und Erkennbarkeit der neuen Details,
Tastergefühl der Moduswahl und des Scrollens, beide Ausgänge, Reset-Bestätigung,
Wiederherstellung nach Neustart und Bearbeiten einer dicht eingerichteten Küche.

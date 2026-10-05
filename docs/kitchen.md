# Meine Kueche (v1)

Der lokale Singleplayer-Kücheneditor steht am Ende der vorhandenen Spieleauswahl.
Dafür sind weder Homeserver-Änderungen noch heruntergeladene Pakete nötig. Die
Spiele-Seite muss wie bisher in der LEAP-Konfiguration eingeschaltet sein.

## Bedienung auf dem Gerät

| Taster | Aktion |
| --- | --- |
| Links: links / rechts | Rasterposition wählen (12 Plätze) |
| Links: hoch / runter | Unten, Arbeitsplatte oder Oben wählen |
| Links: Mitte | Vorhandenes Objekt aufnehmen; erneut drücken bricht Verschieben ab |
| Rechts: links / rechts | Passende Objekte der gewählten Ebene einschließlich Entfernen, Zurueck und Kueche leeren durchblättern |
| Rechts: hoch / runter | Eine von vier Frontfarben wählen |
| Rechts: Mitte | Vorschau platzieren, Verschieben bestätigen, ersetzen oder entfernen |
| Links: Mitte mindestens 2 Sekunden halten | Zur Spieleauswahl zurückkehren |
| Rechts: „Zurueck“ auswählen, dann Mitte | Ohne Halten zur Spieleauswahl zurückkehren |
| Rechts: „Kueche leeren“ auswählen, dann Mitte | Bestätigung zum Zurücksetzen öffnen |

Die Vorschau ist grün bei gültiger Platzierung und pink bei fehlendem Platz oder
fehlender Unterstützung. Zum Ersetzen den neuen Objekttyp an der Position des alten
Objekts bestätigen. Zum Löschen „Entfernen“ auswählen und rechts bestätigen; auch
die zweite Rasterposition eines breiten Objekts kann dafür ausgewählt werden.

„Zurueck“ und „Kueche leeren“ stehen auf jeder Ebene hinter „Entfernen“ in der
Objektauswahl. Von der ersten Objektart aus führt rechts-links direkt zu
„Kueche leeren“, ein weiterer Schritt links zu „Zurueck“.

Beim Leeren ist zuerst „Abbrechen“ ausgewählt. Rechts-rechts oder rechts-runter
wählt „Leeren“, rechts-Mitte bestätigt. Links-Mitte bricht auch dann ab, wenn
„Leeren“ markiert ist. Erst die ausdrückliche Bestätigung entfernt sämtliche
Objekte und speichert die leere Küche automatisch. Verlassen oder Abbrechen
verändert die eingerichtete Küche nicht.

Der linke lange Mitteldruck ist bewusst der Ausgang: Die Eingabe liefert zunächst
einen kurzen Druck und später das Halte-Ereignis. Der kurze linke Druck verändert
nur die Auswahl; so platziert oder löscht das Verlassen keine Gegenstände.
Zusätzlich zum bisherigen 900-ms-Ereignis erzeugt die vorhandene Eingabe für die
Mitteltaster ein einmaliges Ereignis nach 2000 ms. Die Küche ignoriert das erste
Halte-Ereignis und verarbeitet das zweite als Ausgang, bevor die globale
Sperrbehandlung greifen kann. Beide Ausgänge werden durch den Kücheneditor an die
UI gemeldet, die das Spiel schließt und die Spieleauswahl anzeigt. Die bisherigen
900-ms-Aktionen anderer Seiten bleiben erhalten.

## Platzierung und Speicherung

15 semantische Objekttypen werden aus Linien, Flächen und kleinen Details direkt
auf das bestehende Canvas gezeichnet; kein zusätzlicher Framebuffer und keine
externen Grafikdateien. Herd und Kühlschrank sind zwei Rastereinheiten breit.
Hohe Schränke und Kühlschränke blockieren alle drei Ebenen. Arbeitsplattenobjekte
benötigen durchgehend Unterschränke oder Geschirrspüler; Spüle und Herd tragen
keine weiteren Objekte. Hängeschränke und Abzugshauben benötigen keine Unterbauten.

Verschieben und Ersetzen werden zunächst auf einer kleinen Kopie der logischen
Küche geprüft. Sie scheitern ohne Änderungen, falls Nachbarobjekte kollidieren oder
Arbeitsplattenobjekte ihre Unterstützung verlieren würden. Vorhandene Dekoration
muss deshalb vor dem Verschieben ihres Unterbaus versetzt werden. Wird ein
Unterbau ausdrücklich gelöscht, werden die von ihm getragenen Dekorationen mit
entfernt; Hängeschränke bleiben erhalten.

Der erste Start ist leer. Jede bestätigte Änderung schreibt sofort einen einzelnen
188-Byte-Datensatz in die vorhandene Preferences-Namespace `leap-games`, Schlüssel
`kitchen`. Er enthält Formatversion, Anzahl und Typ/Position/Ebene/Breite/Farbe
jedes Objekts sowie eine Prüfsumme. Keine Zeiger, native Struktur-Padding oder
Bildschirmdaten werden gespeichert. Laden prüft Format, Prüfsumme, Typen, Grenzen,
Kollisionen und Unterstützung. Fehlende/ungültige Daten ergeben eine leere Küche.
Ein fehlgeschlagener Schreibversuch wird angezeigt und nach fünf Sekunden sowie
beim Verlassen erneut versucht. Ein unbestätigtes Verschieben wird nie gespeichert.

`KitchenType` verwendet stabile IDs, die für spätere Nutzung von Herd, Spüle,
Kühlschrank usw. erweitert werden können. Neue Typen hinten ergänzen; Änderungen
am Speicherformat erfordern eine neue Version und gegebenenfalls eine Migration.
Kochfunktionen, Sounds, Punkte, Zeitlimits und Multiplayer sind nicht enthalten.

## Prüfung

`tools/test.sh` enthält native Tests für Kollisionen, breite Objekte, Ersetzen,
Verschieben, Unterstützung, Entfernen, alle 15 Typen, Farben, beide Taster,
Zeichengrenzen, echte gesampelte 2-s-Tasterereignisse, beide Ausgänge,
Reset/Abbruch sowie beschädigte und unvollständige Speicherstände. Die vorhandene
Games-Lifecycle-Prüfung testet zusätzlich echtes Speichern über Preferences,
Wiederherstellung in einer neuen Games-Instanz und Verlassen während des Verschiebens.
Der CI-Workflow baut weiterhin die ESP32-S3-Firmware mit den gepinnten Bibliotheken.

Auf echter Hardware noch prüfen: Erkennbarkeit der Objekte und Farben,
Tastergefühl beim Blättern, beide Ausgänge, Reset-Bestätigung,
Wiederherstellung nach Neustart und Bearbeiten einer dicht eingerichteten Küche.

Validierung des ursprünglichen Editors: Die GitHub-Actions-Prüfung für PR #12
bestand einschließlich der vollständigen nativen Test-Suite und des ESP32-S3-Builds.
Die vollständige native Suite besteht lokal auch mit den neuen Regressionen für
die erweiterten Tasterereignisse, beide Ausgänge und das bestätigte Leeren. Lokale Tests verwenden die
gepinnten ArduinoJson-7.4.2-Quellen; das Verhalten auf dem echten Gerät muss noch
geprüft werden.

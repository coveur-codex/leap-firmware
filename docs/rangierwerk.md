# Rangierwerk

Rangierwerk zeigt den Güterbahnhof als schematischen Netzplan. Stelle eine Weiche, fahre mit
der Lok an einen freien Wagen heran und bringe ihn zum Gebäude mit demselben
Gütersymbol. Wagen können vorne und hinten an der Lok hängen. Bis zu vier
Wagen passen an eine Lok. Bei blockierter Strecke hält der Zug an.

Aufträge zeigen Güterart, Ziel und Fortschritt. Ein passender Wagen wird
gebucht, sobald er auf einem markierten Abschnitt des Zielgleises abgestellt
und von der Lok getrennt ist. Das Gleis blinkt beim Entladen kurz. Abgeschlossene
Aufträge werden durch neue ersetzt. Das Zulaufgleis liefert bei Bedarf Nachschub.

## Bedienung

| Schalter | Aktion |
| --- | --- |
| Rechts hoch / runter halten | Vorwärts / rückwärts fahren. Beim Loslassen anhalten. |
| Rechts Mitte im Stand | Kupplungsmenü öffnen |
| Rechts links / rechts im Kupplungsmenü | Beliebige Kupplung wählen, auch vor der Lok |
| Rechts Mitte / runter im Kupplungsmenü | Trennen / ohne Trennung zurück |
| Links links / rechts | Vorherige / nächste Weiche wählen |
| Links Mitte kurz | Markierte Weiche umstellen, falls sie frei ist |
| Links hoch / runter | Vorherigen / nächsten Auftrag anzeigen |
| Links Mitte halten | Spielmenü mit Weiter, Lok bergen und Verlassen. Allein ist auch „Neue Karte“ verfügbar. |

Beim Start ist die Weiche am eigenen Lokschuppen markiert. Stelle sie auf den
Abzweig, damit die Lok auf das Hauptgleis kommt. Zum Abstellen eines falschen
oder blockierenden Wagens eignet sich ein freies Abstellgleis. „Lok bergen“
holt die eigene Lok in den Schuppen zurück und lässt ihre Wagen an Ort und
Stelle. Zwei Sekunden links Mitte sperren das Gerät wie in der restlichen
Firmware.

## Allein und gemeinsam

Im Startmenü wählt der rechte Schalter mit hoch/runter eine Zeile und mit
links/rechts den Wert. Rechts Mitte startet. „Allein“ funktioniert vollständig
offline, auch auf einem Gerät, das noch nie synchronisiert wurde. Mit gültiger
Gerätekonfiguration muss Rangierwerk unter **Spiele** freigegeben sein. Standard
sind **Kleiner Bahnhof** und **10 Minuten**. Spielzeiten sind 5, 10, 15 und 20
Minuten oder unbegrenzt. Nach Ablauf zeigt das Spiel die gelieferten Wagen und
bietet mit rechts Mitte einen neuen Bahnhof an.

| Stufe | Bahnhof | Offene Aufträge allein |
| --- | --- | --- |
| 1 | Kleiner Bahnhof mit direkten Rangierwegen | 1 |
| 2 | Rangiermeister mit weiteren Abstellgleisen und ersten blockierenden Wagen | 2 |
| 3 | Güterbahnhof mit zusätzlichen Weichen und Zielgleisen | 3 |
| 4 | Großer Rangierbahnhof mit dichter belegten Abstellgleisen | 3 |

„Gemeinsam“ verbindet das Gerät mit dem Homeserver. Bis zu vier Geräte spielen
sofort im vorhandenen Bahnhof. Die Lok trägt die Geräte-Akzentfarbe und eine
Nummer, die zu **Lokomotiven** im Spielmenü passt. Die eigene Lok hat einen
hellen Ring. Drei Aufträge stehen allen offen. Ein vierter kommt beim Beitritt
des vierten aktiven Geräts hinzu. Der angezeigte Auftrag ist auf jedem Gerät
frei wählbar. Geparkte Wagen lassen sich von anderen Loks übernehmen.

Nach zehn Minuten mit mindestens einem verbundenen Gerät erscheint kurz das
gemeinsame Lieferergebnis. Danach setzt der nächste Bahnhof alle Loks in ihre
Schuppen. Ohne Spieler pausiert der Timer. Bei einer Unterbrechung hält die Lok
an und der Platz bleibt 60 Sekunden reserviert. Verlassen über das Spielmenü
gibt den Platz frei. Die Homeserver-Oberfläche **Spielestatus** zeigt die Welt
und stellt die Schwierigkeit für die nächste Karte ein.

## Technik

Der seedbasierte Generator verbindet ein freies Umfahrgleis mit Abstellgleisen,
Einfahrt, Zielgleisen und Lokschuppen. Seed und Stufe variieren Länge, Lage
der Weichen, Verbindungen, Ladung und Aufträge. Vor der Freigabe prüft er
beide Fahrtrichtungen des Hauptumlaufs und die Rangierwege von jedem Stichgleis,
einschließlich Ausfädeln, Wenden und Schieben mit zwei Wagen. Wagen starten
auf den Abstellgleisen. Fehlschläge werden wiederholt und verwenden
anschließend ein festes, geprüftes Grundlayout. Die nativen Tests fahren
Lieferwege aus den erzeugten Anfangsstellungen.

Firmware und Homeserver kompilieren dieselbe Datei `rangierwerk/Yard.h`.
Verbände bestehen aus benachbarten Fahrzeugen mit gegenseitigen Kupplungen,
stabilen IDs und genau einer belegten Schienenposition je Fahrzeug. Offline
läuft die Simulation im Gerät. Online ist der Homeserver verbindlich und
überträgt den Kartenseed, den Weichenstand, Fahrzeuge und Aufträge per
WebSocket. Empfangene Zustände werden auf dem Display interpoliert.
Fahrbefehle laufen bei fehlender Erneuerung nach 450 ms ab.

Die Firmware projiziert die Karte auf horizontale Hauptgleise, diagonale
Abzweige und gerundete Kehrschleifen. Fahrzeuge und Kupplungen folgen
derselben Geometrie wie die gezeichneten Gleise. Gütersymbole und getönte
Lokomotiven bestehen aus kleinen GFX-Zeichenprimitiven. Dadurch stehen sie
bereits beim ersten Offline-Start zur Verfügung. Beim Abgleich beider
Repositories müssen die Kopien der C++-Datei bytegleich bleiben.

[Homeserver-Konfiguration und WebSocket-Protokoll](https://github.com/coveur-codex/leap-homeserver/blob/main/docs/rangierwerk.md)

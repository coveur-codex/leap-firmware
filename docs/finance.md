# Finanzen · Firmware 1.2.0

Benötigt Homeserver 1.2.0. Dort die Seite **Finanzen** aktivieren und bis zu
48 Symbole pro Gerät konfigurieren. Zuerst den Server aktualisieren.
Alle Anbieteranfragen und Keys bleiben am Server. Keine neuen Grafikassets.

Rechts bleibt der normale Bereich x=86…427; die globale Sidebar bleibt erhalten.
Die Übersicht zeigt maximal drei Titel, Kurs, Währung, Originalzeit und Änderung
gegen die Tageseröffnung. Pfeil und Vorzeichen ergänzen Grün/Rot/Grau.
Lange Texte werden gekürzt, große Kurse mit M/B/T kompakt formatiert.

Rechter Taster:

- Übersicht Links/Rechts: vorheriger/nächster Titel, einschließlich Seitenwechsel.
- Mitte: Details zum ausgewählten Titel.
- Details Hoch/Runter: vorheriger/nächster Titel mit Umlauf.
- Details Links/Mitte: Übersicht mit zuvor gespeicherter Auswahl und Seite.

Übersichtsrotation alle 10 Sekunden ab Betreten, nur bei mehr als drei Titeln.
Nach Bedienung 30 Sekunden Pause, anschließend ein normaler 10-Sekunden-Takt
(erster Wechsel 40 Sekunden nach dem Tastendruck). In Details keine Rotation.
Die linke Navigation und alle globalen Funktionen bleiben erhalten.

`Network` liest `/finance` beim normalen Sync und zusätzlich alle fünf Minuten
im vorhandenen Hintergrundtask. Daten werden mit `Storage::commit` atomar im
bestehenden Offline-Snapshot gespeichert. Rendering und Eingaben führen keine
Netzwerk- oder Flashzugriffe aus. Die Datenquelle aktualisiert den aktuellen
Stand abhängig von ihren Quoten; Geräte erzeugen keine zusätzlichen API-Aufrufe.

Offline, fehlende bestätigte Uhrzeit, abgelaufener Datenstand und technische
Fehler zeigen `! Veraltet`. Anbieter-Verzögerungen zeigen `Verzögert`.
Ein geschlossen gemeldeter Markt zeigt zusätzlich einen gezeichneten Mond.
Bei unklarem Status kein erfundener Mond. Datum/Uhrzeit und Nachrichtenzeit
stammen vom Anbieter in Börsenzeitzone, nicht von der Geräteabrufzeit.

Die Detailkurve verwendet Originalzeitabstände, dynamische Y-Skalierung und
Segmentkennungen für echte Datenlücken. Tageshoch/-tief kommen separat vom Server.
Ein einzelner Punkt bleibt ein Punkt, fehlende Daten werden benannt. Das
kostenlose Finnhub-Profil liefert **Gesammelte Kurse**, keine historische
Intraday-Rückbefüllung; internationale Märkte/Indizes sind dort nicht enthalten.
Der Server dokumentiert den optionalen weiteren Anbieter und dessen Tarifgrenzen.

`tools/test.sh` enthält Tests für 0/1/2/3/4/6/7 Titel, Rotationszeiten, manuelle
Pause, Auswahl über Grenzen, Details, Rückkehr, Umlauf, schrumpfende Listen,
Zeitgeberüberlauf und Stale-Zustände. Die echte `FinanceDraw.cpp` wird zusätzlich
für bis zu 48 Titel, Kurse, lange Texte, Datenlücken und fehlende/konstante Charts
gezeichnet; alle Zeichenoperationen müssen innerhalb x=86…427/y=0…131 bleiben.
Reale Lesbarkeit, Farben und Tasten anschließend am Gerät abnehmen.

# Mein Zimmer (1.5.0-beta.1)

Benötigt den LEAP-Homeserver mit Zimmer-API und Migration 0022.
Einrichtung, Zuordnungen und vollständige Abnahme stehen im Homeserver unter
`docs/home-assistant.md`. Home Assistant wird ausschließlich vom Homeserver
angesprochen; auf dem Gerät werden keine HA-Zugangsdaten gespeichert.

Rechter Schalter: hoch/runter wählt Heizung/Lampe, links/rechts ändert den
Heizungs-Sollwert um 1 °C bzw. schaltet eine Lampe aus/an. Mitte öffnet bei
RGB-Lampen die Farbauswahl; links/rechts wählt die Farbe, Mitte sendet sie.
Hoch/runter verlässt die Farbauswahl. Links bleiben Menü und Seitenwechsel.

Live-Daten werden nur während geöffneter Zimmerseite alle drei Sekunden im
Netzwerktask abgefragt und im RAM gehalten. Nach zehn Sekunden ohne frische
Daten oder ohne WLAN sind Befehle gesperrt. Es gibt genau einen ausstehenden
Befehl; weitere Schritte nach Abschluss erneut drücken. Keine Offline-Aufträge,
Flash-Schreibvorgänge oder automatischen Wiederholungen für Zimmerbefehle.

Vor dem Rollout: ESP32-S3-Arduino-Build, Displaydarstellung auf 428×142,
rechte Tasten, gemeinsame Erik-/Lars-Heizung, Grenzen, RGB und WLAN-Ausfall
am echten Gerät prüfen. Native Tests überprüfen Bedienlogik, Offline-Sperre,
Zeitüberlauf und Geräteauswahl; sie ersetzen keinen Hardwaretest.

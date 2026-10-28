# Mein Zimmer (1.5.0-beta.2)

Benötigt LEAP-Homeserver **1.5.0-beta.2** mit Zimmer-API und Migration 0022.
Zuerst den Homeserver aktualisieren, dann die Firmware. Keine neue Migration.
Einrichtung und Zuordnungen: Homeserver `docs/home-assistant.md`.
Home-Assistant-Zugangsdaten bleiben ausschließlich am Homeserver.

## Bedienung

- Rechter Schalter hoch/runter: Heizung oder Lampe auswählen.
- Heizung links/rechts: lokale Zieltemperatur −/+1 °C; mehrere Schritte sammeln.
- Lampe links/rechts: die Farbe der Kachel wechseln. Schwarz bedeutet Aus.
- Zehn Farben: Weiß, Gelb, Orange, Rot, Rosa, Lila, Blau, Hellblau, Türkis, Grün.
- Reine Dimmer ohne RGB wechseln nur zwischen Schwarz (Aus) und Weiß (An).
- Nach **1 s ohne weitere Wertänderung**, auch über mehrere Zeilen hinweg,
  wird die Auswahl automatisch übertragen. Mitte ist nicht erforderlich.
- Der linke Schalter navigiert weiterhin zu anderen Seiten und zum Menü.

Die Vorschau reagiert sofort. Ein Stern kennzeichnet einen noch nicht bestätigten
Wert. Statusabfragen überschreiben keine offenen Eingaben. Weitere Änderungen
während eines Requests werden nach Ablauf der Pause als neuer Endwert gesendet;
mehrere betroffene Geräte werden nacheinander übertragen. Das Verlassen der
Seite verhindert die bereits gewählte Übernahme nicht.

Live-Zustände werden bei geöffneter Zimmerseite alle drei Sekunden im RAM
aktualisiert. Ohne WLAN oder nach zehn Sekunden ohne frische Daten werden
ungesendete Änderungen verworfen, nicht später beim Wiederverbinden ausgeführt.
Fehlgeschlagene Befehle werden nicht automatisch wiederholt. Eine unbestätigte
Vorschau wird nach 15 Sekunden aufgegeben und als Fehler gekennzeichnet.
Die Farben anderer HA-Bedienungen werden aus RGB-Werten der nächsten Kachelfarbe
zugeordnet; unbekannte Farben bleiben bis zur eigenen Auswahl als An sichtbar.

Native Tests: einsekündige Pause, Mehrfacheingaben, Grenzen, unabhängige Lampen,
Statusabfragen, Eingaben während Requests, verzögerte Bestätigung, Rückkehr zum
alten Sollwert, Fehler, Offline-Verhalten, Zeitüberlauf und zehn Farben.
Vor dem Rollout beide ESP32-Builds und die Bedienung auf 428×142 prüfen.

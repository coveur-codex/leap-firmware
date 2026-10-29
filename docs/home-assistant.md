# Mein Zimmer (1.5.1)

Benötigt LEAP-Homeserver **1.5.1** mit Zimmer-API und Migration 0023.
Zuerst den Homeserver aktualisieren, dann die Firmware. Homeserver-Migration 0023 ergänzt Szenenzuordnungen.
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

## Bunte Szenenkachel

Bei `sceneSupported: true` erscheint nach Grün eine regenbogenfarbene Kachel
**Bunt**; auch von Schwarz aus mit links erreichbar. Sie sendet nach 1 s Pause
`action: scene` für die gewählte Lampe. Die Szene wird im Homeserver pro Lampe
konfiguriert; für Annas Lichterkette ist `scene.anna_lichterkette_szene` vorbelegt.
Ohne Zuordnung wird die Kachel übersprungen. Auch Lampen ohne RGB können eine
Szenenkachel erhalten. Es werden keine freien Szenen-IDs vom Gerät übertragen.

Nach erfolgreichem Aufruf erscheint **Szene gesendet**. Das ist ein lokaler Hinweis
auf die angenommene Aktion, keine Bestätigung des physischen Tuya-Modus. Ein
später beobachtetes Aus, eine feste Farbauswahl oder das Zurücksetzen der lokalen
Verbindung/Vorschau entfernt den Hinweis. Ein Geräte-Neustart erkennt bereits
laufende externe Szenen nicht automatisch.

Eine neue Farbe oder Schwarz/Aus nach Bunt wird explizit gesendet, auch bei
verzögertem HA-Zustand. Fehler/Offline-Eingaben werden nicht automatisch wiederholt.
Die bisherigen zehn Farben, Heizungssteuerung und Eingabepause bleiben erhalten.

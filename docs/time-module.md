# Zeit-Modul

Homeserver 1.1.3 mit Migration 0018 zuerst installieren, danach Firmware 1.1.4 installieren
und synchronisieren. Die Seite `time` („Zeit“) ist für neue und vorhandene Geräte
aktiviert und lässt sich im Geräteeditor abschalten oder verschieben.

Rechts HOCH/RUNTER wechselt Timer/Stoppuhr. Links bleiben die gewohnten
Seiten- und Menütasten verfügbar. Beide Messungen können gleichzeitig laufen.

- Timer: rechts LINKS/RECHTS wählt im Ausgangszustand 1, 3, 10, 60 Minuten oder
  „Eigene Zeit“. MITTE startet oder pausiert. RECHTS setzt eine laufende oder
  pausierte Zeit zurück; danach kann eine andere Vorgabe gewählt werden.
- Eigene Zeit: MITTE öffnet den Editor, LINKS/RECHTS wechselt Minuten/Sekunden,
  HOCH/RUNTER ändert den Wert (1 Sekunde bis 99:59), MITTE startet.
- Stoppuhr: MITTE startet/pausiert, RECHTS speichert eine Zwischenzeit
  (Gesamtzeit und Abstand zur vorherigen Runde, höchstens 20).
  LINKS blättert während der Messung durch ältere Runden und setzt im pausierten
  Zustand zurück. RECHTS blättert im pausierten Zustand durch ältere Runden. Anzeige auf Hundertstelsekunden, Bildschirmaktualisierung 100 ms.

Der Ring aus 60 Punkten leert sich in der Geräte-Akzentfarbe im Uhrzeigersinn.
Die Messung nutzt monotone Millisekunden und läuft auf anderen Seiten, in Spielen,
in Ruhezeit, gedimmt und gesperrt weiter, auch ohne WLAN. Uhrzeitkorrekturen und
der millis()-Überlauf beeinflussen die Dauer nicht. Kein Deep-Sleep; Ausschalten
oder Neustarten verwirft beide Messungen und Zwischenzeiten.

Beim Ablauf zeigt das gemeinsame Benachrichtigungssystem sofort für zehn Sekunden
„Zeit ist um!“ in der Akzentfarbe und mischt vier kurze Warntöne in laufendes Audio.
Der Timer hat Vorrang vor wartenden Hinweisen. Der Seitenmarker bleibt bis zum
Öffnen bestehen. Im Standby wird die Zeit-Seite geöffnet; aktive Nutzung bleibt
auf ihrer Seite. Lautstärke 0, deaktivierter Timer-Ton und Nachtruhe bleiben stumm.
Ein abgelaufener Timer klingelt genau einmal, bis erneut gestartet wird.

## Abnahme am Gerät

1. 1-Minuten-Timer starten, Spiel/Chill öffnen bzw. sperren: Ablauf, Aufhellen,
   Karte und Warnton prüfen. Das Spiel darf nicht stoppen; im Standby Zeit-Seite.
2. Dasselbe offline, mit laufender Musik/WAV sowie mit Lautstärke 0/Nachtruhe.
3. Pause, Fortsetzen, Reset und eigene Zeit 00:01 / 99:59 prüfen.
4. Parallel Stoppuhr starten, Runden nehmen, Seite verlassen, zurückkehren,
   pausieren und zurücksetzen; Akzentfarbe und Lesbarkeit auf dem Panel prüfen.

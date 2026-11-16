# Lernwelt und SilbenTrainer (Firmware 1.9.0)

Lernwelt (`learning`) ist eine normale, über den Homeserver geordnete und
abschaltbare Hauptmenü-Seite mit Buchsymbol. Sie enthält Mathe-Labor und
SilbenTrainer. Homeserver 1.8.0 mit Migration 0026 zuerst installieren, dann
Firmware 1.9.0. Beide Apps brauchen während der Übungen keine Verbindung.

## Bedienung

In der Lernwelt rechts Hoch/Runter wählen, Mitte öffnet die Anwendung.
Der SilbenTrainer beginnt mit einer kleinen Startansicht: Starten, Modus,
Schwierigkeitsstufe, feste/zufällige Reihenfolge, Wechselzeit (Aus/5/10/15 Sekunden),
Rundenlänge (10/15) und Wiederholen. Hoch/Runter wählt die Zeile,
Links/Rechts oder Mitte ändert die Einstellung; Mitte startet eine Runde.
Nur am Server freigegebene Stufen sind wählbar. Standard: leicht, Modus 1,
10 Aufgaben, Zufall, automatischer Wechsel aus.

| Rechter Schalter beim Üben | Funktion |
| --- | --- |
| Links / Rechts | Vorige / nächste Aufgabe; am Anfang bleibt die erste Aufgabe |
| Hoch / Runter | Modus wechseln, neue Runde im gewählten Modus |
| Mitte kurz | Silbentrennung umschalten (Modus 2/3) |
| Mitte 0,9 s halten | Aktuelle Aufgabe merken / wieder entfernen |

Links bleibt global: Links/Rechts wechselt die Region, Mitte zurück zur
App-Auswahl, Mitte halten sperrt. Auch im Mathe-Labor sind linkes Links/Rechts
globale Seitenwechsel; dessen übrige lokale Bedienung bleibt erhalten.
Am Rundenende startet Mitte eine weitere Runde; rechts Links zeigt die letzte
Aufgabe erneut. Kein Punktestand, keine Bewertung oder Zeitwertung.
Der optionale Timer läuft nur bei sichtbarer Übung und stoppt am Rundenende.
Jede Eingabe startet das Wechselintervall neu. Die normale globale
Inaktivitätssperre bleibt aktiv; automatische Aufgabenwechsel zählen nicht als Eingabe.

Bis zu 32 Merkeinträge werden im vorhandenen NVS-Namensraum `leap-ui` unter
`syllable-repeat` gespeichert. Schreibfehler werden alle fünf Sekunden erneut
versucht. Wort- und Silbenmerkliste sind getrennt, die zwei Wortmodi teilen
Wortmarkierungen. „Wiederholen“ startet eine Runde ausschließlich mit gemerkten,
zur gewählten Stufe und zum Modus passenden Einträgen. Bei leerer Auswahl bleibt
die Startansicht offen. Bei nur einem passenden Eintrag sind Wiederholungen
unvermeidbar; sonst gibt es keine direkt aufeinanderfolgenden gleichen Aufgaben.
Gelöschte eigene Inhalte bleiben als harmlose Merkschlüssel erhalten und werden
nicht als Aufgaben erzeugt. Lokale Übungsoptionen gelten bis zum nächsten Öffnen;
Serverwerte sind die Startwerte. Ein unveränderter Hintergrund-Sync erhält die Runde.

## Darstellung und Daten

Die 86 Pixel breite Sidebar bleibt sichtbar. Rechts (342×142) steht jeweils
eine Aufgabe mit der vorhandenen 6×8-Schrift in Skalierung 2–6 (16–48 Pixel).
Die längste erlaubte Aufgabe passt einschließlich Bindestrichen ungekürzt.
Getrennte Silben wechseln zwischen lesbarer Akzentfarbe (bei dunkler Farbe Weiß)
und Gelb; sichtbare Bindestriche erhalten die Bedeutung ohne Farberkennung.
Vollständige Wörter sind weiß. Modus und Position erscheinen klein am oberen Rand.

`SyllableData.h` enthält 72 Silben und 29 Wörter mit expliziter Gliederung und
Stufe 1–3. Leicht: offene Silben und einfache zweisilbige Wörter;
Mittel: geschlossene Silben und längere Wörter; Schwer: Konsonantenverbindungen,
SCH/CH, Umlaute und längere Wörter. Keine automatische Silbentrennung.
Inhalte am Server pädagogisch prüfen; eine strukturelle Prüfung kann keine
sprachlich falsche Gliederung erkennen.

Die Konfiguration liegt in `config.pages[id=learning].settings`:

```json
{
  "applications": [
    {"id": "math_lab", "enabled": true},
    {"id": "syllable_trainer", "enabled": true}
  ],
  "syllableTrainer": {
    "levels": [1, 2, 3],
    "defaultMode": 0,
    "sessionLength": 10,
    "random": true,
    "autoSeconds": 0,
    "syllables": [{"text": "WE", "level": 1}],
    "words": [{"word": "WIESE", "syllables": ["WIE", "SE"], "level": 3}]
  }
}
```

Modi sind nullbasiert 0–2, Stufen 1–3. Je höchstens 32 zusätzliche Silben/Wörter;
Großbuchstaben A–Z/ÄÖÜ, Silben bis 6, Wörter bis 16 Buchstaben und 2–5 Silben.
Die Silben müssen zusammen exakt das Wort ergeben. Ungültige Einträge ignoriert
die Firmware, doppelte Wörter ersetzen keine vorhandene Gliederung. Fehlende
oder ungültige Optionen fallen auf lokale Standardwerte zurück.
Weitere Anwendungen ergänzen die bestehende App-Auswahl in `LearningSelection.h`
und die entsprechenden Start-/Zeichenpfade; keine zusätzliche Paketverwaltung.
Eigene Inhalte verwenden den normalen Konfigurations-Snapshot im LittleFS.

## Mathe-Labor und Übergang

Engine, Einstellungen, `config.mathLab.targets` und bestehende NVS-Daten bleiben
unverändert. `math_lab` wird nicht mehr in Spiele angezeigt. Bei einer alten,
offline gespeicherten Konfiguration ohne `learning`-Seite erzeugt die Firmware
eine Lernwelt nur dann, wenn Spiele und `math_lab` bisher freigegeben waren.
Eine explizite Lernwelt-Seite (auch deaktiviert) ersetzt diesen Übergang und
wird nie durch alte Spielefreigaben überstimmt. Der SilbenTrainer erfordert
eine explizite Freigabe, seine mitgelieferten Inhalte funktionieren dann offline.

## Prüfung und Hardware-Abnahme

`tools/test.sh`: Wortdaten, Grenzen, Freigaben/Alt-Konfiguration, Moduswechsel,
Vor-/Zurückhistorie, feste Reihenfolge, Zufallsrunden ohne direkte Wiederholung,
Merkliste/Neustartrepräsentation, Timer inklusive millis-Überlauf und Pause,
lokale/zusätzliche Daten und sämtliche Zeichen-/Textgrenzen.
Die bisherigen Mathe-Labor-Tests bleiben unverändert.

Am ESP32-S3 N16R8 mit 428×142-Display prüfen: alle drei Modi einschließlich
SCHMET-TER-LING und BÜ-CHER, dunkle Akzentfarbe, wechselnde Farben/Bindestriche,
0,9-s-Merkfunktion ohne doppeltes Umschalten, linke Navigation/Sperren,
10/15-Aufgaben-Runden und 5/10/15-s-Wechsel, Markierungen nach Neustart,
Abschaltung per Sync, WLAN aus, unveränderter Sync während einer Runde und
Mathe-Labor mit bisherigen Zielzahlen. Cloudtests ersetzen diese Abnahme nicht.

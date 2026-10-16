# Mathe-Labor · Firmware 1.1.1

`math_lab` ist genau ein Eintrag in der bestehenden freigegebenen Spieleliste.
Homeserver 1.1.1 ergänzt die Registry; im Geräteeditor aktivieren und synchronisieren.
Bestehende Gerätefreigaben bleiben unverändert. Danach sind alle Übungen offline
verfügbar. Sidebar, Uhr und Avatar bleiben erhalten; die Aktivitäten verwenden
x=94…427 und y=0…141. Keine Bildpakete, Netzwerkzugriffe, Töne, Ranglisten,
Leben oder Bearbeitungsfristen. Die übliche Gerätesperre bei Inaktivität bleibt.

## Bedienung

Rechts hoch/runter wählt eine Aktivität und die Einstellungszeile. Rechts links/rechts
ändert Variante, Stufe und bei Bauaufgaben die Merkzeit (3–20 Sekunden).
Rechts Mitte startet aus jeder Einstellungszeile. Links Mitte geht eine Ebene zurück:
Übung → Einstellungen → Laborauswahl → Spiele. Links Mitte halten sperrt wie gewohnt.
Alle Stufen sind sofort wählbar. Die Fußzeile zeigt die kontextabhängige Bedienung.

Während einer Übung: links rechts erzeugt eine neue Aufgabe (überspringen), links
links zeigt Hilfe. Rechts Richtungen ändern die ausgewählte Menge/Antwort; rechts
Mitte prüft. Kein automatischer Aufgabenwechsel nach Erfolg: weiter ausprobieren!

- **Würfellabor:** Drei Varianten: zählen, Kombinationen, Häufigkeiten. Links runter
  wechselt beim Zählen zwischen einem/zwei Würfeln und bei Kombinationen den zu
  ändernden Würfel. Links rechts würfelt neu. Kombinationen gleicher Summe bleiben
  zum Experimentieren bestehen; vertauschte Würfel zählen als dasselbe Paar.
  Im Experiment würfelt rechts Mitte, links rechts leert die Sammlung.
  Balken sind relativ zum häufigsten Ergebnis skaliert, darunter die Summen 2–12.
  Es werden keine Häufigkeitsversprechen aus wenigen Würfen abgeleitet.
- **Zahlenwerkstatt:** Mengen erkennen, zwei frei editierbare Teile einer Zahl,
  Mengenvergleich (<, =, >) und Zehner/Einer. Links runter wechselt den editierbaren
  Teil beziehungsweise Zehner/Einer. Alle nichtnegativen Zerlegungen sind gültig,
  einschließlich 0 + n und n + 0. Freie Zahlenräume 5, 10, 20, 100.
  Punkte sind in Fünferreihen/Zehnerrahmen geordnet. Jede Zehnergruppe enthält zehn
  sichtbare Punkte; bis zu zehn Zehner und neun Einer sind einzeln veränderbar.
- **Würfelarchitekt:** Rechts Richtungen wählen das Rasterfeld, rechts Mitte fügt
  einen Würfel hinzu (nach Höhe 3 wieder 0); links runter entfernt einen.
  Links hoch vergleicht. Gelbe Stapel markieren abweichende Positionen in Vorlage
  und Nachbau; leere Positionen bleiben am Grundraster sichtbar.
  Nachbauen zeigt die Vorlage dauerhaft. Merken blendet sie nach der frei
  gewählten Betrachtungszeit aus; rechts Mitte kann früher zum Bau wechseln.
  Genau hinschauen nutzt mehr Würfel; links links ruft die Vorlage erneut auf.
  Im Merkmodus kann die Aufgabe jederzeit neu gestartet werden.
- **Würfeldetektiv:** Fehlende Kante und falsch gerichtete Kante nutzen denselben
  isometrischen Würfel mit drei, sechs beziehungsweise neun nummerierten sichtbaren Kanten. Rechts wählt eine
  Kantennummer; Mitte prüft. Die Hilfe ergänzt/korrigiert die Kante farbig.
  Richtige Ansicht zeigt ein Bauwerk und drei Draufsichten: die Zahlen bezeichnen
  die Stapelhöhen, einschließlich 0 für leere Felder. Exakt eine stimmt überein.

## Architektur und Prüfungen

`MathLabLogic.h` ist ohne Arduino testbar: unverzerrte Würfelzufallszahlen über
Rejection Sampling, Mengenlogik, Höhenfelder, gemeinsame Projektion und Kanten.
`MathLab` verwaltet Auswahl, Einstellungen und Übung. Der bestehende `Games`- und
`Ui`-Lebenszyklus bleibt zuständig. Die drei Raumstufen verwenden 2×2 bzw. 3×3
Felder und höchstens drei Würfel pro Stapel. Die Vorlage ist monoton zum Betrachter
abgestuft, um verdeckte Stapel zu vermeiden. Bauvergleiche prüfen alle neun Höhen,
keine Pixel. Ansichts-Distraktoren verändern verschiedene Stapel eindeutig.
Zustand und Häufigkeiten sind begrenzt und bleiben nur während der Sitzung im RAM.

`tools/test.sh` führt auch `test_math_lab.cpp` aus: sämtliche Zerlegungen bis 100,
36.000 Würfe, 3.000 Bauwerke, Kanten, eindeutige Ansichten, Navigation, Zeitüberlauf
und Grafikgrenzen über alle Varianten/Stufen. Eine echte Display-/Schalterabnahme
bleibt erforderlich: Augenbilder, Kantennummern und 5×2-Felder auf dem kleinen
Panel prüfen; Original/Nachbau und Hilfefarben auch bei leerem Stapel testen.
Die Kantenübungen beginnen bewusst mit Einzelwürfeln; die drei Stufen erweitern dort
die Anzahl der auswählbaren Kanten. Bei Draufsichten steigt die Gebäudegröße mit der Stufe.


Ab 1.1.2 kommen Menüauswahl, Titel, Mengen und Bauwerke aus der konfigurierten
Geräteakzentfarbe. Der Text ausgewählter Menüzeilen wechselt abhängig von deren
Helligkeit zwischen hell und dunkel. Die beiden Würfel behalten unabhängig von
der Akzentfarbe dunkle Augen auf Weiß, beim zweiten Würfel dunkelviolett.

# Haustier und Snake: Asset-Vertrag und Abnahme

Firmware `1.0.0-beta.12` und zugehöriger Homeserver. Beide neuen IDs werden in
`config.games` angeboten; bestehende Spiele bleiben erhalten. Kein neues Netzwerk-
oder Speicherprotokoll. Pet-Zustand und Snake-Rekord sind lokal im NVS-Namespace
`leap-games` (`pet`, versionierter 5-Byte-Wellbeing-Datensatz; `snake-best`, Integer).
Ungültige/fehlende Datensätze erhalten die kinderfreundlichen Standardwerte.
Ein Sync ersetzt diese Spielstände nicht.

## Avatarpaket

Das gewählte `avatar-*`-Paket enthält wie bisher `definition.json`, PNG-Dateien
und optional Web-Vorschauen. Das Beispiel-ZIP war bei der Implementierung nicht
hochgeladen. Der Import unterstützt die beschriebene Struktur mit äußerem Ordner,
`data/pet/STATE/*.png` oder `STATE/*.png` sowie
`background_day.png`/`background_night.png` oder gleichnamigen Hintergrundordnern.
Pfade bleiben unverändert. Uploads ergänzen die reguläre Paketversion; keine
separaten Tamagotchi-Downloads. Ein PNG-Idle wird auch für die Sidebar referenziert,
wenn diese bislang keine Idle-Animation hatte. Bestehende Sidebar-Animationen und
explizite Metadaten bleiben erhalten. Pro Version gelten weiterhin alle Hash-,
Pfad-, Speicher- und Aktivierungsprüfungen.

```json
{
  "tamagotchi": {
    "backgrounds": {"day": "background_day.png", "night": "background_night.png"},
    "animations": {
      "idle": {"frames": ["data/pet/idle/frame_01.png"], "frameDurationMs": 400},
      "eating": {"frames": ["data/pet/eating/frame_01.png"], "frameDurationMs": 400}
    }
  }
}
```

Weitere Zustände: `happy`, `sad`, `hungry`, `tired`, `dirty`, `playing`, `sleeping`.
Üblich sind je vier 80×80-Frames; variable Frameanzahlen funktionieren ebenfalls.
Waschen nutzt `happy`. Fehlende Zustandsanimationen fallen auf Pet-Idle, dann
Sidebar-Idle/Preview bzw. eine Zeichnung zurück. Hintergrund-PNGs müssen exakt
256×142 sein; alle anderen Avatar-PNGs weiterhin 80×80. Hintergründe werden über explizite Referenzen oder die eindeutigen Namen
`background_day`/`background_night` erkannt. Andere Avatar-PNGs bleiben auf
80×80 beschränkt. Bereits hochgeladene Pakete benötigen keine neue Definition:
Die Firmware erkennt Zustandsordner und die bisherigen `animations`-Einträge
aus dem unveränderten Manifest. Der Homeserver ergänzt fehlende Metadaten
einmalig als neue Paketversion beim Start bzw. nächsten Sync. Dateien, vorhandene
Paketversionen und explizite Konfiguration bleiben erhalten.
Die Firmware nutzt gecachte Manifeste und den bestehenden asynchronen Media-Worker.
Eine zusätzliche Maske lässt transparente Bereiche des Tieres frei.

## Einfaches Tamagotchi (beta.12)

Vier Balken („Satt“, „Spass“, „Sauber“, „Kraft“) zeigen die Bedürfnisse dauerhaft:
grün ab 80, gelb ab 55, orange darunter. Eine Statuszeile benennt das dringendste
Bedürfnis und bestätigt jede Pflegeaktion. Alle 30 Betriebssekunden sinken Essen,
Spaß und Energie um zwei Punkte, Sauberkeit um einen. Der Mindestwert bleibt 35;
das Tier stirbt nicht. Nach etwa acht Minuten ohne Pflege wird ein neues Tier
hungrig. Pflege füllt den jeweiligen Wert auf 100. Die unveränderten gespeicherten
5-Byte-Spielstände bleiben gültig. Passive Änderungen werden alle fünf Minuten
und beim Verlassen gespeichert, Pflege sofort. Ein harter Stromverlust kann bis
zu fünf Minuten passiver Änderungen verlieren; ausgeschaltete Zeit zählt nicht.

Die vier Paketframes werden durch leichtes Hüpfen, seitliche Spielbewegung,
Glitzern und Schlaf-„Z“ ergänzt. Schlafen nutzt während der dreisekündigen Aktion
den Nachthintergrund. Sonst gilt die lokale Uhr (20–7 Uhr, ohne Uhr Tag).
Fehlende/ladebereite Hintergründe erhalten eine lokale Landschaft als Ersatz;
sobald das Paketbild dekodiert ist, übernimmt es. Ein fehlgeschlagener Decode wird
nach zwei Sekunden erneut versucht, auch wenn sich der Blob-Pfad nicht ändert.

## Automatisierte Prüfungen

`tools/test.sh` prüft Regeln und die echte `Games.cpp` mit simulierten GPIOs,
Preferences, Zeichnen und Zeit. Dabei: vier Aktionsframes, Rückkehr zum Gemüt nach
3 s, alle Aktionen, persistenter Zustand nach neuer Games-Instanz, korrupter
Speicher, Snake-Runde/Beenden/Neustart und persistenter Rekord bereits beim Fressen.
Dies ist kein Beleg für physische NVS-Power-Cut-Eigenschaften.
`tools/test_png.sh` nutzt PNGdec 1.1.6 mit echten 80×80- und 256×142-Fixtures,
Alpha-Masken und Decoderfehlern. `tests/server_contract.py` erzeugt reale
Homeserver-Manifeste mit Pet-Hintergründen für `tests/test_core.cpp`.
Homeserver-Tests prüfen Import, Sync, Versionsschutz, Frame-Erweiterung, Löschen,
ungültige Referenzen und die Geräte-Vorschau.

## Auf dem Gerät prüfen

- Spieleliste: neue und bisherige Spiele erreichbar, Navigation links unverändert.
- Haustier: rechts UP/DOWN/CENTER, alle vier Aktionen und vier Frames pro Zustand;
  alle Gemüter mit passenden Testwerten; Tier transparent über Tag-/Nachtbild.
  Statuszeile, vier Balken und Bewegungen prüfen; acht Minuten ohne Pflege warten.
- Uhr auf 06:59/07:00 und 19:59/20:00 setzen, auch nach Offline-Neustart prüfen.
- Haustier verlassen, sperren, Seite wechseln und Gerät neu starten: Bedürfnisse
  bleiben erhalten. Mehrere Stunden ausgeschaltet verursacht keinen Abzug.
- Snake: alle vier Richtungen, Gegenrichtung, zwei schnelle Kurven, Futter,
  Wand-/Körperkollision, Neustart, Linke Mitte, Seitenwechsel und Sperren.
- Rekord übertreffen, sofort neu starten: Rekord bleibt; Runde beginnt neu.
- Während beider Spiele einen regulären Sync und eine neue Avatarversion anbieten:
  unveränderter Sync erhält Runde, Konfigurations-/Assetwechsel beendet sie sauber.
  Spielstände bleiben. Fehlende Bilder/offline: Ersatzdarstellung, bedienbar.
- Reale Bildübertragung, Tastenlatenz, NVS bei Stromverlust und Sounds abnehmen.

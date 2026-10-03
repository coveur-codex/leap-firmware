# Haustier und Snake: Asset-Vertrag und Abnahme

Firmware `1.0.0-beta.10` und zugehöriger Homeserver. Beide neuen IDs werden in
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
256×142 sein; alle anderen Avatar-PNGs weiterhin 80×80. Hintergründe werden nur
über die expliziten Referenzen erkannt, keine pauschale Lockerung der Avatarprüfung.
Die Firmware nutzt gecachte Manifeste und den bestehenden asynchronen Media-Worker.
Eine zusätzliche Maske lässt transparente Bereiche des Tieres frei.

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

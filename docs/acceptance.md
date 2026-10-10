# Hardware-Abnahme vor Rollout

Diese Liste ist eine reale Geräteprüfung, kein bereits ausgeführter Testbericht.
Benötigt werden mindestens zwei LEAP-Geräte, der aktuelle Homeserver, USB-Serial
und eine Möglichkeit, die Versorgung kontrolliert zu unterbrechen.

1. **Erststart:** Flash 16 MiB / PSRAM 8 MiB im Log; kein Warten auf USB. Frisches
   Vollständig gelöschtes FS muss sich automatisch initialisieren; belegtes/defektes
   FS nur mit beiden Mitteltasten formatieren. Netzlosen Start und Bedienung prüfen.
2. **Display:** alle vier Ränder sichtbar, keine 12-Pixel-Verschiebung, Rotation,
   RGB-Farben, Backlight-PWM und Flimmern prüfen. Die Initialisierung entspricht
   dem funktionierenden 2,79″-Test: spezielle NV3007-Sequenz, `ips=false`,
   Spaltenoffsets 12/14 und Zeilenoffsets 0/0. Schriftgröße am Gerät
   bewerten, insbesondere lange Quizantworten.
3. **Eingaben:** alle zehn Richtungen/Mitten einzeln, Prellen, Halten/Wiederholen,
   Menü, Sperren, Dimmung. Keine Nutzung von GPIO19/20 außer USB.
4. **IMU und Audio:** MPU6050 an GPIO17/18, Adresse/WHO_AM_I im Log prüfen.
   Kippen in alle vier Richtungen, Neutralstellung und Schüttelerkennung; Achsen
   bei abweichender Montage in Hardware.h korrigieren. I²C-Ausfall muss
   Schaltersteuerung erhalten. Audio: leise beginnen; links/rechts nicht vertauschte BCLK/WS/DIN prüfen,
   WAV mono/stereo 8/22,05/48 kHz, Stoppen und Lautstärke 0. Keine Analogausgabe
   oder erfundenen Amplifier-Enable-Pins.
5. **Sync:** aktive Seiten/Reihenfolge/Name/Avatar/Altersfilter ändern. Kommunikation
   deaktivieren, gleichzeitig ein unpassendes Asset zuordnen: Chat-Relay muss trotzdem
   deaktiviert bleiben. News/Wetter/Flugradar/Quiz und Wissenssuche prüfen.
6. **Offline:** nach Sync Server und AP abschalten, Neustart. Alle lokal vorhandenen
   Bereiche weiter nutzen. Uhr mit `~` kennzeichnet unbekannte Ausschaltzeit.
7. **Assets:** PNG, JPEG, 80×80-PNG-Avatar, Animation, gültiges WAV. Server-HTTP-Log:
   SVG-Dateien eines gemischten Avatarpakets dürfen nie abgerufen werden. Falscher
   Hash, fehlende Referenz, beschädigtes Bild und zu großes Paket dürfen nicht
   aktiv werden. Shared-Blob-Cleanup mit zwei Paketen prüfen.
8. **Power-Cut:** während Dateidownload, nach Datei-Flush, während Snapshot-Datei-
   Wechsel, vor/nach NVS-Zeiger und nach Serverbestätigung ausschalten. Jeweils
   gültigen alten oder vollständig gültigen neuen Datenbestand nachweisen.
9. **OTA:** gültige höhere Version auf Stable/Beta-Kanal; App-Größe <4 MiB.
   Netzabbruch während Download; falscher Hash; absichtlich falsche Chip-ID.
   Danach bisherige Version muss starten. Testrelease mit absichtlich fehlschlagendem
   Selbsttest bzw. Reset vor Bestätigung installieren: Bootloader muss auf die
   vorherige Version zurückfallen und bei Serverkontakt `rollback` melden.
   **Vor diesem Test keine Annahme über eine bereits früher geflashte Bootloader-Version.**
10. **Chat-Relay:** zwei/drei Geräte an APs mit unterschiedlichen Kanälen, aber
    demselben erreichbaren Homeserver. Vorlage senden und Namen/Icon kontrollieren.
    Neue Nachrichten bei News, Spiel, Sperrbildschirm, Ruhezeit und langem WAV
    müssen plingen und den gelben Brief anzeigen. Kommunikationsseite aufrufen:
    Brief aus; nächste eingehende Nachricht: Brief wieder an. Eigene Nachrichten
    und alte Historie nach Neustart dürfen nicht klingeln. Lautstärke 0 bleibt stumm.
    Server-/WLAN-Ausfall und Wiederkehr, verlorene POST-Antwort, volle Queue sowie
    deaktivierte Kommunikation prüfen. Serverbestätigung ist keine Lesebestätigung.
11. **Dauerlauf:** 24 Stunden mit wiederholtem Sync, Suche, Avataranimation, Audio
    und Paketwechseln. Min-Heap/PSRAM beobachten; keine ungeklärten Neustarts.
12. **Recovery:** defektes/volles FS, physisch bestätigte Formatierung und erneuter
    Sync; USB-Bootmodus und Komplettupload als letzte Wiederherstellung testen.


## WebSocket und Darstellung (1.0.8 / Homeserver 1.0.4)

- Zwei Geräte verbinden: Nachricht/Einzel-Icon auch während eines Spiels sofort
  empfangen, eigene Echos und Boot-Historie ohne Pling. WLAN trennen, währenddessen
  senden, WLAN wiederherstellen: fehlende Nachrichten genau einmal nachladen.
- Homeserver stoppen/neu starten; verlorene Sendebestätigung und volle UI-Queue
  prüfen. Wiederverbindung muss ohne Neustart funktionieren, unbestätigte eventIds
  bleiben identisch. Kommunikation im Homeserver deaktivieren: Socket schließen.
- Unter Geräte → Allgemein die Akzentfarbe ändern und Konfig-Sync abwarten;
  Navigation, Markierungen und Überschriften prüfen, auch nach Gerätestart.
- Lange News-/Wissenstitel und Flugkennungen mit ä/ö/ü/ß prüfen: keine zusätzliche
  Fettzeichnung, vollständiges Umblättern/Scrollen, kein Überlappen.
- Regen- und Flugradar mit demselben Standort prüfen: 60 × 60 km, drei Ringe
  bei 10/20/30 km, N oben mittig, Standort im Zentrum.
- Strom und Nachrichtenzustellzeit bei gutem/schwachem WLAN und verschiedenen
  AP-DTIM-Einstellungen messen. Minimum-Modem-Sleep bleibt aktiv; Keepalive 30 s,
  Task-Wartezeit 50 ms (ohne WLAN/deaktiviert 250 ms). Keine feste Funk-Schlafzeit
  oder Verbrauchseinsparung ohne Messung am Gerät annehmen.

# Abgleich mit dem Homeserver

Referenz: `leap-homeserver` Commit `8175c9f`, insbesondere `app/api/routes.py`,
`app/api/distribution.py`, `app/services/distribution.py`, `app/services/communication.py`,
`app/core/pages.py`, `app/core/layout.py`, `app/templates/device_edit.html`, Modelle
und die Distributions-/Kommunikationstests. Keine zugängliche alte Firmware oder
weitere LEAP-Chats lagen vor. Die ergänzende Nutzer-Pinliste ist die Hardwarequelle.
Der Homeserver wurde nicht verändert. Die nachgereichte Nutzerbestätigung
legt MPU6050 (GPIO17/18) und 80×80-PNG-Avatare fest.

| Bereich | Vertrag und Firmwareverwendung |
| --- | --- |
| Identität | Lokale provisionierte `deviceId`; kein erfundener Registrierungsendpunkt |
| Konfiguration | `POST /api/v1/devices/{id}/sync` → `configUrl`, danach gleiche `configVersion` prüfen |
| Inventar | `installedAssets` enthält ausschließlich lokal vollständig validierte Versionen |
| Auswahl | `desiredAssets`/`assetUpdates` vom Server, inklusive Common, Quiz und Kommunikation |
| Manifest | `schemaVersion=1`, `packageId`, `version`, `type`, `definition`, `files` |
| Dateien | Versionierte URLs, exakte Größe und SHA-256; Definition muss heruntergeladenem JSON entsprechen |
| Voraussetzungen | `minFirmware` mit numerischem Stable/Beta-Vergleich, kein lexikografischer Vergleich |
| Bereinigung | Erst `boot_success` → `cleanupAllowed` und konkrete `{packageId,version}`; aktive Version nie entfernen |
| OTA | Angebot aus gerätespezifischem Stable/Beta-Kanal; `download_started`, `firmware_installed`, nach lokalem Selbsttest `firmware_confirmed` |
| Recovery | NVS hält OTA-Sync und Zielversion; abweichende laufende Version meldet `rollback` |
| Content | GET `/sync` als Versionsübersicht, `/news`, `/weather`, `/aircraft`, `/quiz`; Offline-Snapshots |
| Zeit | `POST /checkin` → UTC `serverTime`; Batterie wird ausgelassen |
| Wissen | `/knowledge/search`, `/knowledge/random`, `/knowledge/article/{articleRef}`; Referenz nicht doppelt URL-kodieren |
| Quiz | `questionsFile`, `q`, vier `a`, `minAge`, `explanation`; Antwort 0 korrekt, UI mischt |
| Gruppenchat | `communicationEnabled`, Paket `communication-messages`, `messagesFile`; nur freigegebene Vorlagen |

Konfiguration wird getrennt von einer Pakettransaktion gespeichert. So greift eine
Kommunikationssperre, auch wenn ein neues Avatarpaket nicht passt. Das Inventar
wechselt erst nach Prüfung des vollständigen gewünschten Paketbestands. Ein
verlorenes Bestätigungs-HTTP führt nicht zu einer vorschnellen Löschung. Der
persistierte letzte Asset-Plan wird vor einem neuen Plan nochmals bestätigt, wenn
sein Inventar aktiv ist; HTTP 404/409 beendet dieses Wiederaufnehmen.

## Bewusste Grenzen und konkrete Ergänzungsvorschläge

1. **Keine definierte Bindung generischer Assets an UI-Funktionen.** `preview`,
   `animations.idle`, `questionsFile`, `messagesFile` sind klar verwendbar. Für
   Wetter-Symbole, UI-Icons, Spiellevel, Soundereignisse und weitere Avatarzustände
   fehlt ein einheitliches Schema. Vorschlag: versioniertes `bindings`-Objekt,
   z. B. `weatherCodes`, `uiIcons`, `soundEvents`, `engine` und `engineVersion`.
   Bis dahin werden ausgewählte unterstützte Pakete gespeichert; unbekannte
   Metadaten lösen keine erfundene Interpretation aus.
2. **Spiel-IDs ohne Engine-Spezifikation.** Die drei gelieferten IDs sind feste
   Serverwerte. Diese Firmware definiert einfache Schaltervarianten und verwendet
   den bestätigten MPU6050 zusätzlich zu den Schaltern. Für austauschbare Level braucht es ein Schema
   und Engine-Kompatibilitätsangaben, nicht nur Asset-Dateien.
3. **Chill ist Asset-Typ, keine PageRegistry-Seite.** Lokale Ruhezeit erscheint bei
   ausgewähltem Chill-Paket. Vorschlag: eigene `chill`-Seite mit Reihenfolge,
   Sessiondauer und optionaler Sound-/Animationsreferenz.
4. **Beliebige `pages[].settings`/`avatarConfig` ohne belegte Schlüssel.** Werden
   gespeichert, aber nicht als erfundene Helligkeits-/Schlafregeln interpretiert.
   Helligkeit/Lautstärke sind derzeit lokal. Vorschlag: typisierte Geräteeinstellungen
   inklusive Zeitzone, Display-Timeout, Lautstärkelimit und Funkkanal.
5. **Wetter-/Flugversionen sind kein zuverlässiger Frischeindikator.** Diese Daten
   werden bei jedem regelmäßigen Sync abgefragt. Vorschlag: separate serverseitige
   Cache-Revision oder ETag, das bei tatsächlich erneuerten Providerdaten wechselt.
6. **Große Pakete / Medientypen.** Der Server akzeptiert wesentlich mehr Daten und
   Formate als in Flash passen. Vorschlag: Gerätemeldung `capabilities` mit
   Formaten, Bildmaßen, `maxJsonBytes`, `maxPackageBytes`, Engines, Displaygeometrie;
   Angebote serverseitig passend filtern und nicht renderbare Pakete sichtbar markieren.
7. **ESP-NOW-Kanal und Vorlagenversion.** Es gibt keinen Kanal-/Gruppenschlüssel-
   Vertrag. V1 verwendet einen gemeinsamen Kanal, keine Relays, 32 letzte Paket-IDs
   als Deduplikationsring und acht RAM-Chatzeilen. Geräte mit unterschiedlicher
   Vorlagenversion verwerfen fremde Versionen, um gelöschte/umgedeutete Vorlagen
   nicht weiterzugeben. Vorschlag: Kanal und Gruppen-ID als Gerätekonfiguration;
   bei Authentifizierungsbedarf eigenes Schlüssel-/Pairing-Konzept.
8. **Zeit und Batteriemessung.** `serverTime` ist UTC, aber eine Gerätezonenangabe
   fehlt. Ohne belegte ADC-Schaltung weder Spannung noch Prozent liefern.
   Vorschlag: typisierte Zeitzone sowie explizites Hardwareprofil, falls später
   ein kalibrierter Spannungsteiler tatsächlich verbaut wird.
9. **Knowledge-Bibliothek.** Aktuell zuletzt geladener Artikel und Suche offline,
   nicht ein vollständiger Klexikon-Spiegel. Vorschlag: kuratierte Offline-Artikelpakete
   mit Bild-, Lizenz- und Linkreferenzen. Der Client bewahrt Quelle/URL/Lizenz.
10. **Sync-Status nach unterbrochener Bereinigung.** Der Server bestätigt nur den
    neuesten Plan und kennt alte Versionen über `previousAssets`. Ältere, vor einem
    Planwechsel nicht freigegebene Staging-Manifeste werden konservativ behalten.
    Ein explizites Geräte-Inventar aller vorhandenen Versionen plus versionierte
    Cleanup-Autorisierung würde dauerhaftes Aufräumen nach langen Fehlerfolgen
    ermöglichen. Aktive Daten werden niemals zur Platzbeschaffung geopfert.

Keine dieser Lücken wurde durch umfangreiche Homeserver-Änderungen verdeckt.

## Avatar-Geräterepräsentation

Die Server-Defaults sind noch SVG-Vorschauen. Nach der ausdrücklichen
Nutzerfestlegung überträgt die Firmware bei Typ `avatar` nur PNG und JSON.
Alle übertragenen PNGs müssen 80×80 sein und vollständig dekodierbar; Hashes und
Längen bleiben verbindlich. Das gesamte Originalmanifest wird unverändert
aufbewahrt, nicht benötigte Webdateien sind durch die deterministische
`requiredAssetFile`-Regel vom lokalen Vollständigkeitstest ausgeschlossen.
Animation/Preview oder der bestätigte `data/pet/idle/frame_01.png`-Pfad muss auf
ein enthaltenes PNG zeigen. Ein Paket ohne passenden PNG-Einstieg wird nicht
als installiert gemeldet. Dies unterstützt vorhandene gemischte Serverpakete,
ohne ein SVG herunterzuladen oder eine zusätzliche Konvertierungs-API zu erfinden.

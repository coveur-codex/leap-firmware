# WLAN, externer Server und Zahlenzerlegung · 1.3.0-beta.1

## Update und erster Test

1. Homeserver 1.3.0-beta.1 installieren (Migration `0020` wird beim Containerstart
   ausgeführt), vorher wie gewohnt das persistente Datenverzeichnis sichern.
2. Am Gerät im Server WLAN-Profile und Zielzahlen speichern. Die bisherigen
   Geräte-IDs, Seiten, Inhalte und WLAN-Daten bleiben erhalten.
3. Universelle App-Binary **ohne LocalConfig.h** als `1.3.0-beta.1` im Beta-Kanal
   hochladen. Nur `LEAP.ino.bin`, keine Bootloader-/Partitions-/Merged-Binary.
4. Ein Gerät im bisherigen Heimnetz auf Beta stellen und synchronisieren lassen.
5. Nach Neustart zunächst Heimnetz/Sync/Chat prüfen. Dann beispielsweise am
   Handy einen **2,4-GHz-Hotspot** aktivieren, dessen Zugangsdaten vorher im Server
   oder direkt am Gerät speichern und den Zugriff von dort prüfen.
6. Erst nach Hardware-Abnahme weitere Geräte aktualisieren.

Die Firmware legt beim ersten Start zusätzlich zum unveränderten alten
`leap-device/config` einen prüfsummengesicherten Datensatz in
`leap-network/profiles` an. Primäradresse ist `http://ndk.tstromi.de:8080`, die
bisherige Serveradresse bleibt als Rückweg erhalten. Bei Verbindungsfehlern
wechselt der Sync auf die andere Adresse und versucht nach zehn Sekunden erneut.
Der Chat wechselt seine WebSocket-Verbindung ebenfalls. Nach einem WLAN-Wechsel
wird sofort ein neuer Sync fällig; kein Warten auf das 15-Minuten-Intervall.
DNS/Portweiterleitung/Firewall müssen den Server tatsächlich erreichbar machen.
Diese Firmware konfiguriert den Router nicht. Im Heimnetz kann bei fehlendem
NAT-Loopback die alte lokale Adresse verwendet werden.

Die universelle Firmware ändert weder Geräte-ID noch Pinbelegung, Kalibrierung,
TLS-CA oder alte Provisionierungsdaten. Ein Firmware-Rollback kann deshalb
weiterhin die ursprüngliche Gerätekonfiguration lesen. Die neuen Profile stehen
älteren Firmwareversionen nicht zur Verfügung.

## WLAN am Server

Im Geräteeditor **WLAN und Server** öffnen. Je Gerät sind vier Profile möglich.
Erneutes Speichern derselben SSID aktualisiert ihr Passwort und schiebt sie an
den Anfang der Serverliste. Entfernen wirkt beim nächsten Sync. Die Serveradresse
ist pro Gerät änderbar. Ein Fehler beim neuen Ziel verhindert nicht den Rückweg
zur ursprünglich provisionierten Adresse.

Das Gerät ruft `GET /api/v1/devices/{id}/network-config` nach einem erfolgreichen
Sync ab. Schema 1 enthält `deviceId`, `revision`, `serverUrl` und bis zu vier
`wifiProfiles` mit `ssid` und `password`. `Cache-Control: no-store`; diese Antwort
wird nicht im allgemeinen Inhalts-Snapshot gespeichert. Die serverseitige Revision
ändert sich nur beim Bearbeiten der Netzwerkeinstellungen. Die Eingabe und
NVS-Schreibprüfung erfolgen, bevor neue Werte aktiv werden.

**HTTP ist hier ausdrücklich der gewünschte erste Betriebsmodus.** Es schützt
WLAN-Passwörter und Gerätekommunikation nicht vor Mitlesen oder Manipulation.
Profile deshalb im vertrauenswürdigen Heimnetz übertragen. Die bestehende
Geräte-API hat keine individuelle Geräteauthentifizierung; eine öffentliche
Freigabe schützt auch diese neue Route nicht automatisch. Passwörter werden am
Server persistent gespeichert und sind Bestandteil seiner Backups. Das
Passwortfeld der Weboberfläche wird nie mit gespeicherten Werten ausgefüllt.
Für eine spätere HTTPS-Umstellung benötigt die Firmware weiterhin eine passende
Root-CA; Zertifikatsprüfungen werden nicht abgeschaltet.

## WLAN direkt am Gerät

**Einstellungen → WLAN einrichten**. Name und Passwort auswählen, eingeben,
danach **Speichern und verbinden**. Es sind zusätzlich vier lokale Profile
möglich. Kein bestehendes Profil wird bei voller Liste automatisch verdrängt.
Ein lokales Profil durch Speichern derselben SSID aktualisieren oder über seinen
Löschen-Eintrag entfernen.

Im Texteditor:

- Rechts LINKS/RECHTS: Zeichen auswählen, einschließlich Leerzeichen, Satzzeichen
  und ä/ö/ü/Ä/Ö/Ü/ß.
- Rechts OBEN: Zeichen anfügen; rechts UNTEN: letztes Zeichen löschen.
- Links OBEN: a/A umschalten; links UNTEN: zu Ziffern/Sonderzeichen springen.
- Rechts MITTE: Eingabe fertig; links MITTE: zurück, im WLAN-Menü zum Verlassen.

Passwörter werden verdeckt angezeigt. SSID maximal 32 UTF-8-Bytes, Passwort leer
für offene Netze, 8–63 Bytes oder 64 Hex-Zeichen. Es wird nichts an den Server
zurückgeladen. Die Speicherung läuft im Netzwerktask, nicht im Renderpfad.
Bei falschen Zugangsdaten bleiben alle anderen Profile verfügbar; der Status
unterscheidet neues WLAN von einem wieder benutzten anderen Profil.

Lokale Profile haben Vorrang vor Serverprofilen mit derselben SSID. Erst danach
folgen Serverprofile und zuletzt das ursprünglich provisionierte WLAN, sofern
seine SSID nicht bereits vorkommt. Jeder Verbindungsversuch hat 15 Sekunden Zeit.
Server-Sync ersetzt ausschließlich die Serverliste. Unterstützt werden
2,4-GHz-Netze ohne Browser-Anmeldung; kein Captive Portal oder Enterprise-WLAN.

## Zahlenzerlegung

Im Homeserver pro Gerät unter **Zahlenzerlegung** die Zielzahlen speichern:
`9,10`, `3-15` oder beispielsweise `3-8,10,12`. Zielzahlen sind 1–100; doppelte
Zahlen werden entfernt, falsche Eingaben abgelehnt. Ohne Konfiguration gilt 10.

**Spiele → Mathe-Labor → Zahlenwerkstatt → Fehlenden Teil finden**.
Die Firmware wählt eine der Zielzahlen und einen Summanden einschließlich 0
und Zielzahl. Nur der fehlende Summand wird mit den rechten Richtungstasten
geändert; rechts MITTE prüft. Links LINKS zeigt die farbigen Gruppen samt Lösung,
links RECHTS erzeugt eine neue Aufgabe. Keine automatische Weiterleitung nach
Erfolg. Die bestehende freie Zerlegung bleibt als eigene Variante erhalten.
Aufgaben funktionieren nach dem Sync offline; keine Sprachausgabe.

## Abnahme

Hosttests prüfen NVS-Migration, Neustart, volle Profile, Schreibfehler,
Server-/lokale Profile, Eingabegrenzen und UTF-8-Löschen. Mathetests prüfen
zufällige Zielauswahl, beide Null-Randfälle, unveränderlichen vorgegebenen
Summanden und Zeichenbegrenzungen bis 100. Zusätzlich erforderlich: reales
OTA, Stromunterbrechung, Heimnetz/Hotspot-Wechsel, DNS, Chat, Display und Tasten.

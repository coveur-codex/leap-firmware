#include "Ui.h"
#include "Audio.h"
#include "Hardware.h"
#include "Motion.h"
#include "Network.h"
#include "Protocol.h"
#include "Radio.h"
#include <algorithm>
#include <time.h>
namespace leap {
Ui ui;
constexpr uint16_t Background = 0x10e5, Panel = 0x18e7, Accent = 0x06b8, Muted = 0x9d35;
static String displayText(String s) {
  // Built-in 6x8 font: predictable wrapping, German transliteration, no broken UTF-8.
  const char *from[] = {"ä", "ö", "ü", "Ä", "Ö", "Ü", "ß", "é", "è", "–", "—", "’", "„", "“", "°"};
  const char *to[] = {"ae", "oe", "ue", "Ae", "Oe", "Ue", "ss",   "e",
                      "e",  "-",  "-",  "'",  "\"", "\"", " Grad"};
  for (int i = 0; i < 15; i++)
    s.replace(from[i], to[i]);
  String out;
  for (unsigned char c : s) {
    if (c < 128) {
      if (c == '\n' || c >= 32)
        out += char(c);
    } else if ((c & 0xc0) != 0x80)
      out += '?';
  }
  return out;
}
static String gameTitle(const String &id) {
  if (id == "hot_potato")
    return "Heisse Kartoffel";
  if (id == "simon_motion")
    return "Simon";
  if (id == "tilt_maze")
    return "Kipp-Labyrinth";
  return "Unbekanntes Spiel";
}
bool Ui::begin() {
  prefs.begin("leap-ui", false);
  brightness = constrain(prefs.getInt("brightness", 170), 20, 255);
  audio.volume = prefs.getUChar("volume", 35);
  ledcAttach(hw::Backlight, hw::BacklightHz, 8);
  ledcWrite(hw::Backlight, 0);
  bus = new Arduino_ESP32SPI(hw::Dc, hw::Cs, hw::Sck, hw::Mosi, GFX_NOT_DEFINED);
  panel = new Arduino_NV3007(bus, hw::Reset, hw::Rotation, true, hw::NativeWidth, hw::NativeHeight,
                             hw::ColumnOffset, 0, hw::ColumnOffset, 0);
  canvas = new Arduino_Canvas(hw::Width, hw::Height, panel);
  healthy = canvas && canvas->begin(hw::SpiHz);
  if (!healthy) {
    log("DISPLAY", "Initialization failed");
    return false;
  }
  canvas->setTextWrap(false);
  reload();
  lastInput = millis();
  render();
  ledcWrite(hw::Backlight, brightness);
  log("DISPLAY", "428x142 landscape ready");
  return true;
}
void Ui::reload() {
  int oldPage = page, oldSelection = selection, oldItem = item, oldScroll = scroll,
      oldKnowledge = knowledgeMode;
  bool oldAnswered = answered, oldGame = gameOpen;
  int oldOrder[4];
  for (int i = 0; i < 4; i++)
    oldOrder[i] = answerOrder[i];
  JsonDocument previous(&jsonRam);
  previous["config"] = state["config"];
  previous["assets"] = state["assets"];
  previous["quiz"] = state["content"]["quiz"];
  storage.load(state);
  bool same =
      previous["config"].as<JsonVariantConst>() == state["config"].as<JsonVariantConst>() &&
      previous["assets"].as<JsonVariantConst>() == state["assets"].as<JsonVariantConst>() &&
      previous["quiz"].as<JsonVariantConst>() == state["content"]["quiz"].as<JsonVariantConst>();
  generation = storage.generation.load();
  radio.configure(state);
  pages.clear();
  JsonArray configuredPages = state["config"]["pages"].as<JsonArray>();
  for (JsonObject p : configuredPages) {
    String id = p["id"] | "";
    if (!(p["enabled"] | false))
      continue;
    if (id == "communication" && state["config"]["communicationEnabled"] != true)
      continue;
    if (id == "home" || id == "news" || id == "weather" || id == "aircraft" || id == "quiz" ||
        id == "games" || id == "communication" || id == "knowledge" || id == "settings")
      pages.push_back({id, p["title"] | id, p["order"] | 0});
  }
  std::stable_sort(pages.begin(), pages.end(),
                   [](const Page &a, const Page &b) { return a.order < b.order; });
  if (assetOfType("chill").length())
    pages.push_back({"chill", "Ruhezeit", 1000});
  if (pages.empty())
    pages.push_back({"home", "LEAP", 0});
  page = std::min(page, int(pages.size()) - 1);
  selection = item = scroll = 0;
  quizDetail = 0;
  answered = false;
  gameOpen = false;
  knowledgeMode = 0;
  quiz.clear();
  auto questions = quiz["questions"].to<JsonArray>();
  // Prefer assigned versioned catalogs; retain the legacy endpoint as fallback.
  for (JsonPair p : state["assets"].as<JsonObject>()) {
    JsonDocument def(&jsonRam), catalog(&jsonRam);
    if (!assets.definition(p.key().c_str(), p.value(), def) || def["type"] != "quiz")
      continue;
    if (storage.readJson(
            assets.resolve(p.key().c_str(), p.value(), def["questionsFile"] | "questions.json"),
            catalog))
      for (JsonObject q : catalog["questions"].as<JsonArray>())
        if ((q["minAge"] | 0) <= (state["config"]["age"] | 0) && questions.size() < 200)
          questions.add(q);
  }
  JsonArray legacyQuestions = state["content"]["quiz"]["questions"].as<JsonArray>();
  if (!questions.size())
    for (JsonObject q : legacyQuestions)
      if ((q["minAge"] | 0) <= (state["config"]["age"] | 0) && questions.size() < 200)
        questions.add(q);
  nextQuestion(0);
  if (same) {
    page = std::min(oldPage, int(pages.size()) - 1);
    selection = oldSelection;
    item = oldItem;
    scroll = oldScroll;
    knowledgeMode = oldKnowledge;
    answered = oldAnswered;
    for (int i = 0; i < 4; i++)
      answerOrder[i] = oldOrder[i];
    gameOpen = oldGame;
  }
}
void Ui::text(const String &s, int x, int y, int size, uint16_t color) {
  canvas->setCursor(x, y);
  canvas->setTextSize(size);
  canvas->setTextColor(color);
  canvas->print(displayText(s));
}
void Ui::body(const String &raw, int x, int y, int width, int height) {
  String s = displayText(raw);
  int columns = width / 6, line = 0, drawn = 0, rows = height / 10;
  unsigned pos = 0;
  while (pos < s.length() && drawn < rows) {
    unsigned end = std::min(pos + columns, unsigned(s.length()));
    int nl = s.indexOf('\n', pos);
    if (nl >= 0 && unsigned(nl) < end)
      end = nl;
    else if (end < s.length()) {
      int space = s.lastIndexOf(' ', end);
      if (space > int(pos))
        end = space;
    }
    if (line++ >= scroll) {
      text(s.substring(pos, end), x, y + drawn * 10);
      drawn++;
    }
    pos = end;
    if (pos < s.length() && (s[pos] == ' ' || s[pos] == '\n'))
      pos++;
  }
  // Clamp overscroll by allowing only one extra screen; empty scroll resets.
  if (!drawn && scroll > 0)
    scroll = std::max(0, scroll - 1);
  if (pos < s.length())
    text("v", x + width - 6, y + height - 8, 1, Accent);
}
void Ui::list(const std::vector<String> &labels, int x, int y, int width) {
  if (labels.empty()) {
    text("Noch keine Inhalte", x, y, 1, Muted);
    return;
  }
  selection = constrain(selection, 0, int(labels.size()) - 1);
  int start = (selection / 4) * 4;
  for (int i = start; i < int(labels.size()) && i < start + 4; i++) {
    int yy = y + (i - start) * (y > 60 ? 14 : 19);
    if (i == selection)
      canvas->fillRoundRect(x - 4, yy - 3, width, y > 60 ? 13 : 18, 4, Accent);
    String label = displayText(labels[i]);
    int max = width / 6 - 2;
    if (label.length() > unsigned(max))
      label = label.substring(0, max - 2) + "..";
    text(label, x, yy, 1, i == selection ? Background : 0xffff);
  }
}
String Ui::assetOfType(const char *type) {
  for (JsonPair p : state["assets"].as<JsonObject>()) {
    JsonDocument def(&jsonRam);
    if (assets.definition(p.key().c_str(), p.value(), def) && def["type"] == type)
      return p.key().c_str();
  }
  return "";
}
bool Ui::drawAsset(const String &id, Media &media, int x, int y, int width, int height,
                   bool animate) {
  if (!id.length())
    return false;
  int version = state["assets"][id] | 0;
  if (!version)
    return false;
  JsonDocument def(&jsonRam);
  if (!assets.definition(id, version, def))
    return false;
  if (def["type"] == "avatar") {
    JsonDocument manifest(&jsonRam);
    if (!storage.readJson(storage.package(id, version), manifest))
      return false;
    String frame = avatarFrame(manifest.as<JsonVariantConst>(), animate ? millis() : 0).c_str();
    return media.draw(*canvas, assets.resolve(id, version, frame), frame, x, y, width, height);
  }
  String path = def["preview"] | "";
  JsonArray frames = def["animations"]["idle"]["frames"].as<JsonArray>();
  if (animate && frames.size()) {
    int ms = std::max(80, def["animations"]["idle"]["frameDurationMs"] | 120);
    path = frames[(millis() / ms) % frames.size()].as<String>();
  }
  return media.draw(*canvas, assets.resolve(id, version, path), path, x, y, width, height);
}
void Ui::sidebar() {
  canvas->fillRect(0, 0, 100, 142, Panel);
  for (int i = 0; i < 3; i++)
    canvas->fillRect(8 + i * 5, 17 - i * 4, 3, 4 + i * 4, network.connected ? Accent : Muted);
  if (network.busy)
    canvas->drawCircle(88, 12, 4, Accent);
  if (radio.enabled) {
    text("G", 32, 10, 1, Accent);
  } // group radio, not Bluetooth
  time_t now = time(nullptr);
  tm local{};
  localtime_r(&now, &local);
  char clock[8];
  if (now > 1700000000)
    strftime(clock, sizeof(clock), "%H:%M", &local);
  else
    strlcpy(clock, "--:--", sizeof(clock));
  text(String(network.timeSynced ? "" : "~") + clock, 8, 25, 2);
  char date[12];
  if (now > 1700000000) {
    strftime(date, sizeof(date), "%d.%m.%y", &local);
    text(date, 8, 43, 1, Muted);
  }
  String id = state["config"]["avatar"] | "dragon";
  if (!id.startsWith("avatar-"))
    id = "avatar-" + id;
  if (!drawAsset(id, avatar, 10, 53, 80, 80, true)) {
    canvas->fillCircle(50, 90, 24, Accent);
    canvas->fillCircle(42, 85, 3, Background);
    canvas->fillCircle(58, 85, 3, Background);
    canvas->drawFastHLine(43, 100, 14, Background);
  }
  String name = state["config"]["avatarName"] | "LEAP";
  text(displayText(name).substring(0, 14), 8, 134, 1, Muted);
}
void Ui::nextQuestion(int delta) {
  int count = quiz["questions"].size();
  item = count ? (item + delta + count) % count : 0;
  selection = scroll = 0;
  answered = false;
  quizDetail = 0;
  for (int i = 0; i < 4; i++)
    answerOrder[i] = i;
  for (int i = 3; i > 0; i--)
    std::swap(answerOrder[i], answerOrder[esp_random() % (i + 1)]);
}
void Ui::drawPage(const String &id) {
  auto content = state["content"].as<JsonObject>();
  if (id == "home") {
    String child = state["config"]["childName"] | "";
    if (!child.length())
      child = state["config"]["name"] | "du";
    text("Hallo " + displayText(child).substring(0, 18) + "!", 112, 40, 2, Accent);
    int row = 0;
    JsonObject slots = state["config"]["homeSlots"].as<JsonObject>();
    for (JsonPair slot : slots) {
      String kind = slot.value().as<String>(), label;
      if (kind == "weather" && !content["weather"]["current"].isNull())
        label = "Wetter: " + String(content["weather"]["current"]["temperature"].as<float>(), 0) +
                " Grad";
      if (kind == "news_count")
        label = "News: " + String(content["news"]["articles"].size());
      if (kind == "question_of_day")
        label = "Quiz: " + String(quiz["questions"].size()) + " Fragen";
      if (label.length())
        text(label, 112, 68 + (row++) * 14);
    }
    if (state["config"]["deviceId"].isNull())
      body("Willkommen! WLAN und Geraete-ID in LocalConfig.h einrichten. Danach hier offline "
           "weiter nutzen.",
           112, 70, 290, 55);
  } else if (id == "news") {
    auto rows = content["news"]["articles"].as<JsonArray>();
    if (!rows.size()) {
      body("Noch keine Nachrichten gespeichert.");
      return;
    }
    item = (item + rows.size()) % rows.size();
    auto a = rows[item];
    String image = a["image"] | "", hash = state["images"][image] | "";
    bool shown = hash.length() && picture.draw(*canvas, storage.blob(hash), image, 320, 40, 96, 66);
    body(String(a["title"] | "") + "\n\n" + String(a["summary"] | "") + "\n" +
             String(a["source"] | "") + " | " + String(a["published"] | ""),
         112, 42, shown ? 196 : 302, 82);
  } else if (id == "weather") {
    auto w = content["weather"];
    if (w["current"].isNull()) {
      body("Wetter wartet auf den ersten Sync.");
      return;
    }
    text(String(w["current"]["temperature"].as<float>(), 0) + " Grad " + String(w["unit"] | "C"),
         112, 39, 2, Accent);
    String details =
        String(w["location"] | "") + "\nMin " + String(w["today"]["min"].as<float>(), 0) +
        " / Max " + String(w["today"]["max"].as<float>(), 0) + "\nRegen " +
        String(w["today"]["precipitationProbability"].as<int>()) + "% | Wind " +
        String(w["current"]["windSpeed"].as<float>(), 0) + "\nStand: " + String(w["updated"] | "");
    if (w["stale"] == true)
      details += " (Cache)";
    body(details, 112, 65, 300, 58);
  } else if (id == "aircraft") {
    auto rows = content["aircraft"]["aircraft"].as<JsonArray>();
    if (!rows.size()) {
      body("Keine Flugzeuge im gespeicherten Umkreis.");
      return;
    }
    item = (item + rows.size()) % rows.size();
    auto a = rows[item];
    body(String(a["callsign"] | a["registration"] | a["hex"] | "") +
         "\nTyp: " + String(a["type"] | "?") +
         "\nEntfernung: " + String(a["distanceNm"].as<float>(), 1) + " NM\nHoehe: " +
         (a["altitudeFeet"].isNull() ? String("?") : String(a["altitudeFeet"].as<int>())) +
         " ft\nStand: " + String(content["aircraft"]["updated"] | ""));
  } else if (id == "quiz") {
    auto rows = quiz["questions"].as<JsonArray>();
    if (!rows.size()) {
      body("Noch keine passenden Quizfragen.");
      return;
    }
    auto q = rows[item % rows.size()];
    if (quizDetail) {
      body(quizDetail == 1 ? String(q["q"] | "") : String(q["a"][answerOrder[selection]] | ""), 112,
           40, 302, 82);
      notice = "Oben/Unten: lesen | Mitte: zurueck";
      return;
    }
    if (answered)
      body(String(answerOrder[selection] == 0 ? "Richtig!\n" : "Gute Idee! Richtig ist:\n") +
           String(q["a"][0] | "") + "\n" + String(q["explanation"] | "") +
           "\nRechts/Links: naechste Frage");
    else {
      body(q["q"] | "", 112, 36, 300, 30);
      std::vector<String> labels;
      for (int i : answerOrder)
        labels.push_back(q["a"][i] | "");
      list(labels, 112, 69, 302);
    }
  } else if (id == "games") {
    if (gameOpen)
      game.draw(*canvas, 112, 38);
    else {
      std::vector<String> labels;
      JsonArray configuredGames = state["config"]["games"].as<JsonArray>();
      for (JsonObject g : configuredGames)
        if (g["enabled"] == true)
          labels.push_back(gameTitle(g["id"] | ""));
      list(labels);
    }
  } else if (id == "communication") {
    if (!radio.enabled) {
      body("Gruppenchat ist ausgeschaltet.");
      return;
    }
    auto messages = radio.messages();
    if (messages.size()) {
      selection = constrain(selection, 0, int(messages.size()) - 1);
      body(messages[selection]["text"] | "", 112, 85, 300, 35);
    } else
      text("Keine Vorlagen", 112, 90, 1, Muted);
    if (radio.count) {
      auto &last =
          radio.history[radio.count - 1 - std::min(size_t(std::max(0, item)), radio.count - 1)];
      text(displayText(last.name).substring(0, 40), 112, 38, 1, Accent);
      int old = scroll;
      scroll = 0;
      body(last.text, 112, 50, 300, 28);
      scroll = old;
    } else
      text("Gemeinsamer Gruppenchat", 112, 43, 1, Muted);
  } else if (id == "knowledge") {
    auto k = content["knowledge"];
    if (knowledgeMode == 0)
      list({"Gespeicherten Artikel lesen", "Suchen", "Zufaelligen Artikel laden",
            "Weiterfuehrende Artikel"});
    if (knowledgeMode == 1)
      body(String(k["title"] | "Noch kein Artikel") + "\n" + String(k["text"] | "") +
           "\n\nQuelle: " + String(k["sourceName"] | k["source"] | "") + "\n" +
           String(k["originalUrl"] | "") + "\n" + String(k["license"] | ""));
    if (knowledgeMode == 2) {
      text(query, 112, 44, 2, Accent);
      const char *alphabet = "abcdefghijklmnopqrstuvwxyz ";
      text(String("< ") + alphabet[character] + " >", 112, 75, 2);
      text("Oben: + | Unten: loeschen", 112, 106);
    }
    if (knowledgeMode == 3 || knowledgeMode == 4) {
      auto rows =
          (knowledgeMode == 3 ? content["knowledgeSearch"]["results"] : k["links"]).as<JsonArray>();
      std::vector<String> labels;
      for (JsonObject row : rows)
        labels.push_back(row["title"] | "");
      list(labels);
    }
  } else if (id == "chill") {
    float phase = (millis() % 8000) / 8000.0f;
    int radius = 12 + int(16 * (0.5f - 0.5f * cosf(phase * 6.283185f)));
    canvas->fillCircle(160, 80, radius, Accent);
    text(phase < 0.5f ? "Einatmen" : "Ausatmen", 212, 62, 2);
    text("Mitte: Klang / Stille", 212, 95);
    drawAsset(assetOfType("chill"), picture, 345, 63, 64, 56, true);
  } else if (id == "settings") {
    list({"Licht: " + String(brightness * 100 / 255) + "%",
          "Lautstaerke: " + String(audio.volume.load()) + "%", "Jetzt synchronisieren",
          "Info / Geraete-ID"});
    if (selection == 3)
      notice = String(LEAP_DEVICE_ID) + " | " + FirmwareVersion +
               (motion.available ? " | IMU OK" : " | IMU fehlt");
  }
}
void Ui::render() {
  if (!healthy)
    return;
  canvas->fillScreen(Background);
  sidebar();
  if (locked) {
    text("LEAP", 135, 40, 4, Accent);
    text("Mitte druecken zum Starten", 122, 99);
  } else if (menu) {
    text("Entdecken", 112, 13, 2, Accent);
    std::vector<String> labels;
    for (auto &p : pages)
      labels.push_back(p.title);
    list(labels);
  } else {
    text(displayText(pages[page].title).substring(0, 24), 112, 12, 2, Accent);
    drawPage(pages[page].id);
  }
  canvas->fillRect(103, 132, 325, 10, Panel);
  text(notice.length() ? displayText(notice).substring(0, 52)
                       : "L: Seiten / Menue   R: Waehlen / OK",
       108, 134, 1, Muted);
  canvas->flush();
}
void Ui::input(const InputEvent &e) {
  lastInput = millis();
  notice = "";
  if (e.longPress) {
    if (!e.right && e.key == Key::Center) {
      locked = true;
      menu = false;
      audio.stop();
    }
    return;
  }
  if (locked) {
    if (e.key == Key::Center) {
      locked = false;
      menu = true;
      selection = page;
    }
    return;
  }
  if (!e.right) {
    if (e.key == Key::Center) {
      if (gameOpen) {
        gameOpen = false;
        audio.stop();
        return;
      }
      menu = !menu;
      selection = page;
      scroll = 0;
      return;
    }
    if (e.key == Key::Left || e.key == Key::Right) {
      page = (page + pages.size() + (e.key == Key::Right ? 1 : -1)) % pages.size();
      selection = item = scroll = 0;
      answered = gameOpen = false;
      knowledgeMode = 0;
      menu = false;
      audio.stop();
      return;
    }
  }
  if (menu) {
    if (e.key == Key::Up)
      selection--;
    if (e.key == Key::Down)
      selection++;
    selection = constrain(selection, 0, int(pages.size()) - 1);
    if (e.key == Key::Center) {
      page = selection;
      selection = item = scroll = 0;
      menu = false;
    }
    return;
  }
  if (e.right)
    action(e);
}
void Ui::action(const InputEvent &e) {
  String id = pages[page].id;
  int direction = (e.key == Key::Down) - (e.key == Key::Up);
  if (id == "quiz") {
    if (quizDetail) {
      scroll = std::max(0, scroll + direction);
      if (e.key == Key::Center) {
        quizDetail = 0;
        scroll = 0;
      }
      return;
    }
    if (answered) {
      scroll = std::max(0, scroll + direction);
      if (e.key == Key::Left || e.key == Key::Right || e.key == Key::Center)
        nextQuestion(e.key == Key::Left ? -1 : 1);
    } else {
      selection = constrain(selection + direction, 0, 3);
      if (e.key == Key::Left || e.key == Key::Right) {
        quizDetail = e.key == Key::Left ? 1 : 2;
        scroll = 0;
      }
      if (e.key == Key::Center && quiz["questions"].size()) {
        answered = true;
        scroll = 0;
        audio.tone(answerOrder[selection] == 0 ? 880 : 220);
      }
    }
  } else if (id == "games") {
    if (gameOpen) {
      if (e.key == Key::Center && !game.active()) {
        gameOpen = false;
      } else
        game.input(e.key);
    } else {
      selection = std::max(0, selection + direction);
      if (e.key == Key::Center) {
        int n = 0;
        JsonArray configuredGames = state["config"]["games"].as<JsonArray>();
        for (JsonObject g : configuredGames)
          if (g["enabled"] == true && n++ == selection) {
            game.start(g["id"] | "");
            gameOpen = true;
            break;
          }
      }
    }
  } else if (id == "communication") {
    if (e.key == Key::Left || e.key == Key::Right)
      item = constrain(item + (e.key == Key::Left ? 1 : -1), 0, std::max(0, int(radio.count) - 1));
    selection = constrain(selection + direction, 0, std::max(0, int(radio.messages().size()) - 1));
    if (e.key == Key::Center) {
      notice = radio.send(selection) ? "Gesendet (ohne Empfangsbestaetigung)"
                                     : "Senden derzeit nicht moeglich";
    }
  } else if (id == "knowledge") {
    if (knowledgeMode == 0) {
      selection = constrain(selection + direction, 0, 3);
      if (e.key == Key::Center) {
        scroll = 0;
        if (selection == 0)
          knowledgeMode = 1;
        if (selection == 1)
          knowledgeMode = 2;
        if (selection == 2) {
          notice = network.connected && network.knowledge("/knowledge/random")
                       ? "Artikel wird geladen"
                       : "Offline: letzter Artikel";
          knowledgeMode = 1;
        }
        if (selection == 3)
          knowledgeMode = 4;
        selection = 0;
      }
    } else if (knowledgeMode == 1) {
      scroll = std::max(0, scroll + direction);
      if (e.key == Key::Center) {
        knowledgeMode = 0;
        selection = 0;
      }
    } else if (knowledgeMode == 2) {
      if (e.key == Key::Left)
        character = (character + 26) % 27;
      if (e.key == Key::Right)
        character = (character + 1) % 27;
      if (e.key == Key::Up && query.length() < 40)
        query += "abcdefghijklmnopqrstuvwxyz "[character];
      if (e.key == Key::Down && query.length())
        query.remove(query.length() - 1);
      if (e.key == Key::Center && query.length() >= 2) {
        notice = network.connected && network.knowledge("/knowledge/search?q=" +
                                                        Transport::encode(query) + "&limit=8")
                     ? "Suche laeuft"
                     : "Offline: letzte Suchergebnisse";
        knowledgeMode = 3;
        selection = 0;
      }
    } else {
      auto rows = (knowledgeMode == 3 ? state["content"]["knowledgeSearch"]["results"]
                                      : state["content"]["knowledge"]["links"])
                      .as<JsonArray>();
      selection = constrain(selection + direction, 0, std::max(0, int(rows.size()) - 1));
      if (e.key == Key::Center && rows.size()) {
        String ref = rows[selection]["articleRef"] | "";
        if (ref.length() && network.connected)
          network.knowledge("/knowledge/article/" + ref);
        else
          notice = "Offline: letzter Artikel";
        knowledgeMode = 1;
        scroll = 0;
      }
    }
  } else if (id == "settings") {
    selection = constrain(selection + direction, 0, 3);
    int delta = (e.key == Key::Right) - (e.key == Key::Left);
    if (selection == 0 && delta) {
      brightness = constrain(brightness + delta * 15, 20, 255);
      dirtySettings = true;
    }
    if (selection == 1 && delta) {
      audio.volume = constrain(int(audio.volume.load()) + delta * 5, 0, 100);
      dirtySettings = true;
      audio.tone(660);
    }
    if (selection == 2 && e.key == Key::Center) {
      network.requestSync();
      notice = "Sync angefragt";
    }
  } else if (id == "chill") {
    if (e.key == Key::Center) {
      static bool playing = false;
      playing = !playing;
      if (!playing)
        audio.stop();
      else {
        String package = assetOfType("sound");
        JsonDocument m(&jsonRam);
        if (storage.readJson(storage.package(package, state["assets"][package] | 0), m))
          for (JsonObject f : m["files"].as<JsonArray>()) {
            String name = f["path"] | "";
            if (name.endsWith(".wav")) {
              audio.play(storage.blob(f["sha256"].as<String>()));
              break;
            }
          }
      }
    }
  } else {
    scroll = std::max(0, scroll + direction);
    if (e.key == Key::Left || e.key == Key::Right) {
      item += e.key == Key::Right ? 1 : -1;
      scroll = 0;
    }
  }
}
void Ui::tick() {
  if (!healthy)
    return;
  if (generation != storage.generation.load())
    reload();
  radio.poll();
  game.tick();
  bool dim = elapsed(millis(), lastInput, 60000);
  ledcWrite(hw::Backlight, dim ? 20 : brightness);
  if (elapsed(millis(), lastInput, 180000)) {
    locked = true;
    menu = false;
  }
  if (dirtySettings && elapsed(millis(), lastSave, 5000)) {
    prefs.putInt("brightness", brightness);
    prefs.putUChar("volume", audio.volume);
    dirtySettings = false;
    lastSave = millis();
  }
  if (elapsed(millis(), lastFrame, 100)) {
    lastFrame = millis();
    render();
  }
}
} // namespace leap

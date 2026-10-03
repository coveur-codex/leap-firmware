#include "Ui.h"
#include "AircraftMap.h"
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
  if (id == "tamagotchi")
    return "Mein Haustier";
  if (id == "snake")
    return "Snake";
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
  game.begin();
  brightness = constrain(prefs.getInt("brightness", 170), 20, 255);
  audio.volume = prefs.getUChar("volume", 35);
  ledcAttach(hw::Backlight, hw::BacklightHz, 8);
  ledcWrite(hw::Backlight, 0);
  bus = new Arduino_ESP32SPI(hw::Dc, hw::Cs, hw::Sck, hw::Mosi, GFX_NOT_DEFINED);
  // Match the working 2.79" hardware test, including inversion and panel-specific init.
  panel = new Arduino_NV3007(bus, hw::Reset, hw::Rotation, false, hw::NativeWidth, hw::NativeHeight,
                             hw::ColumnOffset1, 0, hw::ColumnOffset2, 0,
                             nv3007_279_init_operations, sizeof(nv3007_279_init_operations));
  canvas = new Arduino_Canvas(hw::Width, hw::Height, panel);
  healthy = canvas && canvas->begin(hw::SpiHz);
  if (!healthy) {
    log("DISPLAY", "Initialization failed");
    return false;
  }
  canvas->setTextWrap(false);
  if (!Media::beginWorker()) {
    healthy = false;
    log("DISPLAY", "Media worker initialization failed");
    return false;
  }
  reload();
  lastInput = millis();
  // Resolve only the active, locally installed common asset; no network at boot.
  int systemVersion = state["assets"]["system"] | 0;
  JsonDocument systemDefinition(&jsonRam);
  if (systemVersion > 0 && assets.definition("system", systemVersion, systemDefinition) &&
      systemDefinition["type"] == "common") {
    const char *logo = "bootscreen/leap-boot.png";
    Media bootImage;
    canvas->fillScreen(0x0000);
    bootLogoVisible = bootImage.draw(*canvas, assets.resolve("system", systemVersion, logo),
                                     logo, 0, 0, hw::Width, hw::Height, true, 0x0000, false);
  }
  if (bootLogoVisible)
    canvas->flush();
  else
    render();
  ledcWrite(hw::Backlight, brightness);
  // Count two visible seconds, after decoding, transfer and backlight activation.
  bootLogoAt = millis();
  log("DISPLAY", bootLogoVisible ? "System boot logo visible for 2000 ms"
                                  : "System boot logo unavailable; starting normal UI");
  log("DISPLAY", "428x142 landscape ready");
  return true;
}
bool Ui::reload() {
  uint32_t loadedGeneration = storage.generation.load();
  JsonDocument next(&jsonRam);
  bool busy = false;
  storage.load(next, 0, &busy);
  if (busy)
    return false; // Keep current UI and retry next loop instead of waiting on flash writes.
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
  state = std::move(next);
  bool same =
      previous["config"].as<JsonVariantConst>() == state["config"].as<JsonVariantConst>() &&
      previous["assets"].as<JsonVariantConst>() == state["assets"].as<JsonVariantConst>() &&
      previous["quiz"].as<JsonVariantConst>() == state["content"]["quiz"].as<JsonVariantConst>();
  generation = loadedGeneration;
  manifests.clear();
  for (JsonPair p : state["assets"].as<JsonObject>()) {
    JsonDocument manifest(&jsonRam);
    if (storage.readJson(storage.package(p.key().c_str(), p.value()), manifest))
      manifests[p.key().c_str()] = manifest;
  }
  String avatarId = state["config"]["avatar"] | "dragon";
  if (!avatarId.startsWith("avatar-"))
    avatarId = "avatar-" + avatarId;
  game.avatarPackage(avatarId, state["assets"][avatarId] | 0, manifests[avatarId]);
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
  if (!same) quizDetail = 0;
  answered = false;
  gameOpen = false;
  knowledgeMode = 0;
  if (!same || !quizLoaded) {
    quiz.clear();
    auto catalogs = quiz["catalogs"].to<JsonArray>();
    // Keep metadata here; load at most 200 questions from the chosen catalog.
    for (JsonPair p : state["assets"].as<JsonObject>()) {
      JsonObjectConst manifest = manifests[p.key().c_str()].as<JsonObjectConst>();
      if (manifest["definition"]["type"] != "quiz")
        continue;
      auto entry = catalogs.add<JsonObject>();
      entry["package"] = String(p.key().c_str());
      entry["name"] = String(manifest["definition"]["name"] | p.key().c_str());
    }
    if (!catalogs.size()) {
      for (JsonObjectConst c : state["content"]["quiz"]["catalogs"].as<JsonArrayConst>()) {
        auto entry = catalogs.add<JsonObject>();
        entry["id"] = c["id"];
        entry["name"] = c["name"];
      }
      // Older servers did not expose catalog IDs or names.
      if (!catalogs.size() && state["content"]["quiz"]["questions"].size())
        catalogs.add<JsonObject>()["name"] = "Quiz-Katalog";
    }
    catalogs.add<JsonObject>()["name"] = "Mathe-Quiz";
    quizLoaded = true;
    startQuiz();
  }
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
  frameRequested = true;
  if (!gameOpen)
    game.close();
  return true;
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
  for (JsonPair p : manifests.as<JsonObject>()) {
    if (p.value()["definition"]["type"] == type)
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
  JsonVariantConst manifest = manifests[id];
  JsonObjectConst def = manifest["definition"];
  if (def.isNull())
    return false;
  auto resolve = [&](const String &path) {
    for (JsonObjectConst file : manifest["files"].as<JsonArrayConst>())
      if (file["path"] == path)
        return storage.blob(file["sha256"].as<String>());
    return String();
  };
  if (def["type"] == "avatar") {
    String frame = avatarFrame(manifest, animate ? millis() : 0).c_str();
    return media.draw(*canvas, resolve(frame), frame, x, y, width, height, false, Panel, true, animate);
  }
  String path = def["preview"] | "";
  JsonArrayConst frames = def["animations"]["idle"]["frames"].as<JsonArrayConst>();
  if (animate && frames.size()) {
    int ms = std::max(80, def["animations"]["idle"]["frameDurationMs"] | 120);
    path = frames[(millis() / ms) % frames.size()].as<String>();
  }
  return media.draw(*canvas, resolve(path), path, x, y, width, height);
}
// Seven-pixel page glyphs keep all ten supported pages visible in their configured order.
static const uint8_t pageIcons[][7] = {
    {8, 28, 62, 127, 34, 42, 62},     // home
    {127, 65, 93, 65, 93, 65, 127},   // news
    {8, 42, 28, 127, 28, 42, 8},     // weather
    {8, 8, 28, 127, 28, 28, 42},     // aircraft
    {28, 34, 2, 12, 8, 0, 8},        // quiz
    {0, 62, 65, 93, 73, 65, 34},     // games
    {62, 65, 85, 65, 62, 16, 32},    // communication
    {54, 73, 73, 73, 73, 73, 62},    // knowledge
    {8, 42, 28, 99, 28, 42, 8},      // settings
    {2, 6, 14, 30, 62, 60, 24}};     // chill
static const char *pageIds[] = {"home", "news", "weather", "aircraft", "quiz", "games",
                                "communication", "knowledge", "settings", "chill"};
void Ui::sidebar() {
  canvas->fillRect(0, 0, 86, 142, Panel);
  for (int i = 0; i < 3; i++)
    canvas->drawFastVLine(7 + i * 3, 9 - i * 2, 2 + i * 2,
                          network.connected ? Accent : Muted);
  // ESP-NOW group radio; no Bluetooth connection is advertised.
  canvas->drawCircle(28, 7, 2, radio.enabled ? Accent : Muted);
  canvas->drawFastVLine(28, 9, 3, radio.enabled ? Accent : Muted);
  // No wired battery ADC: outline and dash explicitly mean unknown.
  canvas->drawRect(48, 4, 12, 7, Muted);
  canvas->drawFastVLine(60, 6, 3, Muted);
  canvas->drawFastHLine(52, 7, 4, Muted);
  if (network.busy)
    canvas->drawCircle(77, 7, 2, Accent);
  time_t now = time(nullptr);
  tm local{};
  localtime_r(&now, &local);
  char clock[8];
  if (now > 1700000000)
    strftime(clock, sizeof(clock), "%H:%M", &local);
  else
    strlcpy(clock, "--:--", sizeof(clock));
  // 22px high clock, larger than the old 16px font and still within 86px.
  const uint8_t segments[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  for (int i = 0; i < 5; ++i) {
    int x = 7 + i * 16 - (i > 2 ? 8 : 0), y = 15;
    if (i == 2) {
      canvas->fillRect(x + 1, y + 6, 2, 2, 0xffff);
      canvas->fillRect(x + 1, y + 15, 2, 2, 0xffff);
      continue;
    }
    uint8_t mask = clock[i] == '-' ? 0x40 : segments[clock[i] - '0'];
    if (mask & 1) canvas->fillRect(x + 2, y, 9, 2, 0xffff);
    if (mask & 2) canvas->fillRect(x + 11, y + 2, 2, 8, 0xffff);
    if (mask & 4) canvas->fillRect(x + 11, y + 12, 2, 8, 0xffff);
    if (mask & 8) canvas->fillRect(x + 2, y + 20, 9, 2, 0xffff);
    if (mask & 16) canvas->fillRect(x, y + 12, 2, 8, 0xffff);
    if (mask & 32) canvas->fillRect(x, y + 2, 2, 8, 0xffff);
    if (mask & 64) canvas->fillRect(x + 2, y + 10, 9, 2, 0xffff);
  }
  if (!network.timeSynced)
    canvas->drawPixel(82, 35, Muted);
  String id = state["config"]["avatar"] | "dragon";
  if (!id.startsWith("avatar-"))
    id = "avatar-" + id;
  if (!drawAsset(id, avatar, 3, 38, 80, 80, true)) {
    text("Avatar", 25, 67, 1, Muted);
    text("wartet auf", 13, 80, 1, Muted);
    text("Sync", 31, 93, 1, Muted);
  }
  int active = menu ? constrain(selection, 0, int(pages.size()) - 1) : page;
  int count = std::min(10, int(pages.size()));
  int start = std::max(0, active - count + 1);
  int left = (86 - count * 8) / 2;
  for (int i = 0; i < count; ++i) {
    int index = start + i, glyph = 0, x = left + i * 8;
    for (int j = 0; j < 10; ++j)
      if (pages[index].id == pageIds[j]) glyph = j;
    bool selected = !locked && index == active;
    if (selected) canvas->fillRect(x, 119, 8, 11, Accent);
    for (int row = 0; row < 7; ++row)
      for (int col = 0; col < 7; ++col)
        if (pageIcons[glyph][row] & (1 << (6 - col)))
          canvas->drawPixel(x + col, 121 + row, selected ? Background : Muted);
  }
  String title = locked ? "Gesperrt" : menu ? "Menue" : pages[page].title;
  title = displayText(title);
  if (title.length() > 13) title = title.substring(0, 11) + "..";
  text(title, (86 - title.length() * 6) / 2, 133, 1, Muted);
}
void Ui::startQuiz() {
  quizCatalog = -1;
  selection = item = scroll = quizDetail = 0;
  answered = false;
}
void Ui::chooseQuizCatalog(int index) {
  quizCatalog = index;
  auto questions = quiz["questions"].to<JsonArray>();
  JsonObjectConst chosen = quiz["catalogs"][index];
  if (index != int(quiz["catalogs"].size()) - 1) {
    JsonDocument catalog(&jsonRam);
    String package = chosen["package"] | "";
    JsonArrayConst rows;
    if (package.length()) {
      JsonObjectConst def = manifests[package]["definition"];
      storage.readJson(assets.resolve(package, state["assets"][package],
                                     def["questionsFile"] | "questions.json"), catalog);
      rows = catalog["questions"].as<JsonArrayConst>();
    } else
      rows = state["content"]["quiz"]["questions"].as<JsonArrayConst>();
    collectQuizCatalog(questions, rows, state["config"]["age"] | 0,
                       package.length() ? JsonVariantConst() : chosen["id"].as<JsonVariantConst>(),
                       [] { return esp_random(); });
  }
  shuffleQuizQuestions(questionOrder, questions.size(), [] { return esp_random(); });
  item = 0;
  nextQuestion(0);
}
void Ui::nextQuestion(int delta) {
  if (quizCatalog == int(quiz["catalogs"].size()) - 1) {
    auto math = generateMathQuestion(state["config"]["mathQuiz"]["operation"] | "add",
                                    state["config"]["mathQuiz"]["limit"] | 20,
                                    [] { return esp_random(); });
    auto rows = quiz["questions"].to<JsonArray>();
    auto q = rows.add<JsonObject>();
    q["q"] = math.question;
    q["explanation"] = math.explanation;
    auto answers = q["a"].to<JsonArray>();
    for (int answer : math.answers) answers.add(std::to_string(answer));
    questionOrder = {0};
  }
  int count = quiz["questions"].size();
  if (count && delta > 0 && item + delta >= count)
    shuffleQuizQuestions(questionOrder, count, [] { return esp_random(); });
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
    text("Hallo " + displayText(child).substring(0, 18) + "!", 94, 12, 2, Accent);
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
        label = "Quiz: Katalog waehlen";
      if (label.length())
        text(label, 94, 44 + (row++) * 14);
    }
    if (state["config"]["deviceId"].isNull())
      body("Willkommen! WLAN und Geraete-ID in LocalConfig.h einrichten. Danach hier offline "
           "weiter nutzen.",
           94, 46, 290, 55);
  } else if (id == "news") {
    auto rows = content["news"]["articles"].as<JsonArray>();
    if (!rows.size()) {
      body("Noch keine Nachrichten gespeichert.");
      return;
    }
    item = (item + rows.size()) % rows.size();
    auto a = rows[item];
    String image = a["image"] | "", hash = state["images"][image] | "";
    bool shown = hash.length() && picture.draw(*canvas, storage.blob(hash), image, 320, 12, 96, 80);
    body(String(a["title"] | "") + "\n\n" + String(a["summary"] | "") + "\n" +
             String(a["source"] | "") + " | " + String(a["published"] | ""),
         94, 12, shown ? 216 : 326, 112);
  } else if (id == "weather") {
    auto w = content["weather"];
    if (w["current"].isNull()) {
      body("Wetter wartet auf den ersten Sync.");
      return;
    }
    text(String(w["current"]["temperature"].as<float>(), 0) + " Grad " + String(w["unit"] | "C"),
         94, 12, 2, Accent);
    String details =
        String(w["location"] | "") + "\nMin " + String(w["today"]["min"].as<float>(), 0) +
        " / Max " + String(w["today"]["max"].as<float>(), 0) + "\nRegen " +
        String(w["today"]["precipitationProbability"].as<int>()) + "% | Wind " +
        String(w["current"]["windSpeed"].as<float>(), 0) + "\nStand: " + String(w["updated"] | "");
    if (w["stale"] == true)
      details += " (Cache)";
    body(details, 94, 38, 204, 68);
    auto radar = content["weatherRadar"];
    String hash = radar["hash"] | "";
    bool shown = digestValid(hash.c_str()) &&
                 picture.draw(*canvas, "/radar/" + hash, "radar.png", 308, 8, 112, 112);
    if (!shown) {
      canvas->drawRect(308, 8, 112, 112, Muted);
      text("Regenradar", 314, 43, 1, Muted);
      text("nicht verfuegbar", 314, 58, 1, Muted);
    } else {
      String updated = radar["updated"] | "";
      bool stale = radar["stale"] == true || !network.connected;
      tm observed{};
      if (strptime(updated.c_str(), "%Y-%m-%dT%H:%M:%S", &observed)) {
        time_t stamp = utcTimestamp(observed.tm_year + 1900, observed.tm_mon + 1,
                                    observed.tm_mday, observed.tm_hour, observed.tm_min, observed.tm_sec);
        stale = stale || !network.timeSynced || time(nullptr) - stamp > 1800;
      }
      text("Radar " + updated.substring(11, 16) + " UTC" + (stale ? " alt" : ""),
           94, 115, 1, stale ? Muted : Accent);
    }
    text("RainViewer", 308, 122, 1, Muted);
  } else if (id == "aircraft") {
    auto snapshot = content["aircraft"];
    auto rows = snapshot["aircraft"].as<JsonArray>();
    bool centered = snapshot["center"]["latitude"].is<double>() &&
                    snapshot["center"]["longitude"].is<double>();
    double lat = snapshot["center"]["latitude"] | 0.0;
    double lon = snapshot["center"]["longitude"] | 0.0;
    double radius = snapshot["radiusNm"] | 25.0;
    centered = centered && std::isfinite(lat) && std::isfinite(lon) && std::abs(lat) <= 90 &&
               std::abs(lon) <= 180 && std::isfinite(radius) && radius > 0;
    constexpr int cx = 364, cy = 64, pixels = 46;
    canvas->fillRect(308, 8, 112, 112, Panel);
    if (centered) {
      for (int r : {23, 46}) canvas->drawCircle(cx, cy, r, Muted);
      canvas->drawFastVLine(cx, cy - pixels, 2 * pixels + 1, Muted);
      canvas->drawFastHLine(cx - pixels, cy, 2 * pixels + 1, Muted);
      canvas->fillCircle(cx, cy, 2, 0xffff);
      text("N", cx - 2, 8, 1, Muted);
      if (rows.size()) item = (item + rows.size()) % rows.size();
      int index = 0;
      for (JsonObject plane : rows) {
        int dx, dy;
        bool selected = index++ == item;
        if (!plane["latitude"].is<double>() || !plane["longitude"].is<double>() ||
            !aircraftOffset(lat, lon, plane["latitude"], plane["longitude"], radius, pixels, dx, dy))
          continue;
        int x = cx + dx, y = cy + dy;
        uint16_t color = selected ? 0xffe0 : Accent;
        if (plane["trackDegrees"].is<double>() && std::isfinite(plane["trackDegrees"].as<double>())) {
          double heading = plane["trackDegrees"].as<double>() * 3.141592653589793 / 180;
          int hx = std::lround(4 * std::sin(heading)), hy = -std::lround(4 * std::cos(heading));
          canvas->drawLine(x - hx, y - hy, x + hx, y + hy, color);
          canvas->drawLine(x - hy, y + hx, x + hy, y - hx, color);
          canvas->fillCircle(x + hx, y + hy, 1, color);
        } else
          canvas->fillCircle(x, y, 2, color);
        if (selected) canvas->drawCircle(x, y, 6, color);
      }
      text(String(radius, 0) + " NM | ADSB.lol", 308, 122, 1, Muted);
    } else {
      text("Standort fehlt", 314, 52, 1, Muted);
      text("Server-Sync", 320, 66, 1, Muted);
    }
    if (!rows.size()) {
      body("Keine Flugzeuge im gespeicherten Umkreis.", 94, 12, 204, 100);
      return;
    }
    item = (item + rows.size()) % rows.size();
    auto a = rows[item];
    body(String(a["callsign"] | a["registration"] | a["hex"] | "") +
         "\nTyp: " + String(a["type"] | "?") +
         "\nEntfernung: " + String(a["distanceNm"].as<float>(), 1) + " NM\nHoehe: " +
         (a["altitudeFeet"].isNull() ? String("?") : String(a["altitudeFeet"].as<int>())) +
         " ft\nStand: " + String(snapshot["updated"] | "") +
         (snapshot["stale"] == true || !network.connected ? " (Cache)" : ""), 94, 12, 204, 110);
  } else if (id == "quiz") {
    if (quizCatalog < 0) {
      text("Katalog auswaehlen", 94, 12, 1, Accent);
      std::vector<String> labels;
      for (JsonObjectConst c : quiz["catalogs"].as<JsonArrayConst>())
        labels.push_back(c["name"] | "Quiz");
      list(labels, 94, 35, 326);
      notice = "Oben/Unten: Auswahl | Mitte: starten";
      return;
    }
    auto rows = quiz["questions"].as<JsonArray>();
    if (!rows.size()) {
      body("Noch keine passenden Quizfragen.\nMitte: Katalogauswahl");
      return;
    }
    auto q = rows[questionOrder[item % questionOrder.size()]];
    if (quizDetail) {
      body(quizDetail == 1 ? String(q["q"] | "") : String(q["a"][answerOrder[selection]] | ""), 94,
           12, 326, 110);
      notice = "Oben/Unten: lesen | Mitte: zurueck";
      return;
    }
    if (answered) {
      notice = "Hoch/Runter: lesen | Rechts: weiter | OK: Kataloge";
      body(String(answerOrder[selection] == 0 ? "Richtig!\n" : "Gute Idee! Richtig ist:\n") +
           String(q["a"][0] | "") + "\n" + String(q["explanation"] | "") +
           "\nRechts: weiter | Mitte: Kataloge");
    } else {
      body(q["q"] | "", 94, 12, 326, 30);
      std::vector<String> labels;
      for (int i : answerOrder)
        labels.push_back(q["a"][i] | "");
      list(labels, 94, 49, 326);
    }
  } else if (id == "games") {
    if (gameOpen)
      game.draw(*canvas, 94, 10);
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
      body(messages[selection]["text"] | "", 94, 72, 326, 35);
    } else
      text("Keine Vorlagen", 94, 78, 1, Muted);
    if (radio.count) {
      auto &last =
          radio.history[radio.count - 1 - std::min(size_t(std::max(0, item)), radio.count - 1)];
      text(displayText(last.name).substring(0, 40), 94, 10, 1, Accent);
      int old = scroll;
      scroll = 0;
      body(last.text, 94, 26, 326, 28);
      scroll = old;
    } else
      text("Gemeinsamer Gruppenchat", 94, 15, 1, Muted);
  } else if (id == "knowledge") {
    auto k = content["knowledge"];
    if (knowledgeMode == 0)
      list({"Gespeicherten Artikel lesen", "Suchen", "Zufaelligen Artikel laden",
            "Weiterfuehrende Artikel"});
    if (knowledgeMode == 1) {
      String image = k["image"] | "", hash = state["images"][image] | "";
      bool shown = hash.length() &&
                   picture.draw(*canvas, storage.blob(hash), image, 308, 12, 112, 100, true);
      body(String(k["title"] | "Noch kein Artikel") + "\n" + String(k["text"] | "") +
           "\n\nQuelle: " + String(k["sourceName"] | k["source"] | "") + "\n" +
           String(k["originalUrl"] | "") + "\n" + String(k["license"] | ""),
           94, 12, shown ? 204 : 326, 110);
    }
    if (knowledgeMode == 2) {
      text(query, 94, 20, 2, Accent);
      const char *alphabet = "abcdefghijklmnopqrstuvwxyz ";
      text(String("< ") + alphabet[character] + " >", 94, 55, 2);
      text("Oben: + | Unten: loeschen", 94, 86);
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
    std::vector<String> labels;
    for (auto &p : pages)
      labels.push_back(p.title);
    list(labels);
  } else {
    // Current page title lives below the sidebar navigation.
    drawPage(pages[page].id);
  }
  if (!locked && !menu && gameOpen && game.isPet()) {
    canvas->flush();
    return;
  }
  canvas->fillRect(86, 132, 342, 10, Panel);
  text(notice.length() ? displayText(notice).substring(0, 52)
                       : "L: Seiten / Menue   R: Waehlen / OK",
       94, 134, 1, Muted);
  canvas->flush();
}
void Ui::input(const InputEvent &e) {
  if (bootLogoVisible)
    return; // A boot-time key press must not shorten the logo or unlock the UI.
  lastInput = millis();
  frameRequested = true;
  notice = "";
  if (e.longPress) {
    if (!e.right && e.key == Key::Center) {
      locked = true;
      game.close();
      gameOpen = false;
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
        game.close();
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
      if (pages[page].id == "quiz")
        startQuiz();
      game.close();
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
      if (pages[page].id == "quiz")
        startQuiz();
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
    if (quizCatalog < 0) {
      selection = constrain(selection + direction, 0, int(quiz["catalogs"].size()) - 1);
      if (e.key == Key::Center) chooseQuizCatalog(selection);
      return;
    }
    if (!quiz["questions"].size()) {
      if (e.key == Key::Center) startQuiz();
      return;
    }
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
      if (e.key == Key::Center) startQuiz();
      else if (e.key == Key::Left || e.key == Key::Right)
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
        if (game.isSnake())
          game.start("snake");
        else {
          gameOpen = false;
          game.close();
        }
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
        String ref = rows[selection]["articleRef"] | rows[selection]["ref"] | "";
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
  if (bootLogoVisible) {
    if (!elapsed(millis(), bootLogoAt, 2000))
      return;
    bootLogoVisible = false;
    lastInput = millis();
    // Continue into the normal render path; watchdog and network ran throughout.
  }
  if (generation != storage.generation.load())
    reload();
  radio.poll();
  game.tick();
  bool dim = elapsed(millis(), lastInput, 60000);
  ledcWrite(hw::Backlight, dim ? 20 : brightness);
  if (elapsed(millis(), lastInput, 180000)) {
    locked = true;
    game.close();
    gameOpen = false;
    menu = false;
  }
  if (dirtySettings && elapsed(millis(), lastSave, 5000)) {
    prefs.putInt("brightness", brightness);
    prefs.putUChar("volume", audio.volume);
    dirtySettings = false;
    lastSave = millis();
  }
  if ((frameRequested && elapsed(millis(), lastFrame, 25)) || elapsed(millis(), lastFrame, 100)) {
    frameRequested = false;
    render();
    lastFrame = millis(); // Avoid immediately rendering again after a slow transfer.
  }
}
} // namespace leap

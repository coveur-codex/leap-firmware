#include "Ui.h"
#include "ChillAssets.h"
#include "AircraftMap.h"
#include "WeatherIcon.h"
#include "Audio.h"
#include "Hardware.h"
#include "Motion.h"
#include "MemoryUsage.h"
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
  if (id == "crab_journey")
    return "Krabbenreise";
  if (id == "kitchen")
    return "Meine Kueche";
  if (id == "connect_four")
    return "Vier Gewinnt";
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
bool Ui::beginDisplay() {
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
  canvas->fillScreen(0x0000);
  canvas->setTextColor(0xffff);
  canvas->setTextSize(4);
  canvas->setCursor(166, 47);
  canvas->print("LEAP");
  canvas->setTextSize(1);
  canvas->setCursor(184, 87);
  canvas->print("Startet...");
  canvas->flush();
  ledcWrite(hw::Backlight, brightness);
  log("DISPLAY", "Early LEAP screen visible before storage and radio initialization");
  return true;
}
bool Ui::begin(JsonDocument *bootState) {
  if (!healthy && !beginDisplay()) return false;
  prefs.begin("leap-ui", false);
  game.begin();
  chill.begin();
  brightness = constrain(prefs.getInt("brightness", 170), 20, 255);
  audio.volume = prefs.getUChar("volume", 35);
  if (bootState) state = std::move(*bootState);
  else storage.load(state);
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
  if (bootLogoVisible) {
    canvas->flush();
    ledcWrite(hw::Backlight, brightness);
    bootLogoAt = millis();
    log("DISPLAY", "System boot logo visible for at least 2000 ms; preparing UI");
  }
  reload(true);
  if (!bootLogoVisible) render();
  ledcWrite(hw::Backlight, brightness);
  if (!bootLogoVisible)
    log("DISPLAY", "System boot logo unavailable; starting normal UI");
  log("DISPLAY", "428x142 landscape ready");
  return true;
}
bool Ui::reload(bool initial) {
  uint32_t loadedGeneration = storage.generation.load();
  JsonDocument next(&jsonRam);
  bool busy = false;
  std::shared_ptr<const JsonDocument> nextManifests;
  if (initial) {
    next = std::move(state);
    nextManifests = storage.manifestView();
  } else
    storage.load(next, 0, &busy, nullptr, &nextManifests);
  if (busy || next.overflowed())
    return false; // Keep current UI and retry next loop instead of waiting on flash writes.
  int oldPage = page, oldSelection = selection, oldItem = item, oldScroll = scroll,
      oldKnowledge = knowledgeMode;
  bool oldAnswered = answered, oldGame = gameOpen;
  int oldOrder[4];
  for (int i = 0; i < 4; i++)
    oldOrder[i] = answerOrder[i];
  bool same = !initial &&
      state["config"].as<JsonVariantConst>() == next["config"].as<JsonVariantConst>() &&
      state["assets"].as<JsonVariantConst>() == next["assets"].as<JsonVariantConst>() &&
      state["content"]["quiz"].as<JsonVariantConst>() == next["content"]["quiz"].as<JsonVariantConst>();
  state = std::move(next);
  aircraftFrame.clear();
  generation = loadedGeneration;
  manifests = std::move(nextManifests);
  String avatarId = state["config"]["avatar"] | "dragon";
  if (!avatarId.startsWith("avatar-"))
    avatarId = "avatar-" + avatarId;
  game.avatarPackage(avatarId, state["assets"][avatarId] | 0, manifestFor(avatarId));
  if (!same || generation == 0)
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
      JsonObjectConst manifest = manifestFor(p.key().c_str()).as<JsonObjectConst>();
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
  if (!manifests) return "";
  for (JsonPairConst p : manifests->as<JsonObjectConst>()) {
    if (p.value()["definition"]["type"] == type &&
        (String(type) != "chill" || validChill(p.value()["definition"], p.value()["files"])))
      return p.key().c_str();
  }
  return "";
}
bool Ui::drawSidebarAvatar(const String &id, const String &pageId) {
  if (!id.length())
    return false;
  int version = state["assets"][id] | 0;
  if (!version)
    return false;
  JsonVariantConst manifest = manifestFor(id);
  JsonObjectConst def = manifest["definition"];
  if (def.isNull() || def["type"] != "avatar")
    return false;
  auto resolve = [&](const String &path) {
    for (JsonObjectConst file : manifest["files"].as<JsonArrayConst>())
      if (file["path"] == path)
        return storage.blob(file["sha256"].as<String>());
    return String();
  };
  String path = avatarPageImage(manifest, pageId.c_str()).c_str();
  // Do not retain an image from the previous page or avatar while decoding.
  return avatar.draw(*canvas, resolve(path), path, 3, 38, 80, 80, false, Panel, true, false);
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
  String avatarPage = locked || menu || pages.empty() ? "home" : pages[page].id;
  if (!drawSidebarAvatar(id, avatarPage)) {
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
      JsonObjectConst def = manifestFor(package)["definition"];
      storage.readJson(assets.resolve(package, state["assets"][package],
                                     def["questionsFile"] | "questions.json"), catalog, CatalogJsonLimit);
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
  quizTimer.reset();
  quizTrackingFailed = false;
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
void Ui::recordQuizAnswer(uint32_t clickedAt) {
  JsonDocument attempt(&jsonRam);
  char eventId[33];
  snprintf(eventId, sizeof(eventId), "%08lx%08lx%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random());
  attempt["eventId"] = eventId;
  int index = questionOrder[item % questionOrder.size()];
  quizAnswerSnapshot(attempt, quiz["questions"][index], answerOrder, selection,
                     quizTimer.duration(clickedAt));
  JsonObjectConst catalog = quiz["catalogs"][quizCatalog];
  bool math = quizCatalog == int(quiz["catalogs"].size()) - 1;
  String package = catalog["package"] | "";
  attempt["kind"] = math ? "math" : "catalog";
  attempt["quizSetName"] = catalog["name"];
  attempt["questionIndex"] = index;
  attempt["firmwareVersion"] = FirmwareVersion;
  if (math) {
    attempt["quizSetId"] = "math";
    attempt["quizSetVersion"] = state["config"]["configVersion"];
    attempt["mathOperation"] = state["config"]["mathQuiz"]["operation"] | "add";
    attempt["mathLimit"] = state["config"]["mathQuiz"]["limit"] | 20;
  } else if (package.length()) {
    attempt["quizSetId"] = package;
    attempt["quizSetVersion"] = state["assets"][package];
  } else {
    attempt["quizSetId"] = catalog["id"].isNull() ? String("legacy")
                          : String("catalog-") + catalog["id"].as<String>();
    attempt["quizSetVersion"] = state["versions"]["quizVersion"];
  }
  if (network.timeSynced) {
    time_t now = time(nullptr);
    tm utc{};
    gmtime_r(&now, &utc);
    char timestamp[24];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    attempt["answeredAt"] = timestamp;
  }
  quizTrackingFailed = !quizTracking.enqueue(attempt);
  if (quizTrackingFailed) log("QUIZ", "Answer could not be saved; queue full or storage unavailable");
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
    auto current = w["current"];
    int code = current["weatherCode"] | -1;
    bool day = current["isDay"] | true;
    String unit = w["unit"] | "C";
    auto number = [](JsonVariantConst value) -> String {
      return value.is<double>() && std::isfinite(value.as<double>()) ? String(value.as<double>(), 0) : String("?");
    };
    text("JETZT | " + displayText(String(w["location"] | "")).substring(0, 25), 94, 8, 1, Accent);
    drawWeatherIcon(*canvas, code, day, 94, 20, 48, Background);
    String temperature = number(current["temperature"]);
    text(temperature, 150, 25, 2);
    int degreeX = 150 + temperature.length() * 12 + 4;
    canvas->drawCircle(degreeX, 29, 2, 0xffff);
    text(unit, degreeX + 8, 25, 2);
    text(weatherLabel(code, day), 150, 47, 1, Muted);
    text("Wind " + number(current["windSpeed"]) + " km/h", 94, 67, 1, Muted);
    text("Regen " + number(w["today"]["precipitationProbability"]) + "%", 210, 67, 1, Muted);
    canvas->fillRoundRect(94, 79, 204, 43, 5, Panel);
    auto tomorrow = w["tomorrow"];
    String forecastDate = tomorrow["date"] | "";
    text("MORGEN" + (forecastDate.length() == 10 ? " " + forecastDate.substring(8, 10) + "." + forecastDate.substring(5, 7) + "." : ""),
         139, 82, 1, Accent);
    if (!tomorrow.isNull()) {
      int tomorrowCode = tomorrow["weatherCode"] | -1;
      drawWeatherIcon(*canvas, tomorrowCode, true, 98, 84, 35, Panel);
      text(weatherLabel(tomorrowCode), 139, 92, 1);
      text(number(tomorrow["min"]) + " bis " + number(tomorrow["max"]) + " " + unit, 139, 102, 1);
      text("Regen " + number(tomorrow["precipitationProbability"]) + "%", 139, 112, 1, Muted);
    } else {
      drawWeatherIcon(*canvas, -1, true, 98, 84, 35, Panel);
      text("Vorhersage fehlt", 139, 101, 1, Muted);
    }
    String weatherUpdated = w["updated"] | "";
    text((w["stale"] == true || !network.connected ? "Wetter alt | " : "Stand ") + weatherUpdated.substring(11, 16) + " UTC",
         94, 124, 1, Muted);
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
    if (aircraftFrame.isNull() || elapsed(millis(), lastAircraftFrame, 2000)) {
      if (!network.copyAircraft(aircraftFrame)) aircraftFrame.set(content["aircraft"]);
      lastAircraftFrame = millis();
      double age = aircraftFrame["ageSeconds"] | 0.0;
      if (aircraftFrame["receivedMillis"].is<uint32_t>())
        age += uint32_t(millis() - aircraftFrame["receivedMillis"].as<uint32_t>()) / 1000.0;
      else age = AircraftMaxPredictionSeconds + 1; // An offline snapshot has no monotonic time anchor.
      for (JsonObject plane : aircraftFrame["aircraft"].as<JsonArray>()) {
        double lat = plane["latitude"] | 999.0, lon = plane["longitude"] | 999.0;
        double observedAge = age + (plane["positionAgeSeconds"] | 0.0);
        bool predicted = aircraftFrame["receivedMillis"].is<uint32_t>() && predictAircraft(lat, lon, plane["groundSpeedKnots"] | -1.0,
                                          plane["trackDegrees"] | NAN, std::min(observedAge, AircraftMaxPredictionSeconds));
        plane["predicted"] = predicted;
        plane["oldPosition"] = observedAge > AircraftMaxPredictionSeconds;
        plane["latitude"] = lat;
        plane["longitude"] = lon;
        if (aircraftFrame["center"]["latitude"].is<double>() &&
            aircraftFrame["center"]["longitude"].is<double>())
          plane["distanceKm"] = aircraftDistanceKm(aircraftFrame["center"]["latitude"],
              aircraftFrame["center"]["longitude"], lat, lon);
      }
    }
    auto snapshot = aircraftFrame.as<JsonObject>();
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
      text(String(radius * 1.852, 0) + " km | ADSB.lol", 308, 122, 1, Muted);
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
    String route;
    if (!a["originName"].isNull()) route += "\nStart: " + String(a["originName"].as<const char *>());
    if (!a["destinationName"].isNull()) route += "\nZiel: " + String(a["destinationName"].as<const char *>());
    String altitude = a["altitudeFeet"].isNull() ? String("?") : String(a["altitudeFeet"].as<double>() * 0.3048, 0);
    String speed = a["groundSpeedKnots"].isNull() ? String("?") : String(a["groundSpeedKnots"].as<double>() * 1.852, 0);
    body(String(a["callsign"] | a["registration"] | a["hex"] | "Flugzeug") +
         "\n" + String(a["typeName"] | "Unbekannter Flugzeugtyp") +
         "\nEntfernung: " + String(a["distanceKm"].isNull() ? a["distanceNm"].as<double>() * 1.852 : a["distanceKm"].as<double>(), 1) +
         " km\nHoehe: " + altitude + " m\nTempo: " + speed + " km/h" + route,
         94, 8, 204, 110);
    text(a["oldPosition"] == true ? "Alte Position" : a["predicted"] == true ? "Position geschaetzt" : "Gemeldete Position",
         94, 124, 1, Muted);
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
      if (quizTrackingFailed) notice = "Tracking: Antwort konnte nicht gespeichert werden";
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
        if (g["enabled"] == true && g["id"] != "kitchen" && g["id"] != "crab_journey")
          labels.push_back(gameTitle(g["id"] | ""));
      labels.push_back(gameTitle("kitchen"));
      labels.push_back(gameTitle("crab_journey"));
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
  } else if (id == "settings") {
    if (selection == 4) {
      const auto memory = memorySnapshot();
      text("Speicher (belegt / gesamt)", 122, 12, 1, Accent);
      text("Flash (Firmware): " + memoryLabel(memory.flash), 122, 38);
      text("LittleFS: " + memoryLabel(memory.littlefs), 122, 62);
      text("PSRAM: " + memoryLabel(memory.psram), 122, 86);
      notice = "Hoch: zurueck zu Einstellungen";
    } else {
      list({"Licht: " + String(brightness * 100 / 255) + "%",
            "Lautstaerke: " + String(audio.volume.load()) + "%", "Jetzt synchronisieren",
            "Info / Geraete-ID", "Speicher"});
      text("Runter: Speicher", 94, 104, 1, Muted);
    }
    if (selection == 3)
      notice = String(LEAP_DEVICE_ID) + " | " + FirmwareVersion +
               (motion.available ? " | IMU OK" : " | IMU fehlt");
  }
}
void Ui::render() {
  if (!healthy)
    return;
  if (!locked && !menu && !pages.empty() && pages[page].id == "chill") {
    String id = assetOfType("chill");
    chill.start(id, state["assets"][id] | 0, manifestFor(id), millis());
    chill.draw(*canvas, millis());
    canvas->flush();
    return;
  }
  chill.close();
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
  if (!locked && !menu && gameOpen &&
      (game.isPet() || game.isConnectFour() || game.isKitchen() || game.isCrabJourney())) {
    canvas->flush();
    return;
  }
  canvas->fillRect(86, 132, 342, 10, Panel);
  text(notice.length() ? displayText(notice).substring(0, 52)
                       : "L: Seiten / Menue   R: Waehlen / OK",
       94, 134, 1, Muted);
  canvas->flush();
  if (!locked && !menu && !pages.empty() && pages[page].id == "quiz" &&
      quizCatalog >= 0 && quiz["questions"].size() && !answered)
    quizTimer.shown(millis());
}
void Ui::input(const InputEvent &e) {
  if (bootLogoVisible)
    return; // A boot-time key press must not shorten the logo or unlock the UI.
  lastInput = millis();
  frameRequested = true;
  notice = "";
  if (!locked && !menu && gameOpen && game.isKitchen()) {
    // Kitchen consumes both the regular hold and the two-second hold, so the
    // global lock handler cannot swallow its exit or act on the first 900 ms.
    if (game.kitchenInput(e)) {
      game.close();
      gameOpen = false;
      selection = scroll = 0;
    }
    return;
  }
  // Only the kitchen uses the additional hold; other pages keep one long action.
  if (e.longPress && e.heldMs >= Input::ExtendedCenterHoldMs)
    return;
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
  if (!menu && !pages.empty() && pages[page].id == "chill") {
    if (!chill.active()) {
      String id = assetOfType("chill");
      chill.start(id, state["assets"][id] | 0, manifestFor(id), millis());
    }
    if (chill.input(e, millis())) {
      menu = true;
      selection = page;
      scroll = 0;
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
      uint32_t clickedAt = e.timestamped ? e.atMs : millis();
      if (e.key == Key::Center && quiz["questions"].size() && quizTimer.accepts(clickedAt)) {
        recordQuizAnswer(clickedAt);
        answered = true;
        scroll = 0;
        audio.tone(answerOrder[selection] == 0 ? 880 : 220);
      }
    }
  } else if (id == "games") {
    if (gameOpen) {
      if (e.key == Key::Center && !game.active() && !game.isConnectFour()) {
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
          if (g["enabled"] == true && g["id"] != "kitchen" && g["id"] != "crab_journey" &&
              n++ == selection) {
            game.start(g["id"] | "");
            gameOpen = true;
            break;
          }
        if (!gameOpen && (selection == n || selection == n + 1)) {
          game.start(selection == n ? "kitchen" : "crab_journey");
          gameOpen = true;
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
    selection = constrain(selection + direction, 0, 4);
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
  bool radarVisible = !locked && !menu && !pages.empty() && pages[page].id == "aircraft";
  network.aircraftVisible = radarVisible;
  radio.poll();
  game.tick();
  chill.tick(millis());
  bool chillVisible = !locked && !menu && !pages.empty() && pages[page].id == "chill";
  bool dim = !chillVisible && elapsed(millis(), lastInput, 60000);
  ledcWrite(hw::Backlight, dim ? 20 : brightness);
  if (!chillVisible && elapsed(millis(), lastInput, 180000)) {
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
  uint32_t frameInterval = gameOpen && game.isCrabJourney() ? CrabJourney::FrameMs : 100;
  if ((frameRequested && elapsed(millis(), lastFrame, 25)) ||
      elapsed(millis(), lastFrame, frameInterval)) {
    frameRequested = false;
    uint32_t frameAt = millis();
    render();
    // Animated gameplay measures cadence from frame start, including display transfer time.
    lastFrame = gameOpen && game.isCrabJourney() ? frameAt : millis();
  }
}
} // namespace leap

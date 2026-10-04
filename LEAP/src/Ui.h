#pragma once
#include "Games.h"
#include "Input.h"
#include "Media.h"
#include "QuizQuestions.h"
#include "MathQuiz.h"
#include "Storage.h"
#include <vector>
namespace leap {
struct Page {
  String id, title;
  int order;
};
class Ui {
  Arduino_DataBus *bus = nullptr;
  Arduino_NV3007 *panel = nullptr;
  Arduino_Canvas *canvas = nullptr;
  JsonDocument state{&jsonRam}, quiz{&jsonRam};
  std::shared_ptr<const JsonDocument> manifests;
  JsonVariantConst manifestFor(const String &id) const {
    return manifests ? (*manifests)[id].as<JsonVariantConst>() : JsonVariantConst();
  }
  std::vector<Page> pages;
  std::vector<uint16_t> questionOrder;
  bool quizLoaded = false;
  Media avatar, picture;
  Games game;
  Preferences prefs;
  int page = 0, selection = 0, item = 0, scroll = 0, answerOrder[4] = {0, 1, 2, 3},
      knowledgeMode = 0, character = 0;
  int quizDetail = 0, quizCatalog = -1;
  void chooseQuizCatalog(int index);
  uint32_t generation = UINT32_MAX, lastFrame = 0, lastInput = 0, lastSave = 0;
  uint32_t bootLogoAt = 0, lastAircraftFrame = 0;
  JsonDocument aircraftFrame{&jsonRam};
  bool bootLogoVisible = false;
  bool frameRequested = true;
  bool locked = true, menu = false, answered = false, gameOpen = false, dirtySettings = false;
  int brightness = 170;
  String query, notice;
  bool reload(bool initial = false);
  void render();
  void sidebar();
  void body(const String &text, int x = 94, int y = 12, int width = 326, int height = 110);
  void text(const String &text, int x, int y, int size = 1, uint16_t color = 0xffff);
  void list(const std::vector<String> &labels, int x = 94, int y = 18, int width = 326);
  void nextQuestion(int delta);
  void startQuiz();
  void drawPage(const String &id);
  void action(const InputEvent &event);
  String assetOfType(const char *type);
  bool drawAsset(const String &id, Media &media, int x, int y, int width, int height,
                 bool animate = false);

public:
  bool beginDisplay();
  bool begin(JsonDocument *bootState = nullptr);
  void input(const InputEvent &event);
  void tick();
  bool healthy = false;
};
extern Ui ui;
} // namespace leap

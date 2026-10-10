#pragma once
#include "Games.h"
#include "Chill.h"
#include "Input.h"
#include "Media.h"
#include "QuizQuestions.h"
#include "MathQuiz.h"
#include "QuizTracking.h"
#include "Storage.h"
#include "ChatSelection.h"
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
  Chill chill;
  Preferences prefs;
  int page = 0, selection = 0, item = 0, scroll = 0, answerOrder[4] = {0, 1, 2, 3},
      knowledgeMode = 0, character = 0;
  int quizDetail = 0, quizCatalog = -1;
  QuizTimer quizTimer;
  MathAnswerInput mathAnswer;
  bool quizTrackingFailed = false;
  void recordQuizAnswer(uint32_t clickedAt);
  void chooseQuizCatalog(int index);
  uint32_t generation = UINT32_MAX, lastFrame = 0, lastInput = 0, lastSave = 0;
  uint32_t bootLogoAt = 0, lastAircraftFrame = 0;
  JsonDocument aircraftFrame{&jsonRam};
  bool bootLogoVisible = false;
  bool frameRequested = true;
  bool locked = true, menu = false, answered = false, gameOpen = false, dirtySettings = false;
  bool inputDiagnostics = false, calibrationVisible = false;
  void drawCalibration();
  ChatSelection chatChoice;
  void chatSymbol(const String &symbol, int x, int y, int scale = 1);
  uint16_t heldButtons = 0;
  bool diagnosticsVisible() const {
    return inputDiagnostics && !locked && !menu && !pages.empty() && pages[page].id == "settings";
  }
  void drawInputDiagnostics();
  int brightness = 170;
  String query, notice, diagnosticLastKey;
  bool reload(bool initial = false);
  void render();
  void sidebar();
  void envelope();
  void body(const String &text, int x = 94, int y = 12, int width = 326, int height = 110, const String &heading = "");
  void text(const String &text, int x, int y, int size = 1, uint16_t color = 0xffff);
  void list(const std::vector<String> &labels, int x = 94, int y = 18, int width = 326);
  void nextQuestion(int delta);
  void startQuiz();
  void drawPage(const String &id);
  void action(const InputEvent &event);
  String assetOfType(const char *type);
  bool drawSidebarAvatar(const String &id, const String &pageId);

public:
  bool beginDisplay();
  bool begin(JsonDocument *bootState = nullptr);
  void input(const InputEvent &event);
  void tick();
  void heldInputs(uint16_t mask) {
    if (heldButtons != mask && diagnosticsVisible())
      frameRequested = true;
    heldButtons = mask;
    game.heldDirections(uint8_t((mask >> 5) & 0x0f));
  }
  bool healthy = false;
};
extern Ui ui;
} // namespace leap

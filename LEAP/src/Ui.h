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
#include "Notifications.h"
#include "TimeModule.h"
#include "Finance.h"
#include "RoomControls.h"
#include "WifiEditor.h"
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
  Media avatar, picture, senderAvatar;
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
  bool wifiVisible = false;
  WifiEditor wifiEditor;
  bool wifiActive() const { return wifiVisible && !locked && !menu && !pages.empty() && pages[page].id == "settings"; }
  void drawWifi();
  void wifiAction(const InputEvent &e);
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
  TimeModule timeModule;
  bool stopwatchView = false, timeEditing = false, editSeconds = false;
  int timePreset = 0, customSeconds = 5 * 60, lapScroll = 0;
  FinanceNavigation financeNavigation;
  bool financeWasVisible = false;
  JsonDocument roomFrame{&jsonRam};
  uint32_t lastRoomRevision=0;
  RoomControls roomControls;
  void drawRoom();
  void roomAction(const InputEvent &event);
  void drawFinance();
  void drawTime();
  void timeAction(const InputEvent &event);
  Notifications notifications;
  NewsNotifications newsNotifications;
  AircraftProximity aircraftProximity;
  JsonDocument notificationAircraft{&jsonRam};
  uint32_t lastProximity = 0, lastNewsRevision = 0;
  bool aircraftBaseline = false;
  void observeNews(bool initial);
  void observeAircraft();
  void drawNotification();
  void openNotification(const Notification &message);
  std::string selectedNewsId, selectedAircraftHex;
  // headingFont styles the title, or the entire text if there is no separate title.
  void body(const String &text, int x = 94, int y = 12, int width = 326, int height = 110, const String &heading = "", uint16_t color = 0xffff, bool headingFont = false);
  void text(const String &text, int x, int y, int size = 1, uint16_t color = 0xffff);
  void glyphText(const String &glyphs, int x, int y, int size = 1, uint16_t color = 0xffff);
  void list(const std::vector<String> &labels, int x = 94, int y = 18, int width = 326);
  void nextQuestion(int delta);
  void startQuiz();
  void drawPage(const String &id);
  void action(const InputEvent &event);
  String assetOfType(const char *type);
  bool chillVisible() const { return !locked && !menu && !pages.empty() && pages[page].id == "chill"; }
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

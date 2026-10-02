#pragma once
#include "Transport.h"
#include <esp_ota_ops.h>
namespace leap {
class Ota {
  Preferences prefs;

public:
  bool begin();
  bool confirm(bool healthy);
  bool offered(JsonObjectConst release, const String &sync, Transport &net);
  String pendingSync(), pendingVersion();
  void clearReport();
  bool rolledBack = false;
  std::atomic<bool> locallyConfirmed{false};
};
extern Ota ota;
} // namespace leap

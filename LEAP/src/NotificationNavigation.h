#pragma once
#include <ArduinoJson.h>
#include <string>
namespace leap {
inline int notificationTargetIndex(JsonArrayConst rows, const char *field, const std::string &target) {
  if (target.empty()) return -1;
  int index = 0;
  for (JsonObjectConst row : rows) {
    if (target == (row[field] | "")) return index;
    ++index;
  }
  return -1;
}
} // namespace leap

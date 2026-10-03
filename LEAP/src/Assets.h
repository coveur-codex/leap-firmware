#pragma once
#include "Transport.h"
namespace leap {
class Assets {
public:
  String error;
  bool install(JsonObjectConst update, Transport &net);
  bool verify(JsonDocument &manifest, bool hashes = false);
  String resolve(const String &id, int version, const String &path);
  bool definition(const String &id, int version, JsonDocument &out);
  void cleanup(JsonArrayConst removals, JsonObjectConst active);

private:
  bool fail(JsonDocument &manifest, const String &reason, const String &path = "");
  bool validDefinition(JsonDocument &manifest);
};
extern Assets assets;
} // namespace leap

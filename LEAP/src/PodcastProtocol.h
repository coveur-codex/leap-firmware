#pragma once
#include "Core.h"
#include "RadioStream.h"
#include <ArduinoJson.h>
namespace leap {
inline bool podcastEpisode(JsonVariantConst e, int podcastId=0) {
  const char *status=e["status"]|"";
  return e.is<JsonObjectConst>() && digestValid(e["id"]|"") &&
      e["podcastId"].is<int>() && e["podcastId"].as<int>()>0 && (!podcastId || e["podcastId"]==podcastId) &&
      e["title"].is<const char *>() && strlen(e["title"])<=2000 &&
      e["url"].is<const char *>() && radioUrl(e["url"].as<std::string>()) &&
      e["position"].is<uint32_t>() && e["position"].as<uint32_t>()<=7*86400 &&
      (e["duration"].isNull() || (e["duration"].is<uint32_t>() && e["duration"].as<uint32_t>()<=7*86400)) &&
      (!strcmp(status,"unheard") || !strcmp(status,"partial") || !strcmp(status,"completed")) &&
      e["revision"].is<uint64_t>();
}
inline bool podcastPage(JsonVariantConst p,int id) {
  if (!p["episodes"].is<JsonArrayConst>() || p["episodes"].size()>20 || p["podcastId"]!=id ||
      !p["offset"].is<int>() || p["offset"].as<int>()<0 || !p["total"].is<int>() || p["total"].as<int>()<0) return false;
  for (JsonVariantConst e:p["episodes"].as<JsonArrayConst>()) if (!podcastEpisode(e,id)) return false;
  return true;
}
inline bool podcastSingle(JsonVariantConst p,int id=0) {
  return p.is<JsonObjectConst>() && !p["episode"].isUnbound() &&
      (p["episode"].isNull() || (podcastEpisode(p["episode"],id) && p["offset"].is<int>() && p["offset"].as<int>()>=0));
}
} // namespace leap

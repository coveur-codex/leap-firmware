#include "SnakeMultiplayer.h"
#include <cstring>
namespace leap {
static bool text(JsonVariantConst v, char *out, size_t n) {
  if (!v.is<const char *>() || strlen(v.as<const char *>()) >= n) return false;
  strcpy(out, v.as<const char *>());
  return true;
}
static bool cell(JsonVariantConst v, SnakeCell &out) {
  if (!v.is<JsonArrayConst>() || v.size() != 2 || !v[0].is<unsigned>() ||
      !v[1].is<unsigned>() || v[0].as<unsigned>() >= 40 || v[1].as<unsigned>() >= 13) return false;
  out = {v[0].as<uint8_t>(), v[1].as<uint8_t>()};
  return true;
}
bool decodeSnake(JsonVariantConst f, SnakeSnapshot &out) {
  if (f["type"] != "state" || f["schemaVersion"] != 1 || f["columns"] != 40 ||
      f["rows"] != 13 || f["stepMs"] != 450 || !text(f["session"], out.session, sizeof(out.session)) ||
      strlen(out.session) != 32 || !f["tick"].is<uint32_t>() || !f["revision"].is<uint32_t>() ||
      !f["remainingMs"].is<uint32_t>() || f["remainingMs"].as<uint32_t>() > 120000 ||
      !f["score"].is<uint16_t>() || !f["target"].is<uint16_t>() ||
      !f["players"].is<JsonArrayConst>() || f["players"].size() > 4 ||
      !f["food"].is<JsonArrayConst>() || f["food"].size() > 10) return false;
  const char *phases[] = {"connecting", "lobby", "countdown", "playing", "alone", "finished", "error"};
  int phase = -1;
  for (int i=1; i<6; ++i) if (f["phase"] == phases[i]) phase=i;
  if (phase < 0) return false;
  out.phase = static_cast<SnakePhase>(phase);
  out.revision=f["revision"]; out.tick=f["tick"]; out.remainingMs=f["remainingMs"];
  out.score=f["score"]; out.target=f["target"];
  out.count=0; out.foodCount=0;
  unsigned used=0;
  bool occupied[520]{};
  for (JsonVariantConst v : f["players"].as<JsonArrayConst>()) {
    auto &p=out.players[out.count++];
    if (!text(v["id"],p.id,sizeof(p.id)) || !p.id[0] ||
        !text(v["avatar"],p.avatar,sizeof(p.avatar)) || !text(v["accentColor"],p.accent,sizeof(p.accent)) ||
        !v["connected"].is<bool>() || !v["protected"].is<bool>() || !v["seq"].is<uint32_t>() ||
        !v["respawnMs"].is<uint32_t>() || !v["direction"].is<unsigned>() || v["direction"].as<unsigned>()>3 ||
        !v["body"].is<JsonArrayConst>() || v["body"].size()+used>520) return false;
    for (unsigned i=0; i+1<out.count; ++i) if (!strcmp(p.id,out.players[i].id)) return false;
    p.offset=used; p.length=v["body"].size(); p.seq=v["seq"]; p.direction=v["direction"];
    p.connected=v["connected"]; p.protectedSpawn=v["protected"]; p.respawnMs=v["respawnMs"];
    for (JsonVariantConst c : v["body"].as<JsonArrayConst>()) {
      if (!cell(c,out.cells[used])) return false;
      unsigned index=out.cells[used].y*40+out.cells[used].x;
      if (occupied[index]) return false;
      occupied[index]=true; ++used;
    }
  }
  for (JsonVariantConst c : f["food"].as<JsonArrayConst>()) {
    if (!cell(c,out.food[out.foodCount])) return false;
    auto &fc=out.food[out.foodCount++];
    unsigned index=fc.y*40+fc.x;
    if (occupied[index]) return false;
    occupied[index]=true;
  }
  return true;
}
} // namespace leap

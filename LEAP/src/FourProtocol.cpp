#include "FourMultiplayer.h"
#include <cstring>
namespace leap {
static bool text(JsonVariantConst v, char *out, size_t n) {
  if (!v.is<const char *>() || strlen(v.as<const char *>()) >= n) return false;
  strcpy(out, v.as<const char *>());
  return true;
}
bool decodeFour(JsonVariantConst f, FourSnapshot &out) {
  if (f["type"] != "state" || f["schemaVersion"] != 1 ||
      !text(f["session"],out.session,sizeof(out.session)) || strlen(out.session)!=32 ||
      !text(f["reason"],out.reason,sizeof(out.reason)) ||
      !f["revision"].is<uint32_t>() || !f["remainingMs"].is<uint32_t>() || f["remainingMs"].as<uint32_t>()>30000 ||
      !f["round"].is<uint32_t>() || !f["move"].is<uint32_t>() || f["move"].as<uint32_t>()>42 ||
      !f["turn"].is<unsigned>() || f["turn"].as<unsigned>()>2 ||
      !f["winner"].is<unsigned>() || f["winner"].as<unsigned>()>2 ||
      !f["players"].is<JsonArrayConst>() || f["players"].size()<1 || f["players"].size()>2 ||
      !f["board"].is<JsonArrayConst>() || f["board"].size()!=6) return false;
  const char *phases[]={"connecting","lobby","playing","paused","finished","error"};
  int phase=-1;
  for (int i=1;i<5;++i) if (f["phase"]==phases[i]) phase=i;
  if (phase<0) return false;
  out.phase=static_cast<FourPhase>(phase);
  out.revision=f["revision"]; out.remainingMs=f["remainingMs"]; out.round=f["round"]; out.move=f["move"];
  out.turn=f["turn"]; out.winner=f["winner"]; out.count=0;
  for (JsonVariantConst v:f["players"].as<JsonArrayConst>()) {
    auto &p=out.players[out.count++];
    if (!text(v["id"],p.id,sizeof(p.id)) || !p.id[0] ||
        !text(v["name"],p.name,sizeof(p.name)) || !text(v["accentColor"],p.accent,sizeof(p.accent)) ||
        !v["seq"].is<uint32_t>() || !v["connected"].is<bool>() ||
        !v["rematch"].is<bool>() || !v["left"].is<bool>()) return false;
    p.seq=v["seq"]; p.connected=v["connected"]; p.rematch=v["rematch"]; p.left=v["left"];
  }
  if (out.count==2 && !strcmp(out.players[0].id,out.players[1].id)) return false;
  if ((out.phase==FourPhase::Playing || out.phase==FourPhase::Paused) && (out.count!=2 || out.turn<1)) return false;
  for (int r=0;r<6;++r) {
    if (!f["board"][r].is<JsonArrayConst>() || f["board"][r].size()!=7) return false;
    for (int c=0;c<7;++c) {
      auto v=f["board"][r][c];
      if (!v.is<unsigned>() || v.as<unsigned>()>out.count) return false;
      out.board[r][c]=v.as<uint8_t>();
    }
  }
  return true;
}
} // namespace leap

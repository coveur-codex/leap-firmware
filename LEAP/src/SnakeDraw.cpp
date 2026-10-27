#include "Games.h"
#include "AccentColor.h"
#include "DisplayText.h"
#include <cstring>
namespace leap {
void Games::drawCooperative(Arduino_GFX &gfx,int left,int top) {
  auto &s=cooperative;
  gfx.setTextSize(1); gfx.setTextColor(0xffff); gfx.setCursor(left,top);
  if (s.phase==SnakePhase::Connecting) {
    printDisplayText(gfx,"Verbinde mit Homeserver..."); return;
  }
  if (s.phase==SnakePhase::Alone || s.phase==SnakePhase::Finished || s.phase==SnakePhase::Error) {
    printDisplayText(gfx,s.phase==SnakePhase::Alone ? "Keine Mitspieler gefunden" :
        s.phase==SnakePhase::Finished ? "Zusammen geschafft!" : s.error);
    if (s.phase==SnakePhase::Finished) {
      gfx.setCursor(left,top+30); gfx.printf("Team: %u Punkte  Ziel: %u",s.score,s.target);
      gfx.setCursor(left,top+50);
      printDisplayText(gfx,s.score>=s.target ? "Punkteziel erreicht!" : "Toll gespielt! Nochmal zusammen?");
    }
    gfx.setCursor(left,top+100); printDisplayText(gfx,"R Mitte: Neu  L Mitte: Zurück");
    return;
  }
  if (s.phase==SnakePhase::Lobby || s.phase==SnakePhase::Countdown) {
    gfx.printf("Team %u/4   %s %lus",s.count,s.phase==SnakePhase::Lobby ? "Suche:" : "Start:",
               (unsigned long)((s.remainingMs+999)/1000));
    // Same 320px game content area as solo Snake, separate from the sidebar.
    int slots=std::max(2,int(s.count)), spacing=320/slots;
    for (int i=0;i<slots;++i) {
      int x=left+i*spacing+(spacing-80)/2;
      if (i>=s.count) { gfx.drawRect(x+22,top+46,36,36,0x7bef); continue; }
      auto &p=s.players[i];
      const char *image=gameAvatars["definition"]["avatarSources"][p.avatar]["image"] | "";
      String blob;
      for (JsonObjectConst f : gameAvatars["files"].as<JsonArrayConst>())
        if (f["path"]==image) blob=storage.blob(f["sha256"].as<String>());
      if (!blob.length() || !lobbyImages[i].draw(gfx,blob,image,x,top+24,80,80,true,0x10e5,true,false,true)) {
        gfx.drawRect(x+22,top+46,36,36,0x7bef);
        gfx.setCursor(x+35,top+60); gfx.printf("%d",i+1);
      }
      gfx.fillRect(x,top+106,80,3,accentColor(p.accent));
    }
    gfx.setCursor(left,top+120);
    printDisplayText(gfx,snakeMultiplayer.online ? "Gleich geht es gemeinsam los!" : "WLAN: Verbinde erneut...");
    return;
  }
  gfx.printf("Team %u/%u  %lus%s",s.score,s.target,(unsigned long)((s.remainingMs+999)/1000),
             snakeMultiplayer.online ? "" : "  Verbinde...");
  int y=top+17;
  gfx.fillRect(left-1,y-1,322,106,0x18c3); gfx.drawRect(left-1,y-1,322,106,0x9d35);
  for (unsigned i=0;i<s.foodCount;++i)
    gfx.fillCircle(left+s.food[i].x*8+3,y+s.food[i].y*8+3,3,0xf800);
  for (unsigned i=0;i<s.count;++i) {
    auto &p=s.players[i];
    for (unsigned j=0;j<p.length;++j) {
      auto c=s.cells[p.offset+j];
      gfx.fillRect(left+c.x*8,y+c.y*8,7,7,accentColor(p.accent));
      if (!j) {
        // One to four dots distinguish even identical accent colours.
        for (unsigned dot=0;dot<=i;++dot)
          gfx.fillRect(left+c.x*8+1+(dot%2)*3,y+c.y*8+1+(dot/2)*3,2,2,0xffff);
        if (p.protectedSpawn) gfx.drawRect(left+c.x*8,y+c.y*8,7,7,0xffff);
      }
    }
  }
  gfx.setCursor(left,top+122);
  for (unsigned i=0;i<s.count;++i) {
    auto &p=s.players[i];
    if (!p.length) { gfx.printf("P%u: %lus  ",i+1,(unsigned long)((p.respawnMs+999)/1000)); }
  }
}
} // namespace leap

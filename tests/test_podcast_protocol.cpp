#include "PodcastProtocol.h"
#include "Protocol.h"
#include <cassert>
#include <cstdio>
#include <fstream>
using namespace leap;
int main(int argc,char **argv) {
  if (argc>1) {
    std::ifstream file(argv[1]);JsonDocument fixture;
    assert(!deserializeJson(fixture,file));
    int id=fixture["config"]["podcasts"][0]["id"]|0;
    assert(id>0 && podcastPage(fixture["podcastPage"],id) && podcastSingle(fixture["podcastEpisode"],id));
  }
  JsonDocument p;
  p["id"]=std::string(64,'a');p["podcastId"]=1;p["title"]="Titel";
  p["url"]="https://example.org/a.mp3";p["position"]=10;p["duration"]=nullptr;
  p["status"]="partial";p["revision"]=uint64_t(123);
  assert(podcastEpisode(p,1) && !podcastEpisode(p,2));
  p["position"]=-1;assert(!podcastEpisode(p));p["position"]=0;
  p["duration"]=7*86400+1;assert(!podcastEpisode(p));p["duration"]=0;
  p["url"]="javascript:evil";assert(!podcastEpisode(p));p["url"]="https://example.org/a.mp3";
  p["status"]="fake";assert(!podcastEpisode(p));p["status"]="completed";
  JsonDocument page;page["podcastId"]=1;page["offset"]=0;page["total"]=30;
  auto rows=page["episodes"].to<JsonArray>();rows.add(p);
  assert(podcastPage(page,1));
  for(int i=0;i<20;++i) rows.add(p);
  assert(!podcastPage(page,1));
  JsonDocument single;assert(!podcastSingle(single));
  single["episode"]=nullptr;assert(podcastSingle(single));
  single["episode"]=p;single["offset"]=5;assert(podcastSingle(single,1));
  single["offset"]=-1;assert(!podcastSingle(single,1));
  JsonDocument config;
  config["deviceId"]="test";config["configVersion"]=1;config["age"]=8;config["communicationEnabled"]=true;
  config["pages"].to<JsonArray>();
  assert(deviceConfig(config,"test",1)); // Older servers remain accepted.
  auto channels=config["podcasts"].to<JsonArray>();
  auto channel=channels.add<JsonObject>();channel["id"]=1;channel["name"]="Wissen";channel["episodeCount"]=300;
  assert(deviceConfig(config,"test",1));channels.add(channel);assert(!deviceConfig(config,"test",1));
  puts("PASS: bounded Podcast config/pages, typed episode metadata, URL/ID validation, missing/unknown duration and old-server compatibility");
}

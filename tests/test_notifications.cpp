#include "Notifications.h"
#include "NotificationTone.h"
#include "NotificationNavigation.h"
#include <cassert>
#include <limits>
#include <cstdio>
using namespace leap;
int main() {
  Notifications queue;
  queue.submit({"communication", "Anna", "Hallo", "", 123, NotificationSound::Pling});
  queue.submit({"news", "News", "Erster Titel", "", 12, NotificationSound::Plong, 2, "news_1"}, true);
  queue.submit({"news", "News", "Neuer Titel", "", 12, NotificationSound::Plong, 1, "news_2"}, true);
  assert(queue.tick(UINT32_MAX - 1000));
  assert(queue.visible()->title == "Anna" && queue.unread("communication"));
  assert(!queue.tick(3998)); // Wraparound, still within five seconds.
  assert(queue.tick(3999));
  assert(queue.visible()->text == "Neuer Titel" && queue.visible()->count == 3);
  queue.markRead("communication"); assert(!queue.unread("communication") && queue.unread("news"));
  assert(queue.visible()->target == "news_2");
  assert(!queue.tick(13998) && queue.visible());
  assert(!queue.tick(13999) && !queue.visible());
  queue.submit({"aircraft", "Flight", "", "", 12, NotificationSound::Swish});
  assert(queue.tick(15000));
  assert(!queue.tick(24999) && queue.visible());
  assert(!queue.tick(25000) && !queue.visible());
  assert(notificationMinutes("23:59") == 1439);
  assert(notificationMinutes("24:00") == -1 && notificationMinutes("bad") == -1);
  for (int minute : {1320, 1439, 0, 419}) assert(notificationQuiet(true, "22:00", "07:00", minute));
  for (int minute : {420, 1319}) assert(!notificationQuiet(true, "22:00", "07:00", minute));
  assert(notificationQuiet(true, "09:00", "12:00", 540));
  assert(!notificationQuiet(true, "09:00", "12:00", 720));
  assert(!notificationQuiet(false, "22:00", "07:00", 0));
  assert(notificationQuiet(true, "22:00", "07:00", -1));
  assert(!notificationQuiet(true, "bad", "07:00", -1));
  JsonDocument data;
  auto rows = data.to<JsonArray>();
  rows.add<JsonObject>()["id"] = "news_2";
  rows.add<JsonObject>()["id"] = "news_1";
  assert(notificationTargetIndex(rows, "id", "news_1") == 1);
  assert(notificationTargetIndex(rows, "id", "missing") == -1);
  rows[0]["hex"] = "abc"; rows[1]["hex"] = "def";
  assert(notificationTargetIndex(rows, "hex", "def") == 1);

  NewsNotifications news;
  assert(!news.observe(1, 90, 100));
  assert(!news.observe(2, 100, 100));
  assert(news.observe(3, 101, 100));
  assert(!news.observe(3, 101, 100));
  assert(!news.observe(1, 102, 100)); // Category changes/history never notify again.
  AircraftProximity planes;
  assert(!planes.observe("boot", 1, 1000, 0, 0, true));
  assert(!planes.observe("boot", 1, 1000, 0, 1000));
  assert(!planes.observe("boot", 1, 1000, 0, 900000)); // A long absence does not confirm an exit.
  assert(!planes.observe("new", 4, 1000, 0, 1000));
  assert(planes.observe("new", 2.9, 1000, 0, 2000));
  assert(!planes.observe("new", 3.1, 1000, 0, 3000));
  assert(!planes.observe("new", 2.9, 1000, 0, 4000));
  assert(!planes.observe("new", 3.6, 1000, 0, 5000));
  assert(planes.observe("new", 2.9, 1000, 0, 6000));
  assert(!planes.observe("high", 0, 10000, 0, 6000)); // Directly overhead, >3km high.
  assert(planes.observe("mountain", 0, 10000, 1000, 6000));
  assert(!planes.observe("invalid", 0, std::numeric_limits<double>::quiet_NaN(), 0, 6000));
  assert(!planes.observe("", 0, 1000, 0, 6000));
  NotificationTone pling, plong, swish;
  pling.start(); plong.start(NotificationSound::Plong); swish.start(NotificationSound::Swish);
  bool different = false;
  for (int i = 0; i < 7000; ++i) {
    auto a = pling.next(22050), b = plong.next(22050), c = swish.next(22050);
    different |= a != b && b != c;
  }
  assert(different && !pling.active() && !plong.active() && !swish.active());
  puts("PASS: queued cards, news bundling, five-second wraparound, acknowledgement, silent startup, spatial proximity, hysteresis and three sounds");
}

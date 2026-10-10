#include "QuizTracking.h"
#include "MathQuiz.h"
#include <cassert>
#include <iostream>
using namespace leap;

int main() {
  assert(storage.begin());
  QuizTimer timer;
  assert(!timer.active() && !timer.accepts(100));
  timer.shown(100);
  timer.shown(500); // Re-rendering and detail reading must not restart timing.
  assert(timer.duration(1300) == 1200 && !timer.accepts(99));
  timer.reset();
  timer.shown(UINT32_MAX - 20);
  assert(timer.accepts(30) && timer.duration(30) == 51);

  auto math = generateMathQuestion("multiply", 10, [] { return esp_random(); });
  JsonDocument question, answer;
  question["q"] = math.question;
  auto choices = question["a"].to<JsonArray>();
  for (int n : {math.result, math.result + 1, math.result + 2, math.result + 3})
    choices.add(std::to_string(n));
  int order[4] = {2, 0, 3, 1};
  quizAnswerSnapshot(answer, question.as<JsonVariantConst>(), order, 3, 1250);
  assert(answer["question"] == math.question);
  assert(answer["answers"][1] == std::to_string(math.result));
  assert(answer["answers"][3] == std::to_string(math.result + 1));
  assert(answer["correctIndex"] == 1 && answer["selectedIndex"] == 3);
  assert(answer["elapsedMs"] == 1250 && answer["questionId"].isNull());
  question["id"] = 42;
  quizAnswerSnapshot(answer, question.as<JsonVariantConst>(), order, 1, 2000);
  assert(answer["questionId"] == 42 && answer["answers"].size() == 4);

  JsonDocument numeric;
  question["result"] = math.result;
  mathAnswerSnapshot(numeric, question.as<JsonVariantConst>(), "0012", 3210);
  assert(numeric["answerMode"] == "numeric" && numeric["enteredAnswer"] == "0012");
  assert(numeric["correctAnswer"] == math.result && numeric["elapsedMs"] == 3210);
  assert(numeric["question"] == math.question && numeric["questionId"].isNull());
  assert(numeric["answers"].isNull() && numeric["selectedIndex"].isNull() && numeric["correctIndex"].isNull());

  QuizTracking queue;
  assert(queue.begin());
  answer = numeric;
  answer["eventId"] = "first";
  assert(queue.enqueue(answer));
  JsonDocument first;
  assert(queue.front(first) && first["eventId"] == "first");
  assert(first["enteredAnswer"] == "0012" && first["correctAnswer"] == math.result);
  // Another click while the network uploads the head survives its acknowledgement.
  answer["eventId"] = "second";
  assert(queue.enqueue(answer));
  assert(!queue.acknowledge("wrong"));
  assert(queue.acknowledge("first"));
  QuizTracking reboot;
  assert(reboot.begin());
  assert(reboot.front(first) && first["eventId"] == "second");
  fakeRenameFail = true;
  assert(!reboot.acknowledge("second")); // Retry safely after a failed local acknowledgement.
  assert(reboot.front(first) && first["eventId"] == "second");
  answer["eventId"] = "unsaved";
  assert(!reboot.enqueue(answer));
  fakeRenameFail = false;
  assert(reboot.acknowledge("second") && !reboot.front(first));
  for (size_t i = 0; i < QuizTracking::MaxAttempts; ++i) {
    answer["eventId"] = std::to_string(i);
    assert(reboot.enqueue(answer));
  }
  assert(!reboot.enqueue(answer)); // Bounded flash use; preserve all queued attempts.
  QuizTracking fullReboot;
  assert(fullReboot.begin() && fullReboot.front(first) && first["eventId"] == "0");
  fakeFiles["/quiz-attempts.json"] = std::make_shared<std::string>("broken");
  QuizTracking damaged;
  assert(!damaged.begin() && !damaged.enqueue(answer));
  assert(*fakeFiles["/quiz-attempts.json"] == "broken");
  std::cout << "PASS: quiz permutation, timer rollover, durable retries and bounded outbox\n";
}

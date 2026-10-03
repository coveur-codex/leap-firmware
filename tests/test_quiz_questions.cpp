#include "QuizQuestions.h"
#include <cassert>
#include <iostream>
#include <random>
#include <set>
using namespace leap;
int main() {
  std::mt19937 rng(123);
  JsonDocument quiz;
  auto pool = quiz["questions"].to<JsonArray>();
  uint32_t seen = 0;
  for (int catalog = 0; catalog < 3; ++catalog)
    for (int i = 0; i < 250; ++i) {
      JsonDocument question;
      question["q"] = "catalog-" + std::to_string(catalog) + "-" + std::to_string(i);
      question["catalog"] = catalog;
      question["a"].to<JsonArray>().add("correct");
      collectQuizQuestion(pool, question, seen, rng());
    }
  assert(seen == 750 && pool.size() == 200);
  std::set<std::string> questions;
  std::set<int> catalogs;
  for (JsonObjectConst q : pool) {
    questions.insert(q["q"].as<std::string>());
    catalogs.insert(q["catalog"].as<int>());
    assert(q["a"][0] == "correct");
  }
  assert(questions.size() == 200 && catalogs.size() == 3);
  std::vector<uint16_t> order;
  shuffleQuizQuestions(order, pool.size(), [&] { return rng(); });
  std::set<uint16_t> indices(order.begin(), order.end());
  assert(indices.size() == 200 && *indices.begin() == 0 && *indices.rbegin() == 199);
  int transitions = 0;
  for (unsigned i = 1; i < order.size(); ++i)
    transitions += pool[order[i]]["catalog"] != pool[order[i - 1]]["catalog"];
  assert(transitions > 50); // Catalogs intermixed, not three shuffled blocks.
  auto before = order;
  shuffleQuizQuestions(order, pool.size(), [&] { return rng(); });
  assert(order != before && order[0] != before[0]);
  shuffleQuizQuestions(order, 0, [&] { return rng(); });
  assert(order.empty());
  shuffleQuizQuestions(order, 1, [&] { return rng(); });
  assert(order.size() == 1 && order[0] == 0);
  JsonDocument source, selected;
  auto sourceRows = source["questions"].to<JsonArray>();
  for (int catalogId = 1; catalogId <= 2; ++catalogId)
    for (int i = 0; i < 250; ++i) {
      auto q = sourceRows.add<JsonObject>();
      q["q"] = "question-" + std::to_string(i);
      q["catalogId"] = catalogId;
      q["minAge"] = i % 2 ? 7 : 12;
    }
  source["chosen"] = 2;
  auto selectedRows = selected["questions"].to<JsonArray>();
  collectQuizCatalog(selectedRows, sourceRows, 8, source["chosen"], [&] { return rng(); });
  assert(selectedRows.size() == 125);
  for (JsonObjectConst q : selectedRows) {
    assert(q["catalogId"] == 2);
    assert(q["minAge"] == 7);
  }
  // Changing catalogs clears the previous pool, including at the memory limit.
  selectedRows = selected["questions"].to<JsonArray>();
  source["chosen"] = 1;
  collectQuizCatalog(selectedRows, sourceRows, 12, source["chosen"], [&] { return rng(); });
  assert(selectedRows.size() == 200);
  for (JsonObjectConst q : selectedRows) assert(q["catalogId"] == 1);
  selectedRows = selected["questions"].to<JsonArray>();
  collectQuizCatalog(selectedRows, sourceRows, 6, source["chosen"], [&] { return rng(); });
  assert(selectedRows.size() == 0);
  std::cout << "PASS: reservoir includes all catalogs, globally shuffled unique questions and "
               "fresh rounds\n";
}

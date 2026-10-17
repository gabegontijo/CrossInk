// Orbite add-on: the /Orbite/hoje.json parser (sections, workout, summary, caps) and the local date from the RTC.
#include <gtest/gtest.h>

#include <string>

#include "OrbiteDay.h"

namespace {

const char* const kDay = R"({
  "orbite": 1, "dia": "2026-10-15", "titulo": "Quinta, 15 de outubro", "feitoEm": "07:12",
  "resumo": {"itens": 3, "treino": "Treino A"},
  "secoes": [
    {"titulo": "Agenda", "itens": [{"quando": "14:30", "texto": "Dentista", "nota": ""}]},
    {"titulo": "Vazia", "itens": []},
    {"titulo": "Para fazer", "itens": [{"quando": "", "texto": "Lavar roupa", "nota": "desde 13/10"},
                                      {"quando": "", "texto": "", "nota": "sem texto não entra"}]}
  ],
  "treino": {"titulo": "Treino A", "detalhe": "Smart Fit 06:00–22:00",
             "itens": [{"quando": "3 × 10", "texto": "Supino", "nota": "20 kg"}]},
  "campoNovo": "ignorado"
})";

bool parse(const std::string& json, orbite::Day& day) { return orbite::parseDay(json.data(), json.size(), day); }

}  // namespace

TEST(OrbiteDay, ParsesSectionsAsHeaderRowsAndTheWorkoutApart) {
  orbite::Day day;
  ASSERT_TRUE(parse(kDay, day));
  EXPECT_EQ(day.date, "2026-10-15");
  EXPECT_EQ(day.title, "Quinta, 15 de outubro");
  EXPECT_EQ(day.madeAt, "07:12");
  ASSERT_EQ(day.today.size(), 4u);  // Agenda + 1, Para fazer + 1; the empty section and the blank item are skipped
  EXPECT_TRUE(day.today[0].header);
  EXPECT_EQ(day.today[0].label, "Agenda");
  EXPECT_EQ(day.today[1].value, "14:30");
  EXPECT_EQ(day.today[1].label, "Dentista");
  EXPECT_TRUE(day.today[2].header);
  EXPECT_EQ(day.today[2].label, "Para fazer");
  EXPECT_EQ(day.today[3].subtitle, "desde 13/10");
  EXPECT_EQ(day.workoutTitle, "Treino A");
  EXPECT_EQ(day.workoutDetail, "Smart Fit 06:00–22:00");
  ASSERT_EQ(day.workout.size(), 1u);
  EXPECT_EQ(day.workout[0].value, "3 × 10");
  EXPECT_EQ(day.workout[0].subtitle, "20 kg");
}

TEST(OrbiteDay, RefusesWhatIsNotAnOrbiteDayOfThisVersion) {
  orbite::Day day;
  EXPECT_FALSE(parse(R"({"orbite": 2, "secoes": []})", day));
  EXPECT_FALSE(parse(R"({"secoes": []})", day));
  EXPECT_FALSE(parse("not json", day));
  EXPECT_FALSE(parse("", day));
  EXPECT_FALSE(parse(std::string(orbite::MAX_FILE_BYTES + 1, ' '), day));
  EXPECT_TRUE(day.today.empty());
}

TEST(OrbiteDay, WithoutWorkoutTheTitleIsEmpty) {
  orbite::Day day;
  ASSERT_TRUE(parse(R"({"orbite": 1, "dia": "2026-10-15", "secoes": [], "treino": null})", day));
  EXPECT_TRUE(day.workoutTitle.empty());
  EXPECT_TRUE(day.workout.empty());
}

TEST(OrbiteDay, CapsLongTextOnACharacterBoundaryAndLongLists) {
  std::string longText = "a";                     // 1 byte, so the 120-byte cut falls inside an "é"
  for (int i = 0; i < 100; ++i) longText += "é";  // 2 bytes each
  std::string items;
  for (size_t i = 0; i < orbite::MAX_ITEMS_PER_SECTION + 5; ++i) {
    items += std::string(i ? "," : "") + R"({"texto": ")" + longText + R"("})";
  }
  orbite::Day day;
  ASSERT_TRUE(parse(R"({"orbite": 1, "secoes": [{"titulo": "T", "itens": [)" + items + "]}]}", day));
  ASSERT_EQ(day.today.size(), 1u + orbite::MAX_ITEMS_PER_SECTION);
  const std::string& clipped = day.today[1].label;
  EXPECT_EQ(clipped.size(), orbite::MAX_TEXT_BYTES - 1) << "backs off to the start of the cut character";
  EXPECT_EQ(clipped.substr(clipped.size() - 2), "é") << "no half character at the end";
}

TEST(OrbiteDay, SummaryReadsOnlyTheResumo) {
  orbite::Summary summary;
  ASSERT_TRUE(orbite::parseSummary(kDay, std::string(kDay).size(), summary));
  EXPECT_EQ(summary.items, 3);
  EXPECT_EQ(summary.workoutTitle, "Treino A");
  const std::string other = R"({"orbite": 9, "resumo": {"itens": 3}})";
  EXPECT_FALSE(orbite::parseSummary(other.data(), other.size(), summary));
}

TEST(OrbiteDay, LocalDateAppliesTheOffsetAcrossDayMonthAndYear) {
  // São Paulo is UTC-3: biased 48 - 12 = 36.
  EXPECT_EQ(orbite::localDate(2026, 10, 15, 12, 0, 36), "2026-10-15");
  EXPECT_EQ(orbite::localDate(2026, 10, 16, 2, 30, 36), "2026-10-15");
  EXPECT_EQ(orbite::localDate(2026, 3, 1, 1, 0, 36), "2026-02-28");
  EXPECT_EQ(orbite::localDate(2028, 3, 1, 1, 0, 36), "2028-02-29");
  EXPECT_EQ(orbite::localDate(2027, 1, 1, 0, 10, 36), "2026-12-31");
  EXPECT_EQ(orbite::localDate(2026, 12, 31, 23, 0, 52), "2027-01-01");  // UTC+1
  EXPECT_EQ(orbite::localDate(2026, 2, 30, 12, 0, 48), "");
}

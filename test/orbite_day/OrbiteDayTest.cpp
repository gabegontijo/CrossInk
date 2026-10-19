// Orbite add-on: the /Orbite/hoje.json parser (each part through its filter, ids, what is done, caps), the Home
// summary, the marks file (marked and unmarked) and the marking rules (a done item unmarked is handed back; a workout
// marked when all its exercises are), and the local date from the RTC.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "OrbiteDay.h"

namespace {

const char* const kDay = R"({
  "orbite": 2, "dia": "2026-10-15", "titulo": "Quinta, 15 de outubro", "feitoEm": "07:12",
  "resumo": {"itens": 3, "listas": 1, "treinos": 2, "treinoDoDia": "Treino A"},
  "secoes": [
    {"titulo": "Agenda", "itens": [{"id": "c:dentista01", "quando": "14:30", "texto": "Dentista", "nota": "Centro"}]},
    {"titulo": "Vazia", "itens": []},
    {"titulo": "Contas", "itens": [{"id": "", "quando": "", "texto": "Luz", "nota": "R$ 230,00 · hoje"},
                                  {"id": "", "quando": "", "texto": "", "nota": "sem texto não entra"}]}
  ],
  "listas": [{"titulo": "Supermercado", "itens": [{"id": "l:lista-mercado/leite00001", "quando": "", "texto": "Leite", "nota": ""},
                                                {"id": "l:lista-mercado/pao0000001", "quando": "", "texto": "Pão", "nota": "", "feito": true}]}],
  "treinos": [
    {"id": "t:fichaA0001", "titulo": "Treino A", "detalhe": "Smart Fit 06:00–22:00", "hoje": true,
     "itens": [{"id": "e:fichaA0001/supino", "quando": "3 × 10", "texto": "Supino", "nota": "20 kg"},
               {"id": "e:fichaA0001/remada", "quando": "3 × 12", "texto": "Remada", "nota": ""}]},
    {"id": "t:fichaB0001", "titulo": "Treino B", "detalhe": "", "hoje": false,
     "itens": [{"id": "e:fichaB0001/prancha", "quando": "3 × 30 s", "texto": "Prancha", "nota": ""}]}
  ],
  "campoNovo": "ignorado"
})";

orbite::Group group(const orbite::Day& day, const size_t i) {
  return i < day.groups.size() ? day.groups[i] : orbite::Group{};
}

}  // namespace

TEST(OrbiteDay, TodayHasTheSectionsWithIdsAndSubtitlesJoined) {
  orbite::Day day;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Today, day));
  EXPECT_EQ(day.date, "2026-10-15");
  EXPECT_EQ(day.title, "Quinta, 15 de outubro");
  EXPECT_EQ(day.madeAt, "07:12");
  ASSERT_EQ(day.groups.size(), 2u) << "the empty section is not shown";
  EXPECT_EQ(group(day, 0).title, "Agenda");
  ASSERT_EQ(group(day, 0).rows.size(), 1u);
  EXPECT_EQ(group(day, 0).rows[0].id, "c:dentista01");
  EXPECT_EQ(group(day, 0).rows[0].label, "Dentista");
  EXPECT_EQ(group(day, 0).rows[0].subtitle, "14:30 · Centro");
  ASSERT_EQ(group(day, 1).rows.size(), 1u) << "the item without text is not shown";
  EXPECT_EQ(group(day, 1).rows[0].id, "") << "a bill is only to read";
}

TEST(OrbiteDay, ListsAndWorkoutsAreReadOnlyWhenAsked) {
  orbite::Day lists;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Lists, lists));
  ASSERT_EQ(lists.groups.size(), 1u);
  EXPECT_EQ(group(lists, 0).title, "Supermercado");
  EXPECT_EQ(group(lists, 0).rows[0].id, "l:lista-mercado/leite00001");

  orbite::Day workouts;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Workouts, workouts));
  ASSERT_EQ(workouts.groups.size(), 2u);
  EXPECT_EQ(group(workouts, 0).id, "t:fichaA0001");
  EXPECT_TRUE(group(workouts, 0).today);
  EXPECT_EQ(group(workouts, 0).detail, "Smart Fit 06:00–22:00");
  EXPECT_EQ(group(workouts, 0).rows[0].subtitle, "3 × 10 · 20 kg");
  EXPECT_FALSE(group(workouts, 1).today);
}

TEST(OrbiteDay, RefusesWhatIsNotAnOrbiteDayOfThisVersion) {
  orbite::Day day;
  EXPECT_FALSE(orbite::parseDay(R"({"orbite": 1, "secoes": []})", orbite::Part::Today, day));
  EXPECT_FALSE(orbite::parseDay(R"({"secoes": []})", orbite::Part::Today, day));
  EXPECT_FALSE(orbite::parseDay("not json", orbite::Part::Today, day));
  EXPECT_FALSE(orbite::parseDay("", orbite::Part::Today, day));
  EXPECT_TRUE(day.groups.empty());
}

TEST(OrbiteDay, CapsLongTextOnACharacterBoundaryAndLongLists) {
  std::string longText = "a";                     // 1 byte, so the 120-byte cut falls inside an "é"
  for (int i = 0; i < 100; ++i) longText += "é";  // 2 bytes each
  std::string items;
  for (size_t i = 0; i < orbite::MAX_ITEMS_PER_GROUP + 5; ++i) {
    items +=
        std::string(i ? "," : "") + R"({"id": "l:x/)" + std::to_string(i) + R"(", "texto": ")" + longText + R"("})";
  }
  orbite::Day day;
  ASSERT_TRUE(orbite::parseDay(R"({"orbite": 2, "listas": [{"titulo": "T", "itens": [)" + items + "]}]}",
                               orbite::Part::Lists, day));
  const orbite::Group list = group(day, 0);
  ASSERT_EQ(list.rows.size(), orbite::MAX_ITEMS_PER_GROUP);
  const std::string& clipped = list.rows[0].label;
  EXPECT_EQ(clipped.size(), orbite::MAX_TEXT_BYTES - 1) << "backs off to the start of the cut character";
  EXPECT_EQ(clipped.substr(clipped.size() - 2), "é") << "no half character at the end";
}

TEST(OrbiteDay, OneScreenKeepsAtMostMaxRowsInRam) {
  std::string lists;
  for (int l = 0; l < 6; ++l) {
    std::string items;
    for (size_t i = 0; i < orbite::MAX_ITEMS_PER_GROUP; ++i) {
      items += std::string(i ? "," : "") + R"({"id": "l:x/)" + std::to_string(l * 100 + i) + R"(", "texto": "item"})";
    }
    lists += std::string(l ? "," : "") + R"({"titulo": "L", "itens": [)" + items + "]}";
  }
  orbite::Day day;
  ASSERT_TRUE(orbite::parseDay(R"({"orbite": 2, "listas": [)" + lists + "]}", orbite::Part::Lists, day));
  size_t rows = 0;
  for (const auto& g : day.groups) rows += g.rows.size();
  EXPECT_EQ(rows, orbite::MAX_ROWS) << "6 lists of 40 stop at the cap";
}

TEST(OrbiteDay, SummaryReadsOnlyTheResumo) {
  orbite::Summary summary;
  ASSERT_TRUE(orbite::parseSummary(kDay, summary));
  EXPECT_EQ(summary.items, 3);
  EXPECT_EQ(summary.lists, 1);
  EXPECT_EQ(summary.workouts, 2);
  EXPECT_FALSE(orbite::parseSummary(R"({"orbite": 1, "resumo": {"itens": 3}})", summary));
}

TEST(OrbiteDay, MarksRoundTripAndRefuseOtherFiles) {
  orbite::Marks marks;
  ASSERT_TRUE(orbite::parseMarks(
      R"({"orbite": 2, "feitos": ["c:dentista01", 42, "", "c:dentista01", "l:x/1"], "desfeitos": ["l:x/2", "l:x/2"]})",
      marks));
  EXPECT_EQ(marks.done, (std::vector<std::string>{"c:dentista01", "l:x/1"})) << "only text, no empty, no repeat";
  EXPECT_EQ(marks.undone, (std::vector<std::string>{"l:x/2"}));
  orbite::Marks back;
  ASSERT_TRUE(orbite::parseMarks(orbite::serializeMarks(marks), back));
  EXPECT_EQ(back.done, marks.done);
  EXPECT_EQ(back.undone, marks.undone);
  ASSERT_TRUE(orbite::parseMarks(R"({"orbite": 2, "feitos": ["l:x/1"]})", back)) << "a file from before the unmarks";
  EXPECT_TRUE(back.undone.empty());
  EXPECT_FALSE(orbite::parseMarks(R"({"orbite": 1, "feitos": []})", back));
  EXPECT_FALSE(orbite::parseMarks("nada", back));
}

TEST(OrbiteDay, ADoneItemComesCheckedAndUnmarkingItIsHandedBack) {
  orbite::Day lists;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Lists, lists));
  ASSERT_EQ(group(lists, 0).rows.size(), 2u);
  const orbite::Row leite = group(lists, 0).rows[0];
  const orbite::Row pao = group(lists, 0).rows[1];
  EXPECT_FALSE(leite.done);
  ASSERT_TRUE(pao.done);
  orbite::Marks marks;
  EXPECT_FALSE(orbite::isChecked(marks, leite));
  EXPECT_TRUE(orbite::isChecked(marks, pao)) << "done in the Orbite";

  EXPECT_FALSE(orbite::toggle(marks, nullptr, pao, "2026-10-15"));
  EXPECT_EQ(marks.undone, (std::vector<std::string>{"l:lista-mercado/pao0000001"})) << "handed back as unmarked";
  EXPECT_TRUE(marks.done.empty());
  EXPECT_TRUE(orbite::toggle(marks, nullptr, pao, "2026-10-15"));
  EXPECT_EQ(marks.size(), 0u) << "checked again: nothing to hand back";

  // A mark the last sync could not apply (the file stayed) and that came back done: unmarking it moves it, never in
  // both.
  marks.done = {"l:lista-mercado/pao0000001"};
  EXPECT_FALSE(orbite::toggle(marks, nullptr, pao, "2026-10-15"));
  EXPECT_TRUE(marks.done.empty());
  EXPECT_EQ(marks.undone, (std::vector<std::string>{"l:lista-mercado/pao0000001"}));
}

TEST(OrbiteDay, ToggleMarksAndAllExercisesMarkTheWorkout) {
  orbite::Day workouts;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Workouts, workouts));
  const orbite::Group workout = group(workouts, 0);
  const orbite::Row dentista{"c:dentista01", "Dentista", "14:30"};
  orbite::Marks marks;
  EXPECT_TRUE(orbite::toggle(marks, nullptr, dentista, "2026-10-15"));
  EXPECT_TRUE(orbite::isMarked(marks.done, "c:dentista01"));
  EXPECT_FALSE(orbite::toggle(marks, nullptr, dentista, "2026-10-15"));
  EXPECT_EQ(marks.size(), 0u) << "unmarking before the sync takes it back";

  orbite::toggle(marks, &workout, workout.rows[0], "2026-10-16");
  EXPECT_FALSE(orbite::isMarked(marks.done, "t:fichaA0001@2026-10-16")) << "one exercise left";
  orbite::toggle(marks, &workout, workout.rows[1], "2026-10-16");
  EXPECT_TRUE(orbite::isMarked(marks.done, "t:fichaA0001@2026-10-16")) << "all marked: the workout is done that day";
  orbite::toggle(marks, &workout, workout.rows[1], "2026-10-16");
  EXPECT_FALSE(orbite::isMarked(marks.done, "t:fichaA0001@2026-10-16")) << "unmarking one undoes the workout";
}

TEST(OrbiteDay, AnExerciseShortOfACompleteWorkoutLeavesTheWorkoutMarkAlone) {
  orbite::Day workouts;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Workouts, workouts));
  const orbite::Group workout = group(workouts, 0);
  orbite::Marks marks;
  marks.done = {"t:fichaA0001@2026-10-15"};  // the workout marked on the Today screen
  orbite::toggle(marks, &workout, workout.rows[0], "2026-10-15");
  EXPECT_TRUE(orbite::isMarked(marks.done, "t:fichaA0001@2026-10-15")) << "one exercise marked, one left";
  orbite::toggle(marks, &workout, workout.rows[0], "2026-10-15");
  EXPECT_TRUE(orbite::isMarked(marks.done, "t:fichaA0001@2026-10-15")) << "and unmarked again";
}

TEST(OrbiteDay, ProgressCountsTheCheckedRowsThatCanBeMarked) {
  orbite::Day workouts;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Workouts, workouts));
  const orbite::Group workout = group(workouts, 0);
  orbite::Marks marks;
  marks.done = {"e:fichaA0001/remada"};
  const orbite::Progress progress = orbite::progressOf(marks, workout);
  EXPECT_EQ(progress.checked, 1u);
  EXPECT_EQ(progress.total, 2u);

  orbite::Day today;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Today, today));
  EXPECT_EQ(orbite::progressOf(marks, group(today, 1)).total, 0u) << "a bill is not counted";
  orbite::Day lists;
  ASSERT_TRUE(orbite::parseDay(kDay, orbite::Part::Lists, lists));
  const orbite::Progress done = orbite::progressOf(orbite::Marks{}, group(lists, 0));
  EXPECT_EQ(done.checked, 1u) << "what came done counts as checked";
  EXPECT_EQ(done.total, 2u);
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

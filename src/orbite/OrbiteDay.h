#pragma once

// Orbite add-on: what the Orbite app (on the owner's Mac) writes to the SD card as /Orbite/hoje.json — the day, the
// lists and the workout sheets, each item with the id the reader hands back when it is marked, and what is already done
// there — and the marks the reader keeps in /Orbite/feitos.json until the next sync. Parsing and the marking rules are
// pure (ArduinoJson + std::string) so they run in the native test suite; reading the SD card lives in OrbiteStore.

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace orbite {

constexpr char HOJE_PATH[] = "/Orbite/hoje.json";
constexpr char FEITOS_PATH[] = "/Orbite/feitos.json";
constexpr int FORMAT_VERSION = 2;
// Caps keep a hand-edited or damaged file from exhausting the C3 heap.
constexpr size_t MAX_FILE_BYTES = 48 * 1024;
constexpr size_t MAX_GROUPS = 12;
constexpr size_t MAX_ITEMS_PER_GROUP = 40;
// All the items of one screen: the reader keeps them in RAM while it is open.
constexpr size_t MAX_ROWS = 200;
constexpr size_t MAX_TEXT_BYTES = 120;
constexpr size_t MAX_MARKS = 500;
constexpr size_t MAX_ID_BYTES = 100;

struct Row {
  std::string id;        // what the reader hands back marked; empty when it is only to read (a bill)
  std::string label;     // the item ("Dentista")
  std::string subtitle;  // the time or sets and the note ("14:30 · Centro", "3 × 10 · 20 kg")
  bool done = false;     // already done in the Orbite ("feito"): shown checked; unmarking it is handed back
};

struct Group {
  std::string id;      // a workout's own id ("t:<sheet>"): all its exercises marked makes "<id>@<day>"
  std::string title;   // "Agenda", "Supermercado", "Treino A"
  std::string detail;  // "Smart Fit 06:00–22:00"
  bool today = false;  // the workout of the day
  std::vector<Row> rows;
};

enum class Part : uint8_t { Today, Lists, Workouts };

struct Day {
  std::string date;           // "AAAA-MM-DD", the day it was made for
  std::string title;          // "Quinta, 15 de outubro"
  std::string madeAt;         // "07:12"
  std::vector<Group> groups;  // only the part that was asked for
};

struct Summary {
  uint16_t items = 0;     // what is left for today (the Home hint)
  uint16_t lists = 0;     // lists with something left
  uint16_t workouts = 0;  // active workout sheets
};

// The ArduinoJson filter for one part of the day, or for the Home summary: only that part takes heap.
void dayFilter(JsonDocument& filter, Part part);
void summaryFilter(JsonDocument& filter);

// From a document deserialized through the filter. False (and `out` cleared) when it is not an Orbite day of this
// version.
bool readDay(JsonVariantConst doc, Part part, Day& out);
bool readSummary(JsonVariantConst doc, Summary& out);

// What changed on the reader since the last sync: the items marked ("feitos") and, of those the Orbite sent done, the
// ones unmarked ("desfeitos"). An id is in one list at most; together they keep at most MAX_MARKS.
struct Marks {
  std::vector<std::string> done;
  std::vector<std::string> undone;
  size_t size() const { return done.size() + undone.size(); }
};

// The marks file, {"orbite": 2, "feitos": [...], "desfeitos": [...]} ("desfeitos" may be missing). False when it is not
// an Orbite marks file.
bool readMarks(JsonVariantConst doc, Marks& out);
std::string serializeMarks(const Marks& marks);

// Convenience for a JSON text (the native tests).
bool parseDay(const std::string& json, Part part, Day& out);
bool parseSummary(const std::string& json, Summary& out);
bool parseMarks(const std::string& json, Marks& out);

bool isMarked(const std::vector<std::string>& ids, const std::string& id);
// Whether the row shows checked: one the Orbite sent done unless it was unmarked here; any other if it was marked here.
bool isChecked(const Marks& marks, const Row& row);
// How many of a group's rows that can be marked are checked, of how many: "3/7" beside a sheet on the Workouts list.
struct Progress {
  size_t checked = 0;
  size_t total = 0;
};
Progress progressOf(const Marks& marks, const Group& group);

// Checks or unchecks one row. Inside a workout, checking its last exercise also marks the workout "<group id>@<date>",
// and unchecking one of a complete workout unmarks it. Returns whether the row is now checked.
bool toggle(Marks& marks, const Group* workout, const Row& row, const std::string& date);

// The local calendar day ("AAAA-MM-DD") from the RTC's UTC date/time plus the user's offset in quarter hours
// (biased: 48 = UTC+0, as CrossPointSettings::clockUtcOffsetQ).
std::string localDate(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t offsetQBiased);

}  // namespace orbite

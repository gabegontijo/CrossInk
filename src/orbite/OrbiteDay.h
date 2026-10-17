#pragma once

// Orbite add-on: the day that the Orbite app (on the owner's Mac) writes to the SD card as /Orbite/hoje.json.
// Read-only on the device. The parser is pure (ArduinoJson + std::string) so it runs in the native test suite.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace orbite {

constexpr char HOJE_PATH[] = "/Orbite/hoje.json";
// The Orbite writes a few KB; anything bigger is not ours.
constexpr size_t MAX_FILE_BYTES = 24 * 1024;
constexpr int FORMAT_VERSION = 1;
// Caps keep a hand-edited or damaged file from exhausting the C3 heap.
constexpr size_t MAX_SECTIONS = 8;
constexpr size_t MAX_ITEMS_PER_SECTION = 40;
constexpr size_t MAX_TEXT_BYTES = 120;

struct Row {
  std::string label;     // the item ("Dentista"), or the section title when header
  std::string subtitle;  // the note ("desde 13/10", "R$ 230,00 · hoje", "20 kg")
  std::string value;     // the time ("14:30") or the sets ("3 × 10")
  bool header = false;
};

struct Day {
  std::string date;           // "AAAA-MM-DD", the day it was made for
  std::string title;          // "Quinta, 15 de outubro"
  std::string madeAt;         // "07:12"
  std::string workoutTitle;   // "Treino A", or empty when there is no workout today
  std::string workoutDetail;  // "Smart Fit 06:00–22:00"
  std::vector<Row> today;     // the sections, each title as a header row
  std::vector<Row> workout;   // the exercises
};

struct Summary {
  uint16_t items = 0;        // pending items of the day (the Home row hint)
  std::string workoutTitle;  // empty: no Workout row on Home
};

// Full parse. False (and `out` cleared) when the JSON is not an Orbite day of a known version.
bool parseDay(const char* json, size_t length, Day& out);

// Only the "resumo" object, through an ArduinoJson filter, so Home pays a few hundred bytes of heap.
bool parseSummary(const char* json, size_t length, Summary& out);

// The local calendar day ("AAAA-MM-DD") from the RTC's UTC date/time plus the user's offset in quarter hours
// (biased: 48 = UTC+0, as CrossPointSettings::clockUtcOffsetQ).
std::string localDate(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t offsetQBiased);

}  // namespace orbite

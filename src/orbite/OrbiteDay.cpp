#include "OrbiteDay.h"

#include <ArduinoJson.h>

#include <cstdio>

namespace orbite {
namespace {

// Copy at most MAX_TEXT_BYTES, backing off to a UTF-8 boundary so a cut never leaves half a character.
std::string clip(const char* text) {
  if (text == nullptr) return {};
  std::string out(text);
  if (out.size() <= MAX_TEXT_BYTES) return out;
  size_t end = MAX_TEXT_BYTES;
  while (end > 0 && (static_cast<unsigned char>(out[end]) & 0xC0) == 0x80) --end;
  out.resize(end);
  return out;
}

void appendItems(JsonArrayConst items, std::vector<Row>& out) {
  size_t count = 0;
  for (JsonObjectConst item : items) {
    if (count++ >= MAX_ITEMS_PER_SECTION) break;
    Row row;
    row.label = clip(item["texto"] | "");
    if (row.label.empty()) continue;
    row.subtitle = clip(item["nota"] | "");
    row.value = clip(item["quando"] | "");
    out.push_back(std::move(row));
  }
}

bool isOrbiteDocument(JsonVariantConst doc) { return (doc["orbite"] | 0) == FORMAT_VERSION; }

bool isLeap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int daysInMonth(int year, int month) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeap(year) ? 29 : kDays[month - 1];
}

}  // namespace

bool parseDay(const char* json, const size_t length, Day& out) {
  out = Day{};
  if (json == nullptr || length == 0 || length > MAX_FILE_BYTES) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json, length) || !isOrbiteDocument(doc)) return false;

  out.date = clip(doc["dia"] | "");
  out.title = clip(doc["titulo"] | "");
  out.madeAt = clip(doc["feitoEm"] | "");

  size_t sections = 0;
  for (JsonObjectConst section : doc["secoes"].as<JsonArrayConst>()) {
    if (sections++ >= MAX_SECTIONS) break;
    const size_t before = out.today.size();
    Row header;
    header.label = clip(section["titulo"] | "");
    header.header = true;
    out.today.push_back(std::move(header));
    appendItems(section["itens"].as<JsonArrayConst>(), out.today);
    // A section without items is not shown.
    if (out.today.size() == before + 1) out.today.pop_back();
  }

  JsonObjectConst workout = doc["treino"].as<JsonObjectConst>();
  if (!workout.isNull()) {
    out.workoutTitle = clip(workout["titulo"] | "");
    out.workoutDetail = clip(workout["detalhe"] | "");
    appendItems(workout["itens"].as<JsonArrayConst>(), out.workout);
  }
  return true;
}

bool parseSummary(const char* json, const size_t length, Summary& out) {
  out = Summary{};
  if (json == nullptr || length == 0 || length > MAX_FILE_BYTES) return false;
  JsonDocument filter;
  filter["orbite"] = true;
  filter["resumo"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, json, length, DeserializationOption::Filter(filter)) || !isOrbiteDocument(doc)) return false;
  const int items = doc["resumo"]["itens"] | 0;
  out.items = static_cast<uint16_t>(items < 0 ? 0 : (items > 999 ? 999 : items));
  out.workoutTitle = clip(doc["resumo"]["treino"] | "");
  return true;
}

std::string localDate(uint16_t year, uint8_t month, uint8_t day, const uint8_t hour, const uint8_t minute,
                      const uint8_t offsetQBiased) {
  if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) return {};
  const int minutes = hour * 60 + minute + (static_cast<int>(offsetQBiased) - 48) * 15;
  const int shift = minutes < 0 ? -1 : (minutes >= 24 * 60 ? 1 : 0);
  if (shift < 0) {
    if (--day == 0) {
      if (--month == 0) {
        month = 12;
        --year;
      }
      day = static_cast<uint8_t>(daysInMonth(year, month));
    }
  } else if (shift > 0) {
    if (++day > daysInMonth(year, month)) {
      day = 1;
      if (++month > 12) {
        month = 1;
        ++year;
      }
    }
  }
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", static_cast<unsigned>(year), static_cast<unsigned>(month),
           static_cast<unsigned>(day));
  return buffer;
}

}  // namespace orbite

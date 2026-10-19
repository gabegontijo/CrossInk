#include "OrbiteDay.h"

#include <algorithm>
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

std::string joined(const std::string& first, const std::string& second) {
  if (first.empty()) return second;
  if (second.empty()) return first;
  return first + " · " + second;
}

void readRows(JsonArrayConst items, std::vector<Row>& out, size_t& budget) {
  size_t count = 0;
  for (JsonObjectConst item : items) {
    if (count++ >= MAX_ITEMS_PER_GROUP || budget == 0) break;
    Row row;
    row.label = clip(item["texto"] | "");
    if (row.label.empty()) continue;
    const std::string id = item["id"] | "";
    if (id.size() <= MAX_ID_BYTES) row.id = id;
    row.subtitle = joined(clip(item["quando"] | ""), clip(item["nota"] | ""));
    row.done = item["feito"] | false;
    out.push_back(std::move(row));
    --budget;
  }
}

const char* partKey(const Part part) {
  switch (part) {
    case Part::Lists:
      return "listas";
    case Part::Workouts:
      return "treinos";
    case Part::Today:
    default:
      return "secoes";
  }
}

bool isOrbiteDocument(JsonVariantConst doc) { return (doc["orbite"] | 0) == FORMAT_VERSION; }

bool isLeap(const int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int daysInMonth(const int year, const int month) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeap(year) ? 29 : kDays[month - 1];
}

bool deserialize(const std::string& json, JsonDocument& filter, JsonDocument& doc) {
  if (json.empty() || json.size() > MAX_FILE_BYTES) return false;
  return !deserializeJson(doc, json, DeserializationOption::Filter(filter));
}

// The ids of one list of the marks file: only text, no empty, no repeat, and no more than MAX_MARKS with the `taken`.
void readIds(JsonArrayConst array, std::vector<std::string>& out, const size_t taken) {
  for (JsonVariantConst mark : array) {
    if (taken + out.size() >= MAX_MARKS) break;
    if (!mark.is<const char*>()) continue;
    const std::string id = mark.as<const char*>();
    if (id.empty() || id.size() > MAX_ID_BYTES || isMarked(out, id)) continue;
    out.push_back(id);
  }
}

void addIds(JsonDocument& doc, const char* key, const std::vector<std::string>& ids) {
  JsonArray array = doc[key].to<JsonArray>();
  for (const auto& id : ids) array.add(id);
}

// Puts the id in the list, or takes it out; the other list never keeps it.
void flip(Marks& marks, std::vector<std::string>& list, std::vector<std::string>& other, const std::string& id) {
  other.erase(std::remove(other.begin(), other.end(), id), other.end());
  const auto found = std::find(list.begin(), list.end(), id);
  if (found != list.end()) {
    list.erase(found);
  } else if (marks.size() < MAX_MARKS) {
    list.push_back(id);
  }
}

// Checking the last exercise of a workout marks the workout that day; unchecking one of a complete workout unmarks it
// (marked on the Today screen before, too). Short of complete, before and after, the workout mark stays as it is.
void followWorkout(Marks& marks, const Group& workout, const Row& changed, const bool checked,
                   const std::string& date) {
  const bool othersChecked = std::all_of(workout.rows.begin(), workout.rows.end(), [&](const Row& row) {
    return row.id.empty() || row.id == changed.id || isChecked(marks, row);
  });
  if (!othersChecked) return;
  const std::string done = workout.id + "@" + date;
  const auto at = std::find(marks.done.begin(), marks.done.end(), done);
  if (checked && at == marks.done.end() && marks.size() < MAX_MARKS) marks.done.push_back(done);
  if (!checked && at != marks.done.end()) marks.done.erase(at);
}

}  // namespace

void dayFilter(JsonDocument& filter, const Part part) {
  filter["orbite"] = true;
  filter["dia"] = true;
  filter["titulo"] = true;
  filter["feitoEm"] = true;
  filter[partKey(part)] = true;
}

void summaryFilter(JsonDocument& filter) {
  filter["orbite"] = true;
  filter["resumo"] = true;
}

bool readDay(JsonVariantConst doc, const Part part, Day& out) {
  out = Day{};
  if (!isOrbiteDocument(doc)) return false;
  out.date = clip(doc["dia"] | "");
  out.title = clip(doc["titulo"] | "");
  out.madeAt = clip(doc["feitoEm"] | "");
  size_t budget = MAX_ROWS;
  for (JsonObjectConst source : doc[partKey(part)].as<JsonArrayConst>()) {
    if (out.groups.size() >= MAX_GROUPS || budget == 0) break;
    Group group;
    group.id = clip(source["id"] | "");
    group.title = clip(source["titulo"] | "");
    group.detail = clip(source["detalhe"] | "");
    group.today = source["hoje"] | false;
    readRows(source["itens"].as<JsonArrayConst>(), group.rows, budget);
    // A group without items is not shown.
    if (!group.rows.empty()) out.groups.push_back(std::move(group));
  }
  return true;
}

bool readSummary(JsonVariantConst doc, Summary& out) {
  out = Summary{};
  if (!isOrbiteDocument(doc)) return false;
  const auto count = [](const int n) { return static_cast<uint16_t>(n < 0 ? 0 : (n > 999 ? 999 : n)); };
  out.items = count(doc["resumo"]["itens"] | 0);
  out.lists = count(doc["resumo"]["listas"] | 0);
  out.workouts = count(doc["resumo"]["treinos"] | 0);
  return true;
}

bool readMarks(JsonVariantConst doc, Marks& out) {
  out = Marks{};
  if (!isOrbiteDocument(doc) || !doc["feitos"].is<JsonArrayConst>()) return false;
  readIds(doc["feitos"].as<JsonArrayConst>(), out.done, 0);
  readIds(doc["desfeitos"].as<JsonArrayConst>(), out.undone, out.done.size());
  return true;
}

std::string serializeMarks(const Marks& marks) {
  JsonDocument doc;
  doc["orbite"] = FORMAT_VERSION;
  addIds(doc, "feitos", marks.done);
  addIds(doc, "desfeitos", marks.undone);
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool parseDay(const std::string& json, const Part part, Day& out) {
  out = Day{};
  JsonDocument filter;
  dayFilter(filter, part);
  JsonDocument doc;
  return deserialize(json, filter, doc) && readDay(doc.as<JsonVariantConst>(), part, out);
}

bool parseSummary(const std::string& json, Summary& out) {
  out = Summary{};
  JsonDocument filter;
  summaryFilter(filter);
  JsonDocument doc;
  return deserialize(json, filter, doc) && readSummary(doc.as<JsonVariantConst>(), out);
}

bool parseMarks(const std::string& json, Marks& out) {
  out = Marks{};
  JsonDocument doc;
  if (json.empty() || json.size() > MAX_FILE_BYTES || deserializeJson(doc, json)) return false;
  return readMarks(doc.as<JsonVariantConst>(), out);
}

bool isMarked(const std::vector<std::string>& ids, const std::string& id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool isChecked(const Marks& marks, const Row& row) {
  return row.done ? !isMarked(marks.undone, row.id) : isMarked(marks.done, row.id);
}

Progress progressOf(const Marks& marks, const Group& group) {
  Progress progress;
  for (const Row& row : group.rows) {
    if (row.id.empty()) continue;
    ++progress.total;
    if (isChecked(marks, row)) ++progress.checked;
  }
  return progress;
}

bool toggle(Marks& marks, const Group* workout, const Row& row, const std::string& date) {
  if (row.id.empty()) return false;
  const bool before = isChecked(marks, row);
  // A row the Orbite sent done is unmarked into "desfeitos" (and checked again out of it); any other, into "feitos".
  if (row.done) {
    flip(marks, marks.undone, marks.done, row.id);
  } else {
    flip(marks, marks.done, marks.undone, row.id);
  }
  const bool checked = isChecked(marks, row);
  if (workout != nullptr && !workout->id.empty() && checked != before)
    followWorkout(marks, *workout, row, checked, date);
  return checked;
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

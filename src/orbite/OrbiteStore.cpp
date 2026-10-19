#include "OrbiteStore.h"

#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>

#include "CrossPointSettings.h"

namespace orbite {
namespace {

constexpr char FEITOS_TMP_PATH[] = "/Orbite/feitos.json.tmp";

// ArduinoJson reads straight from the SD file, so a big day never sits in RAM as text — only the filtered part does.
struct FileReader {
  HalFile& file;
  int read() { return file.read(); }
  size_t readBytes(char* buffer, const size_t length) {
    const int got = file.read(buffer, length);
    return got < 0 ? 0 : static_cast<size_t>(got);
  }
};

// Deserializes the file through the filter (or whole, without one). False when it is missing, too big or not JSON.
bool deserializeFile(const char* path, JsonDocument& doc, JsonDocument* filter) {
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("ORB", path, file)) {
    LOG_ERR("ORB", "Could not open %s", path);
    return false;
  }
  const size_t size = file.fileSize();
  if (size == 0 || size > MAX_FILE_BYTES) {
    file.close();
    LOG_ERR("ORB", "Ignoring %s: %u bytes", path, static_cast<unsigned>(size));
    return false;
  }
  FileReader reader{file};
  const DeserializationError error =
      filter ? deserializeJson(doc, reader, DeserializationOption::Filter(*filter)) : deserializeJson(doc, reader);
  file.close();
  if (error) LOG_ERR("ORB", "Bad JSON in %s: %s", path, error.c_str());
  return !error;
}

bool rtcToday(std::string& out) {
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0;
  if (!halClock.isAvailable() || !halClock.getDateTime(year, month, day, hour, minute) || year < 2024) return false;
  out = localDate(year, month, day, hour, minute, SETTINGS.clockUtcOffsetQ);
  return !out.empty();
}

}  // namespace

bool loadSummary(Summary& out) {
  out = Summary{};
  JsonDocument filter;
  summaryFilter(filter);
  JsonDocument doc;
  return deserializeFile(HOJE_PATH, doc, &filter) && readSummary(doc.as<JsonVariantConst>(), out);
}

bool loadDay(const Part part, Day& out) {
  out = Day{};
  JsonDocument filter;
  dayFilter(filter, part);
  JsonDocument doc;
  const bool ok = deserializeFile(HOJE_PATH, doc, &filter) && readDay(doc.as<JsonVariantConst>(), part, out);
  if (!ok) LOG_ERR("ORB", "No Orbite day in %s", HOJE_PATH);
  return ok;
}

void loadMarks(Marks& out) {
  out = Marks{};
  JsonDocument doc;
  if (deserializeFile(FEITOS_PATH, doc, nullptr)) readMarks(doc.as<JsonVariantConst>(), out);
}

bool saveMarks(const Marks& marks) {
  // Written beside and renamed over, so a power cut never leaves half a file (and the marks of the day lost).
  if (!Storage.ensureDirectoryExists("/Orbite") ||
      !Storage.writeFile(FEITOS_TMP_PATH, String(serializeMarks(marks).c_str()))) {
    LOG_ERR("ORB", "Could not write %s", FEITOS_TMP_PATH);
    return false;
  }
  Storage.remove(FEITOS_PATH);
  if (!Storage.rename(FEITOS_TMP_PATH, FEITOS_PATH)) {
    LOG_ERR("ORB", "Could not rename %s", FEITOS_TMP_PATH);
    return false;
  }
  return true;
}

std::string markDate(const Day& day) {
  std::string today;
  return rtcToday(today) ? today : day.date;
}

bool isStale(const Day& day) {
  std::string today;
  return rtcToday(today) && !day.date.empty() && today != day.date;
}

}  // namespace orbite

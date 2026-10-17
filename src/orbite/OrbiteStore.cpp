#include "OrbiteStore.h"

#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "CrossPointSettings.h"

namespace orbite {
namespace {

// Reads the whole file into a buffer sized to it (a few KB, freed by the caller's scope).
template <typename Parse>
bool withFile(Parse parse) {
  if (!Storage.exists(HOJE_PATH)) return false;
  HalFile file;
  if (!Storage.openFileForRead("ORB", HOJE_PATH, file)) {
    LOG_ERR("ORB", "Could not open %s", HOJE_PATH);
    return false;
  }
  const size_t size = file.fileSize();
  if (size == 0 || size > MAX_FILE_BYTES) {
    file.close();
    LOG_ERR("ORB", "Ignoring %s: %u bytes", HOJE_PATH, static_cast<unsigned>(size));
    return false;
  }
  auto buffer = makeUniqueNoThrow<char[]>(size);
  if (!buffer) {
    file.close();
    LOG_ERR("ORB", "No heap for %u bytes", static_cast<unsigned>(size));
    return false;
  }
  const int read = file.read(buffer.get(), size);
  file.close();
  if (read != static_cast<int>(size)) {
    LOG_ERR("ORB", "Short read of %s", HOJE_PATH);
    return false;
  }
  return parse(buffer.get(), size);
}

}  // namespace

bool loadSummary(Summary& out) {
  return withFile([&out](const char* json, const size_t size) { return parseSummary(json, size, out); });
}

bool loadDay(Day& out) {
  const bool ok = withFile([&out](const char* json, const size_t size) { return parseDay(json, size, out); });
  if (!ok) LOG_ERR("ORB", "No Orbite day in %s", HOJE_PATH);
  return ok;
}

bool isStale(const Day& day) {
  uint16_t year = 0;
  uint8_t month = 0, dayOfMonth = 0, hour = 0, minute = 0;
  if (!halClock.isAvailable() || !halClock.getDateTime(year, month, dayOfMonth, hour, minute) || year < 2024) {
    return false;
  }
  const std::string today = localDate(year, month, dayOfMonth, hour, minute, SETTINGS.clockUtcOffsetQ);
  return !today.empty() && !day.date.empty() && today != day.date;
}

}  // namespace orbite

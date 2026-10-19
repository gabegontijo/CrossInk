#pragma once

// Orbite add-on: reads /Orbite/hoje.json from the SD card a part at a time, and keeps the marks in /Orbite/feitos.json
// (see OrbiteDay.h for the formats).

#include "OrbiteDay.h"

namespace orbite {

// The Home rows: false when there is no Orbite file (or it is not ours).
bool loadSummary(Summary& out);

// One part of the day, for the Today, Lists and Workouts screens.
bool loadDay(Part part, Day& out);

// The marks made on the reader since the last sync (none when the file is missing).
void loadMarks(Marks& out);
bool saveMarks(const Marks& marks);

// The day a mark counts for: today by the RTC when it knows the date, otherwise the day the file was made for.
std::string markDate(const Day& day);

// True when the RTC knows today's date and the file was made for another day.
bool isStale(const Day& day);

}  // namespace orbite

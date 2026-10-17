#pragma once

// Orbite add-on: reads /Orbite/hoje.json from the SD card (see OrbiteDay.h for the format).

#include "OrbiteDay.h"

namespace orbite {

// The Home rows: false when there is no Orbite file (or it is not ours).
bool loadSummary(Summary& out);

// The whole day, for the Today and Workout screens.
bool loadDay(Day& out);

// True when the RTC knows today's date and the file was made for another day.
bool isStale(const Day& day);

}  // namespace orbite

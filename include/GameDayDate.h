#pragma once
#include <time.h>

// Shared by the ESP32 firmware and a native regression test.
// Uses the process TZ rule, set on the CYD by configTzTime().
inline bool formatLocalGameDate(time_t timestamp, char output[11]) {
  if (timestamp < 1735689600) return false; // NTP has not set the clock.
  struct tm local = {};
  if (!localtime_r(&timestamp, &local)) return false;
  return strftime(output, 11, "%Y-%m-%d", &local) == 10;
}

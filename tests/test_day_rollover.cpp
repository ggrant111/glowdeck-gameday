#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GameDayDate.h"

static void expectDate(time_t utc, const char *expected) {
  char buffer[11] = {};
  assert(formatLocalGameDate(utc, buffer));
  if (strcmp(buffer, expected)) {
    fprintf(stderr, "Expected %s, got %s\n", expected, buffer);
    abort();
  }
}

int main() {
  // CST is UTC-6 after the November 1, 2026 DST transition.
  setenv("TZ", "CST6CDT,M3.2.0/2,M11.1.0/2", 1);
  tzset();
  const time_t midnightNov3Utc = 1793685600; // 2026-11-03T06:00:00Z
  expectDate(midnightNov3Utc - 1, "2026-11-02");
  expectDate(midnightNov3Utc, "2026-11-03");
  expectDate(midnightNov3Utc + 9 * 3600, "2026-11-03");
  // Same UTC timestamp is still yesterday in Mountain time.
  setenv("TZ", "MST7MDT,M3.2.0/2,M11.1.0/2", 1);
  tzset();
  expectDate(midnightNov3Utc, "2026-11-02");
  // No synchronized network clock: don't treat the Unix epoch as a valid day.
  char output[11] = {};
  assert(!formatLocalGameDate(0, output));
  puts("PASS: local-midnight rollover, timezone and NTP-unsynced cases");
  return 0;
}

#pragma once
#include <stdint.h>

namespace NhlFetchPolicy {
// NHL's date-scoped scoreboard can be tens of kilobytes on busy nights.
// Streaming through TLS may pause between packets; the previous 4.5s HTTP
// timeout, combined with Stream's default 1s timeout, could cut off JSON.
constexpr uint32_t kConnectTimeoutMs = 5000UL;
constexpr uint16_t kResponseTimeoutMs = 12000;
constexpr uint32_t kFastRetryMs = 5000UL;
constexpr uint32_t kMediumRetryMs = 15000UL;
constexpr uint32_t kSlowRetryMs = 30000UL;

// Retry a few truncated scoreboards quickly, then back off rather than
// repeatedly blocking the touchscreen or WLED restore on a bad network.
inline uint32_t retryIntervalMs(uint8_t consecutiveFailures) {
  if (!consecutiveFailures) return 0;
  if (consecutiveFailures <= 3) return kFastRetryMs;
  if (consecutiveFailures <= 7) return kMediumRetryMs;
  return kSlowRetryMs;
}

inline uint32_t pollIntervalMs(uint32_t usualInterval,
                               uint8_t consecutiveFailures) {
  if (!consecutiveFailures) return usualInterval;
  const uint32_t retry = retryIntervalMs(consecutiveFailures);
  return retry < usualInterval ? retry : usualInterval;
}
}  // namespace NhlFetchPolicy

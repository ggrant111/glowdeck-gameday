#include <assert.h>
#include <stdio.h>
#include "NhlFetchPolicy.h"

int main() {
  using namespace NhlFetchPolicy;
  assert(kResponseTimeoutMs > 4500);
  assert(retryIntervalMs(0) == 0);
  assert(retryIntervalMs(1) == 5000UL);
  assert(retryIntervalMs(3) == 5000UL);
  assert(retryIntervalMs(4) == 15000UL);
  assert(retryIntervalMs(7) == 15000UL);
  assert(retryIntervalMs(8) == 30000UL);
  assert(retryIntervalMs(255) == 30000UL);
  assert(pollIntervalMs(60000UL, 1) == 5000UL);
  assert(pollIntervalMs(60000UL, 4) == 15000UL);
  assert(pollIntervalMs(60000UL, 8) == 30000UL);
  assert(pollIntervalMs(10000UL, 8) == 10000UL);
  assert(pollIntervalMs(20000UL, 4) == 15000UL);
  assert(pollIntervalMs(30000UL, 0) == 30000UL);
  puts("PASS: NHL partial-response retry policy and bounded backoff");
}

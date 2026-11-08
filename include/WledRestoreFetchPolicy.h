#pragma once
#include <stdint.h>

// An active WLED event owns a snapshot with restoreAt=UINT32_MAX. It is NOT
// waiting to restore. Network work must keep running throughout an ongoing
// power play, otherwise NHL clock/SOG/stats freeze and PP_END is never seen.
//
// A real scheduled restore has a finite deadline. Only requests that might
// overlap that restore should yield to the LED restore task.
namespace WledRestoreFetchPolicy {
constexpr uint32_t kHeldUntilEventEnd=UINT32_MAX;
inline bool dueSoon(bool leaseActive,uint32_t restoreAt,
                    uint32_t now,uint32_t fetchBudgetMs) {
  if(!leaseActive || restoreAt==kHeldUntilEventEnd)return false;
  // Signed subtraction handles millis() wrap for finite deadlines within
  // a normal request budget; a past-due restore is immediately important.
  return int32_t(restoreAt-now)<=int32_t(fetchBudgetMs);
}
} // namespace WledRestoreFetchPolicy

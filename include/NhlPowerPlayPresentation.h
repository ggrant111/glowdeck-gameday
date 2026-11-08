#pragma once
#include <stdint.h>

// PP2's side field always identifies the club WITH the man advantage:
//   1=HOME PP, 2=AWAY PP.
// Only the club that was SHORT-HANDED can return to full strength at PP_END.
// Keep PP_START attributed to the advantage team; invert only at PP_END.
// This is presentation/event ownership, not a change to the NHL penalty
// timeline, celebration eligibility, audio, or WLED ownership.
namespace NhlPowerPlayPresentation {
inline uint8_t shortHandedSide(uint8_t advantageSide) {
  return advantageSide==1?2:advantageSide==2?1:0;
}
}

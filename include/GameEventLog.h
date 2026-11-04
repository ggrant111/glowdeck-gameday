#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Fixed-size, RAM-only session event log for the ESP32 CYD.
// No String allocations, flash writes, full screen changes or background task.
// Device results report *HTTP command acknowledgements*, not optical sensing.
namespace GameEventLog {
constexpr uint8_t kCapacity = 64;
constexpr uint8_t kDevices = 4;

enum class Type : uint8_t {
  GOAL = 1, POWER_PLAY_START, POWER_PLAY_END, GAME_START, FINAL_WIN,
  FINAL_LOSS, PREVIEW_GOAL, PREVIEW_POWER_PLAY, PREVIEW_START,
  PREVIEW_WIN, PREVIEW_LOSS, PREVIEW_STOP, REAR_TEST
};
enum class Rear : uint8_t {
  NA = 0, NOT_SELECTED, NOT_ENABLED, SCHEDULED, ACTIVE
};
enum class Wled : uint8_t {
  NA = 0, NOT_SELECTED, PENDING, ACCEPTED, SNAPSHOT_FAILED,
  COMMAND_FAILED, RESTORED, RESTORE_FAILED, RESTORE_RETRYING, SUPERSEDED
};

inline const char *typeLabel(Type t) {
  switch(t) {
    case Type::GOAL: return "Goal";
    case Type::POWER_PLAY_START: return "Power play started";
    case Type::POWER_PLAY_END: return "Power play ended";
    case Type::GAME_START: return "Game started";
    case Type::FINAL_WIN: return "Final — win";
    case Type::FINAL_LOSS: return "Final — loss";
    case Type::PREVIEW_GOAL: return "Goal preview";
    case Type::PREVIEW_POWER_PLAY: return "Power-play preview";
    case Type::PREVIEW_START: return "Game-start preview";
    case Type::PREVIEW_WIN: return "Win preview";
    case Type::PREVIEW_LOSS: return "Loss preview";
    case Type::PREVIEW_STOP: return "Stop/restore preview";
    case Type::REAR_TEST: return "Rear LED preview";
    default: return "Event";
  }
}
inline const char *rearLabel(Rear r) {
  switch(r) {
    case Rear::NOT_SELECTED: return "Not followed";
    case Rear::NOT_ENABLED: return "Disabled";
    case Rear::SCHEDULED: return "Scheduled";
    case Rear::ACTIVE: return "Triggered";
    default: return "Not applicable";
  }
}
inline const char *wledLabel(Wled w) {
  switch(w) {
    case Wled::NOT_SELECTED: return "Not selected";
    case Wled::PENDING: return "Waiting";
    case Wled::ACCEPTED: return "Command accepted";
    case Wled::SNAPSHOT_FAILED: return "Snapshot failed";
    case Wled::COMMAND_FAILED: return "Command failed";
    case Wled::RESTORED: return "Restored";
    case Wled::RESTORE_FAILED: return "Restore failed";
    case Wled::RESTORE_RETRYING: return "Restore retry";
    case Wled::SUPERSEDED: return "Superseded";
    default: return "Not triggered";
  }
}
inline bool matchesCategory(Type t, const char *category) {
  if (!category || !strcmp(category, "all")) return true;
  if (!strcmp(category, "goals")) return t == Type::GOAL;
  if (!strcmp(category, "powerplays"))
    return t == Type::POWER_PLAY_START || t == Type::POWER_PLAY_END;
  if (!strcmp(category, "previews"))
    return t == Type::PREVIEW_GOAL || t == Type::PREVIEW_POWER_PLAY ||
           t == Type::PREVIEW_START || t == Type::PREVIEW_WIN ||
           t == Type::PREVIEW_LOSS || t == Type::PREVIEW_STOP ||
           t == Type::REAR_TEST;
  if (!strcmp(category, "other"))
    return t == Type::GAME_START || t == Type::FINAL_WIN ||
           t == Type::FINAL_LOSS;
  return false;
}
inline void copyAbbr(char (&dst)[4], const char *src) {
  if (!src) { dst[0] = '\0'; return; }
  size_t i=0;
  for (; i<3 && src[i]; ++i) dst[i]=src[i];
  dst[i]='\0';
}
struct Entry {
  uint32_t sequence = 0;
  uint32_t timestamp = 0; // NTP epoch when available; otherwise 0.
  uint32_t uptimeSeconds = 0;
  uint32_t gameId = 0;
  uint16_t homeScore = 0;
  uint16_t awayScore = 0;
  char home[4] = {};
  char away[4] = {};
  char team[4] = {};
  Type type = Type::GOAL;
  uint8_t goalsDelta = 1;
  uint8_t period = 0;
  bool preview = false;
  Rear rear = Rear::NA;
  Wled wled[kDevices] = {Wled::NA, Wled::NA, Wled::NA, Wled::NA};
  uint8_t wledAcknowledgedMask = 0; // Retained even after restore/failure
};
class Buffer {
 public:
  uint8_t size() const { return size_; }
  uint32_t lastSequence() const { return sequence_; }
  void clear() { next_ = 0; size_ = 0; } // Never reuse sequence IDs.
  Entry &add(Type type, uint32_t epoch, uint32_t uptime,
             uint32_t gameId, const char *away, const char *home,
             const char *team, uint16_t awayScore, uint16_t homeScore,
             uint8_t period, bool preview = false, uint8_t delta = 1) {
    Entry &entry = entries_[next_];
    entry = Entry();
    entry.sequence = ++sequence_;
    if (!entry.sequence) entry.sequence = ++sequence_;
    entry.type = type;
    entry.timestamp = epoch;
    entry.uptimeSeconds = uptime;
    entry.gameId = gameId;
    entry.awayScore = awayScore;
    entry.homeScore = homeScore;
    entry.period = period;
    entry.preview = preview;
    entry.goalsDelta = delta;
    copyAbbr(entry.away, away);
    copyAbbr(entry.home, home);
    copyAbbr(entry.team, team);
    next_ = (next_ + 1) % kCapacity;
    if (size_ < kCapacity) ++size_;
    return entry;
  }
  Entry *find(uint32_t sequence) {
    if (!sequence) return nullptr;
    for (uint8_t i=0; i<size_; ++i) {
      Entry *entry = newest(i);
      if (entry && entry->sequence == sequence) return entry;
    }
    return nullptr;
  }
  Entry *newest(uint8_t offset) {
    if (offset >= size_) return nullptr;
    return &entries_[(next_ + kCapacity - 1 - offset) % kCapacity];
  }
  const Entry *newest(uint8_t offset) const {
    if (offset >= size_) return nullptr;
    return &entries_[(next_ + kCapacity - 1 - offset) % kCapacity];
  }
 private:
  Entry entries_[kCapacity] = {};
  uint8_t next_ = 0, size_ = 0;
  uint32_t sequence_ = 0;
};
} // namespace GameEventLog

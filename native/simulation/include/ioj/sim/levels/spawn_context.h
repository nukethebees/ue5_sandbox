#pragma once

namespace ioj::sim {
class EntityLedger;
struct SimClock;

struct SpawnContext {
    EntityLedger& ledger;
    SimClock const& clock;
};
} // namespace ioj::sim

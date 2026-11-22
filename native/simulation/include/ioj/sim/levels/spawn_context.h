#pragma once

namespace ioj::sim {
class EntityLedger;
struct SimClock;
struct EntityTables;

struct SpawnContext {
    EntityTables& tables;
    EntityLedger& ledger;
    SimClock const& clock;
};
} // namespace ioj::sim

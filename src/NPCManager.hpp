#pragma once

#include "core/Core.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include "JSON.hpp"

#include <map>
#include <unordered_map>
#include <vector>

#define RESURRECT_HEIGHT 400

enum Trigger {
    ON_KILLED,
    ON_COMBAT
};

typedef void (*NPCEventHandler)(CNSocket*, BaseNPC*);

struct NPCEvent {
    int32_t npcType;
    int trigger;
    NPCEventHandler handler;

    NPCEvent(int32_t t, int tr, NPCEventHandler hndlr)
        : npcType(t), trigger(tr), handler(hndlr) {}
};

// this should really be called vec3 or something...
struct WarpLocation {
    int x, y, z, instanceID, isInstance, limitTaskID, npcID;
};

namespace NPCManager {
    extern std::map<int32_t, BaseNPC*> NPCs;
    extern std::map<int32_t, WarpLocation> Warps;
    extern std::vector<WarpLocation> RespawnPoints;
    extern std::vector<NPCEvent> NPCEvents;
    extern nlohmann::json NPCData;
    extern int32_t nextId;
    void init();

    void destroyNPC(int32_t);
    void updateNPCPosition(int32_t, int X, int Y, int Z, uint64_t I, int angle);

    void sendToViewable(BaseNPC* npc, void* buf, uint32_t type, size_t size);

    BaseNPC *summonNPC(int x, int y, int z, uint64_t instance, int type, bool respawn=false, bool baseInstance=false);

    BaseNPC* getNearestNPC(std::set<Chunk*>* chunks, int X, int Y, int Z);
}

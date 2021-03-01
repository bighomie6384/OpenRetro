#pragma once

#include "LuaManager.hpp"
#include "../NPCManager.hpp"

namespace LuaManager {
    namespace NPC {
        void init(lua_State *state);

        void push(lua_State *state, int32_t npc);

        // events (to be called by LuaManager)
        void removeNPC(int32_t id);
    }
}
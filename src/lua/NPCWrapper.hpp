#pragma once

#include "LuaManager.hpp"
#include "../NPCManager.hpp"

namespace LuaManager {
    namespace NPC {
        void init(lua_State *state);

        void push(BaseNPC *npc);
    }
}
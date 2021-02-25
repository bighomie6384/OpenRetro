#pragma once

#include "LuaManager.hpp"

namespace LuaManager {
    namespace World {
        void init(lua_State *state);

        // events (called by LuaManager)
        void playerAdded(CNSocket *sock);
        void playerRemoved(CNSocket *sock);
    }
}
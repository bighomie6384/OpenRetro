#pragma once

#include "LuaManager.hpp"
#include "../PlayerManager.hpp" 

namespace LuaManager {
    namespace Player {
        void init(lua_State *state);

        // creates a new player object and pushes it onto the stack
        void push(lua_State *state, CNSocket *sock);

        void clearState(lua_State *state);

        // events (called by LuaManager)
        void playerRemoved(CNSocket *sock);
        void playerChatted(CNSocket *sock, std::string& msg);
    }
}
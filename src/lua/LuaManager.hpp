#pragma once

#include "../PlayerManager.hpp"

#include <string>
#include <lua5.1/lua.hpp>

typedef int lRegistry;

namespace LuaManager {
    void init();

    // runs the script in the passed file
    void runScript(std::string filename);
    void stopScripts();

    // unregisters the events tied to this state with all wrappers
    void clearState(lua_State *state);

    void playerAdded(CNSocket *sock);
    void playerRemoved(CNSocket *sock);
    void playerChatted(CNSocket *sock, std::string& msg);
}

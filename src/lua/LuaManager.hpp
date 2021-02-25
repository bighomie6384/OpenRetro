#pragma once

#include "../PlayerManager.hpp"

#include <string>
#include <lua5.1/lua.hpp>

typedef int lRegistry;

namespace LuaManager {
    void init();

    // creates a new state from the global state and runs the provided script file
    void runScript(std::string filename); 

    void playerAdded(CNSocket *sock);
    void playerRemoved(CNSocket *sock);
}

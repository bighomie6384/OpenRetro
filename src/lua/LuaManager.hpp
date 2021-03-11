#pragma once

#include "../PlayerManager.hpp"

#include <string>
#ifdef _WIN32
    #include <luajit/lua.hpp>
#else
    #include <luajit-2.1/lua.hpp>
#endif

typedef int lRegistry;

namespace LuaManager {
    extern lua_State *global;
    void init();

    // runs the script in the passed file
    void runScript(std::string filename);
    void stopScripts();
    void loadScripts();

    // unregisters the events tied to this state with all wrappers
    void clearState(lua_State *state);

    void playerAdded(CNSocket *sock);
    void playerRemoved(CNSocket *sock);
    void playerChatted(CNSocket *sock, std::string& msg);

    void npcRemoved(int32_t id);
}

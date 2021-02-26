#include "LuaManager.hpp"
#include "PlayerWrapper.hpp"
#include "WorldWrapper.hpp"
#include "LuaWrapper.hpp"

#include <vector>

// our "main" state, holds our environment
lua_State *global;

class Script {
private:
    lua_State *thread;
    lRegistry threadRef; // we'll need to unref this when closing this state

public: 
    Script(std::string source) {
        // make the thread & register it in the registry
        thread = lua_newthread(global);
        threadRef = luaL_ref(global, LUA_REGISTRYINDEX);

        // compile & run the script, if it error'd, print the error
        if (luaL_dofile(thread, source.c_str()) != 0) {
            std::cout << "[LUA ERROR]: " << lua_tostring(thread, -1) << std::endl; 
        }
    }

    // unregister all of our events from the wrappers
    ~Script() {
        LuaManager::clearState(thread);

        // remove it from the global registry
        luaL_unref(global, LUA_REGISTRYINDEX, threadRef);
    }
};

std::vector<Script*> activeScripts;

void LuaManager::init() {
    // allocate our state
    global = luaL_newstate();

    // open lua's base libraries (excluding the IO for now)
    luaopen_base(global);
    luaopen_table(global);
    luaopen_string(global);
    luaopen_math(global);
    luaopen_debug(global);

    // now load our libraries
    Player::init(global);
    World::init(global);

    activeScripts = std::vector<Script*>();
}

void LuaManager::runScript(std::string filename) {
    Script *script = new Script(filename);
    activeScripts.push_back(script);
}

void LuaManager::stopScripts() {
    // free all the scripts, they'll take care of everything for us :)
    for (Script *script : activeScripts) {
        delete script;
    }

    // finally clear the vector
    activeScripts.clear();
}

void LuaManager::clearState(lua_State *state) {
    Player::clearState(state);
    World::clearState(state);
}

void LuaManager::playerAdded(CNSocket *sock) {
    World::playerAdded(sock);
}

void LuaManager::playerRemoved(CNSocket *sock) {
    World::playerRemoved(sock);
    Player::playerRemoved(sock);
}

void LuaManager::playerChatted(CNSocket *sock, std::string& msg) {
    Player::playerChatted(sock, msg);
}
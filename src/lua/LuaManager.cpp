#include "LuaManager.hpp"
#include "PlayerWrapper.hpp"
#include "WorldWrapper.hpp"
#include "LuaWrapper.hpp"

// our "main" state, holds our environment
lua_State *global;

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
}

void LuaManager::runScript(std::string filename) {
    // compile & run the script, if it error'd, print the error
    if (luaL_dofile(global, filename.c_str()) != 0) {
        std::cout << "[LUA ERROR]: " << lua_tostring(global, -1) << std::endl; 
    }
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
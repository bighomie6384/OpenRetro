#include "WorldWrapper.hpp"
#include "LuaWrapper.hpp"
#include "PlayerWrapper.hpp"
#include "EventWrapper.hpp"

#include "../PlayerManager.hpp"

static lEvent *addedEvent;
static lEvent *removedEvent;

#define LIBNAME "world"

// =============================================== [[ GETTERS ]] ===============================================

int wrld_getPlrAdded(lua_State *state) {
    LuaManager::Event::push(state, addedEvent);
    return 1;
}

int wrld_getPlrRemoved(lua_State *state) {
    LuaManager::Event::push(state, removedEvent);
    return 1;
}

int wrld_getPlayers(lua_State *state) {
    // create a new lua table and push it onto the stack
    int entries = 0;
    lua_newtable(state);

    // walk through the current list of players and add them to the table
    for (auto pair : PlayerManager::players) {
        lua_pushinteger(state, ++entries);
        LuaManager::Player::push(state, pair.first);
        lua_rawset(state, -3);
    }

    // returns the player table :)
    return 1;
}

// =============================================== [[ METHODS ]] ===============================================

// world.getNearbyPlayers(x, y, z, dist)
int wrld_getNPlrs(lua_State *state) {
    int x = luaL_checkint(state, 1);
    int y = luaL_checkint(state, 2);
    int z = luaL_checkint(state, 3);
    int dist = luaL_checkint(state, 4);
    int entries = 0;
    lua_newtable(state);

    // walk through players, if they're within distance add it to the table
    for (auto pair : PlayerManager::players) {
        Player *plr = pair.second;

        if (abs(plr->x - x) <= dist &&
            abs(plr->y - y) <= dist &&
            abs(plr->z - z) <= dist) {
            lua_pushinteger(state, ++entries);
            LuaManager::Player::push(state, pair.first);
            lua_rawset(state, -3);
        }
    }

    // return the table
    return 1;
}

int wrld_index(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__wrldGETTERS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // if it's not nil, call it and run the getter method
    if (!lua_isnil(state, -1)) {
        // push userdata & call the function
        lua_pushvalue(state, 1);
        lua_call(state, 1, 1);

        // return # of results
        return 1;
    }

    // grab the function from the methods lookup table
    lua_pop(state, 1);
    lua_pushstring(state, "__wrldMETHODS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // return result
    return 1;
}

static const luaL_Reg getters[] {
    {"onPlayerAdded", wrld_getPlrAdded},
    {"onPlayerRemoved", wrld_getPlrRemoved},
    {"players", wrld_getPlayers},
    {0, 0}
};

static const luaL_Reg methods[] = {
    {"getNearbyPlayers", wrld_getNPlrs},
    {0, 0}
};


void LuaManager::World::init(lua_State *state) {
    lua_newtable(state);
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushcfunction(state, wrld_index);
    lua_rawset(state, -3); // sets meta.__index = plr_index
    lua_setmetatable(state, -2); // sets world.__metatable = meta
    lua_setglobal(state, LIBNAME);

    // setup the __wrldGETTERS table in the registry
    lua_pushstring(state, "__wrldGETTERS");
    lua_newtable(state);
    luaL_register(state, NULL, getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // setup the __wrldMETHODS table in the registry
    lua_pushstring(state, "__wrldMETHODS");
    lua_newtable(state);
    luaL_register(state, NULL, methods);
    lua_rawset(state, LUA_REGISTRYINDEX);

    addedEvent = new lEvent();
    removedEvent = new lEvent();
}

void LuaManager::World::clearState(lua_State *state) {
    addedEvent->clear(state);
    removedEvent->clear(state);
}

void LuaManager::World::playerAdded(CNSocket *sock) {
    addedEvent->call(sock);
}

void LuaManager::World::playerRemoved(CNSocket *sock) {
    removedEvent->call(sock);
}
#include "WorldWrapper.hpp"
#include "LuaWrapper.hpp"
#include "EventWrapper.hpp"

static lEvent addedEvent;
static lEvent removedEvent;

#define LIBNAME "world"

int wrld_plrAdded(lua_State *state) {
    LuaManager::Event::push(state, &addedEvent);
    return 1;
}

int wrld_plrRemoved(lua_State *state) {
    LuaManager::Event::push(state, &removedEvent);
    return 1;
}

int wrld_index(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__wrldGETTERS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // if it's nil, return nil
    if (lua_isnil(state, -1)) {
        lua_pushnil(state);
        return 1;
    }

    // push userdata & call the function
    lua_pushvalue(state, 1);
    lua_call(state, 1, 1);

    // return # of results
    return 1;    
}

static const luaL_reg getters[] {
    {"onPlayerAdded", wrld_plrAdded},
    {"onPlayerRemoved", wrld_plrRemoved},
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

    // setup the _wrldGETTERS table in the registry
    lua_pushstring(state, "__wrldGETTERS");
    lua_newtable(state);
    luaL_register(state, NULL, getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    addedEvent = lEvent();
    removedEvent = lEvent();
}

void LuaManager::World::clearState(lua_State *state) {
    addedEvent.clear(state);
    removedEvent.clear(state);
}

void LuaManager::World::playerAdded(CNSocket *sock) {
    addedEvent.call(sock);
}

void LuaManager::World::playerRemoved(CNSocket *sock) {
    removedEvent.call(sock);
}
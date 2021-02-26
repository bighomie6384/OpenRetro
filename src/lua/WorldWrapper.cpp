#include "WorldWrapper.hpp"
#include "LuaWrapper.hpp"

static lEvent addedEvent;
static lEvent removedEvent;

#define LIBNAME "world"

int wrld_plrAdded(lua_State *state) {
    int nargs = lua_gettop(state);

    // for each argument passed, check that it's a function and add it to the playerAdded event
    for (int i = 1; i <= nargs; i++) {
        luaL_checktype(state, i, LUA_TFUNCTION);
        lua_pushvalue(state, i);
        addedEvent.addCallback(state, luaL_ref(state, LUA_REGISTRYINDEX));
    }

    // we don't push anything
    return 0;
}

int wrld_plrRemoved(lua_State *state) {
    int nargs = lua_gettop(state);

    // for each argument passed, check that it's a function and add it to the playerRemoved event
    for (int i = 1; i <= nargs; i++) {
        luaL_checktype(state, i, LUA_TFUNCTION);
        lua_pushvalue(state, i);
        removedEvent.addCallback(state, luaL_ref(state, LUA_REGISTRYINDEX));
    }

    // we don't push anything
    return 0;
}

static const luaL_reg funcs[] {
    {"onPlayerAdded", wrld_plrAdded},
    {"onPlayerRemoved", wrld_plrRemoved},
    {0, 0}
};

void LuaManager::World::init(lua_State *state) {
    luaL_register(state, LIBNAME, funcs);
    lua_pop(state, 1);

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
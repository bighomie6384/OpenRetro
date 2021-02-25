#include "PlayerWrapper.hpp"
#include "LuaWrapper.hpp"

#include <map>

#define LIBNAME "player"

struct PlayerEvents {
    lEvent onChat;
    lEvent onDisconnect;
};

// our userdata only stores a pointer, but i couldn't just use a light userdata since they all share the same metatable :(
typedef CNSocket* PlrData;
static std::map<CNSocket*, PlayerEvents> eventMap;

lRegistry getterTbl;

// check at stack index if it's a player object, and if so return the CNSocket pointer
static CNSocket* grabSock(lua_State *state, int index) {
    // first, make sure its a userdata
    luaL_checktype(state, index, LUA_TUSERDATA);

    // now, check and make sure its our libraries metatable attached to this userdata
    PlrData *sock = (PlrData*)luaL_checkudata(state, index, LIBNAME);
    if (sock == NULL)
        luaL_typerror(state, index, LIBNAME);

    // check if the player exists still & return NULL if it doesn't
    return PlayerManager::players.find(*sock) != PlayerManager::players.end() ? *sock : NULL;
}

// check at stack index if it's a player object, and if so return the Player pointer
static Player* grabPlayer(lua_State *state, int index) {
    CNSocket *sock = grabSock(state, index);

    if (sock == NULL)
        return NULL;
    
    return PlayerManager::players[sock];
}

// check at stack index if it's a player object, and if so return the PlayerEvents pointer
static PlayerEvents* grabEvents(lua_State *state, int index) {
    CNSocket *sock = grabSock(state, index);

    if (sock == NULL)
        return NULL;
    
    return &eventMap[sock];
}

static void pushPlayer(lua_State *state, CNSocket *sock) {
    // if the event map doesn't have this socket yet, make it
    if (eventMap.find(sock) == eventMap.end())
        eventMap[sock] = {lEvent(), lEvent()};

    // creates the udata and sets the pointer
    PlrData *plr = (PlrData*)lua_newuserdata(state, sizeof(PlrData));
    *plr = sock;

    // attaches our metatable from the registry to the udata
    luaL_getmetatable(state, LIBNAME);
    lua_setmetatable(state, -2);
}


static int plr_moveTo(lua_State *state) {

    return 0;
}

static int plr_getName(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    lua_pushstring(state, PlayerManager::getPlayerName(plr).c_str());
    return 1;
}

// in charge of calling the correct getter method
static int plr_index(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__plrGETTERS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // if it's not null, call it and run the getter method
    if (!lua_isnil(state, -1)) {
        // push table & call the function
        lua_pushvalue(state, 1);
        if (lua_pcall(state, 1, 1, 0) != 0) {
            std::cout << "[ERROR] : " << lua_tostring(state, -1) << std::endl;
            lua_pop(state, 1);
            return 0;
        }

        // return # of results
        return 1;
    }

    // grab the function from the methods lookup table
    lua_pop(state, 1);
    lua_pushstring(state, "__plrMETHODS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // return result
    return 1;
}

static const luaL_reg getters[] = {
    {"name", plr_getName},
    {0, 0}
};

static const luaL_reg methods[] = {
    {"moveTo", plr_moveTo},
    {0, 0}
};

void LuaManager::Player::init(lua_State *state) {
    // register our library as a global (and leave it on the stack)
    luaL_register(state, LIBNAME, methods);

    // create the meta table and populate it with our functions
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushcfunction(state, plr_index);
    lua_rawset(state, -3); // sets meta.__index = methods
    lua_pop(state, 2); // pop the metatable and methods table off the stack

    // create the methods table
    lua_pushstring(state, "__plrMETHODS");
    lua_newtable(state);
    luaL_register(state, NULL, methods);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the getters table
    lua_pushstring(state, "__plrGETTERS");
    lua_newtable(state);
    luaL_register(state, NULL, getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // setup the map
    eventMap = std::map<CNSocket*, PlayerEvents>();
}

// just a wrapper for pushPlayer :)
void LuaManager::Player::push(lua_State *state, CNSocket *sock) {
    pushPlayer(state, sock);
}

void LuaManager::Player::playerRemoved(CNSocket *sock) {
    auto iter = eventMap.find(sock);

    // if we have a PlayerEvent defined, call the event
    if (iter != eventMap.end()) {
        PlayerEvents *e = &iter->second;
        e->onDisconnect.call(sock); // call the event

        // disconnect the events
        e->onDisconnect.clear();
        e->onChat.clear();
    }
}

void LuaManager::Player::playerChatted(CNSocket *sock, std::string &msg) {
    auto iter = eventMap.find(sock);

    // if we have a PlayerEvent defined, call the event
    if (iter != eventMap.end()) {
        PlayerEvents *e = &iter->second;
        e->onDisconnect.call(sock);
    }
}
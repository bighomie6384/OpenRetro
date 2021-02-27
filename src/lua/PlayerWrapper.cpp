#include "PlayerWrapper.hpp"
#include "LuaWrapper.hpp"
#include "EventWrapper.hpp"

#include "../ChatManager.hpp"
#include <map>

#define LIBNAME "player"
#define PLRGONESTR "Player doesn't exist anymore, they left!"

struct PlayerEvents {
    lEvent onChat;
    lEvent onDisconnect;
};

// our userdata only stores a pointer, but i couldn't just use a light userdata since they all share the same metatable :(
typedef CNSocket* PlrData;
static std::map<CNSocket*, PlayerEvents> eventMap;

// check at stack index if it's a player object, and if so return the CNSocket pointer
static CNSocket* grabSock(lua_State *state, int index) {
    // first, make sure its a userdata
    luaL_checktype(state, index, LUA_TUSERDATA);

    // now, check and make sure its our libraries metatable attached to this userdata
    PlrData *sock = (PlrData*)luaL_checkudata(state, index, LIBNAME);
    if (sock == NULL) {
        luaL_typerror(state, index, LIBNAME);
        return NULL;
    }

    // check if the player exists still & return NULL if it doesn't
    return PlayerManager::players.find(*sock) != PlayerManager::players.end() ? *sock : NULL;
}

// check at stack index if it's a player object, and if so return the Player pointer
static Player* grabPlayer(lua_State *state, int index) {
    CNSocket *sock = grabSock(state, index);

    if (sock == NULL) {
        luaL_argerror(state, 1, PLRGONESTR);
        return NULL;
    }

    return PlayerManager::players[sock];
}

// check at stack index if it's a player object, and if so return the PlayerEvents pointer
static PlayerEvents* grabEvents(lua_State *state, int index) {
    CNSocket *sock = grabSock(state, index);

    if (sock == NULL) {
        luaL_argerror(state, 1, PLRGONESTR);
        return NULL;
    }
    
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

static int plr_getOnChat(lua_State *state) {
    PlayerEvents *evnt = grabEvents(state, 1);

    // sanity check
    if (evnt == NULL)
        return 0;

    LuaManager::Event::push(state, &evnt->onChat);
    return 1;
}

static int plr_getOnDisconnect(lua_State *state) {
    PlayerEvents *evnt = grabEvents(state, 1);

    // sanity check
    if (evnt == NULL)
        return 0;

    LuaManager::Event::push(state, &evnt->onDisconnect);
    return 1;
}

static int plr_moveTo(lua_State *state) {
    CNSocket *sock = grabSock(state, 1);

    if (sock == NULL) {
        luaL_argerror(state, 1, PLRGONESTR);
        return 0;
    }

    int x = luaL_checkint(state, 2);
    int y = luaL_checkint(state, 3);
    int z = luaL_checkint(state, 4);

    PlayerManager::sendPlayerTo(sock, x, y, z);
    return 0;
}

static int plr_setSpeed(lua_State *state) {
    Player *plr;
    CNSocket *sock = grabSock(state, 1);
    int newSpeed = luaL_checkint(state, 2);

    // sanity check
    if (sock == NULL) {
        luaL_argerror(state, 1, PLRGONESTR);
        return 0;
    }

    // grab our player
    plr = PlayerManager::players[sock];

    // prep our response packet
    INITSTRUCT(sP_FE2CL_GM_REP_PC_SET_VALUE, response);
    response.iPC_ID = plr->PCStyle.iPC_UID;
    response.iSetValue = newSpeed;
    response.iSetValueType = 6; // 6 is the speed type

    // send the packet
    sock->sendPacket((void*)&response, P_FE2CL_GM_REP_PC_SET_VALUE, sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE));
    return 0;
}

static int plr_setJump(lua_State *state) {
    Player *plr;
    CNSocket *sock = grabSock(state, 1);
    int newJump = luaL_checkint(state, 2);

    // sanity check
    if (sock == NULL) {
        luaL_argerror(state, 1, PLRGONESTR);
        return 0;
    }

    // grab our player
    plr = PlayerManager::players[sock];

    // prep our response packet
    INITSTRUCT(sP_FE2CL_GM_REP_PC_SET_VALUE, response);
    response.iPC_ID = plr->PCStyle.iPC_UID;
    response.iSetValue = newJump;
    response.iSetValueType = 7; // 7 is the jump type

    // send the packet
    sock->sendPacket((void*)&response, P_FE2CL_GM_REP_PC_SET_VALUE, sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE));
    return 0;
}

static int plr_msg(lua_State *state) {
    CNSocket *sock = grabSock(state, 1); // the first argument should be the player
    luaL_checkstring(state, 2); // the second should be the message

    ChatManager::sendServerMessage(sock, std::string(lua_tostring(state, 2)));
    return 0; // we return nothing
}

// =============================================== [[ GETTERS ]] ===============================================

static int plr_getName(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    lua_pushstring(state, PlayerManager::getPlayerName(plr).c_str());
    return 1;
}

static int plr_getX(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    lua_pushnumber(state, plr->x);
    return 1;
}

static int plr_getY(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    lua_pushnumber(state, plr->y);
    return 1;
}

static int plr_getZ(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    lua_pushnumber(state, plr->z);
    return 1;
}

static int plr_getGM(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    lua_pushnumber(state, plr->accountLevel);
    return 1;
}

// =============================================== [[ SETTERS ]] ===============================================

static int plr_setGM(lua_State *state) {
    Player *plr = grabPlayer(state, 1);
    int newLevel = luaL_checkint(state, 2);

    plr->accountLevel = newLevel;
    return 0;
}

// in charge of calling the correct getter method
static int plr_index(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__plrGETTERS");
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
    lua_pushstring(state, "__plrMETHODS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // return result
    return 1;
}

// in charge of calling the correct setter method
static int plr_newindex(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__plrSETTERS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // if it's nil return
    if (lua_isnil(state, -1))
        return 0;

    // push userdata & call the function
    lua_pushvalue(state, 1);
    lua_call(state, 1, 0);

    // return # of results
    return 0;
}

static const luaL_reg getters[] = {
    {"name", plr_getName},
    {"x", plr_getX},
    {"y", plr_getY},
    {"z", plr_getZ},
    {"GM", plr_getGM}, // grabs account level
    {"onChat", plr_getOnChat},
    {"onDisconnect", plr_getOnDisconnect},
    {0, 0}
};

static const luaL_reg setters[] = {
    {"GM", plr_setGM}, // grabs account level
    {0, 0}
};

static const luaL_reg methods[] = {
    {"moveTo", plr_moveTo},
    {"sendMessage", plr_msg},
    {"setSpeed", plr_setSpeed},
    {"setJump", plr_setJump},
    {0, 0}
};

void LuaManager::Player::init(lua_State *state) {
    // register our library as a global (and leave it on the stack)
    luaL_register(state, LIBNAME, methods);

    // create the meta table and populate it with our functions
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushcfunction(state, plr_index);
    lua_rawset(state, -3); // sets meta.__index = plr_index
    lua_pushstring(state, "__newindex");
    lua_pushcfunction(state, plr_newindex);
    lua_rawset(state, -3); // sets meta.__newindex = plr_newindex

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

    // create the setters table
    lua_pushstring(state, "__plrSETTERS");
    lua_newtable(state);
    luaL_register(state, NULL, setters);
    lua_rawset(state, LUA_REGISTRYINDEX);
    lua_pop(state, 5); // pop everything off the stack

    // setup the map
    eventMap = std::map<CNSocket*, PlayerEvents>();
}

// just a wrapper for pushPlayer :)
void LuaManager::Player::push(lua_State *state, CNSocket *sock) {
    pushPlayer(state, sock);
}

void LuaManager::Player::clearState(lua_State *state) {
    // walk through our active event map and clear the state from every event
    for (auto &e : eventMap) {
        e.second.onChat.clear(state);
        e.second.onDisconnect.clear(state);
    }
}

void LuaManager::Player::playerRemoved(CNSocket *sock) {
    auto iter = eventMap.find(sock);

    // if we have a PlayerEvent defined, call the event
    if (iter != eventMap.end()) {
        PlayerEvents *e = &iter->second;
        e->onDisconnect.call(sock); // player <userdata>

        // disconnect the events
        e->onDisconnect.clear();
        e->onChat.clear();
    }

    // remove from the map
    eventMap.erase(iter);
}

void LuaManager::Player::playerChatted(CNSocket *sock, std::string &msg) {
    auto iter = eventMap.find(sock);

    // if we have a PlayerEvent defined, call the event
    if (iter != eventMap.end()) {
        PlayerEvents *e = &iter->second;
        e->onChat.call(msg.c_str()); // msg <string>
    }
}
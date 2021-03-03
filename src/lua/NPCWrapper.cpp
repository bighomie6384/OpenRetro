#include "NPCWrapper.hpp"
#include "LuaWrapper.hpp"
#include "EventWrapper.hpp"
#include "../NPCManager.hpp"
#include "../TransportManager.hpp"

#define LIBNAME "NPC"

struct PuppetData {
    BaseNPC *npc;
    int speed;

    // events
    lEvent *onDestroy;
};

// stores the NPC id
typedef int32_t NPCData;
static std::map<int32_t, PuppetData> puppetMap;

/*
        "Puppet" NPCs are NPCs spawned by this NPCWrapper. They're not "mobs" since they're not handled by the MobManager. These puppets
    can be controlled from Lua. This wrapper also supports grabbing normal NPCs (aka non-mobs) and "puppeteering" them. This behavior is undefined
    when used on MobManager controlled NPCs.
*/
static int32_t spawnPuppet(int x, int y, int z, int type, uint64_t inst) {
    // grab the next id and allocate the NPC
    int id = NPCManager::nextId++;
    BaseNPC *npc = new BaseNPC(x, y, z, 0, inst, type, id);
    npc->npcClass = NPC_PUPPET;

    // intitalize it with the NPCManager
    NPCManager::NPCs[id] = npc;
    NPCManager::updateNPCPosition(id, x, y, z, inst, 0);

    // add it to the puppet map
    puppetMap[id] = {npc, 750, new lEvent()};

    return id;
}

static int32_t grabID(lua_State *state, int indx) {
    // first, make sure its a userdata
    luaL_checktype(state, indx, LUA_TUSERDATA);

    // now, check and make sure its our libraries metatable attached to this userdata
    NPCData *id = (NPCData*)luaL_checkudata(state, indx, LIBNAME);
    if (id == NULL) {
        luaL_typerror(state, indx, LIBNAME);
        return -1;
    }

    return *id;
}

static BaseNPC *grabNPC(lua_State *state, int indx) {
    int32_t id = grabID(state, indx);

    if (id == -1)
        return NULL;

    // check if the NPC exists still & return NULL if it doesn't
    auto iter = NPCManager::NPCs.find(id);
    return iter != NPCManager::NPCs.end() ? (*iter).second : NULL;
}

static PuppetData *grabPuppet(lua_State *state, int indx) {
    int32_t id = grabID(state, indx);

    if (id == -1)
        return NULL;

    // check if the NPC exists still & return NULL if it doesn't
    auto iter = puppetMap.find(id);
    return iter != puppetMap.end() ? &(*iter).second : NULL;
}

static void pushNPC(lua_State *state, int32_t id) {
    // if the event map doesn't have this socket yet, make it
    if (puppetMap.find(id) == puppetMap.end())
        puppetMap[id] = {NPCManager::NPCs[id], 750, new lEvent()};

    // creates the udata and sets the pointer
    NPCData *npc = (NPCData*)lua_newuserdata(state, sizeof(NPCData));
    *npc = id;

    // attaches our metatable from the registry to the udata
    luaL_getmetatable(state, LIBNAME);
    lua_setmetatable(state, -2);
}

// in charge of calling the correct getter method
static int npc_index(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__npcGETTERS");
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
    lua_pushstring(state, "__npcMETHODS");
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // return result
    return 1;
}

// in charge of calling the correct setter method
static int npc_newindex(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, "__npcSETTERS");
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

// =============================================== [[ METHODS ]] ===============================================

// NPC.new(x, y, z, type, [inst])
static int npc_new(lua_State *state) {
    int x = luaL_checkint(state, 1);
    int y = luaL_checkint(state, 2);
    int z = luaL_checkint(state, 3);
    int type = luaL_checkint(state, 4);
    int inst = luaL_optint(state, 5, INSTANCE_OVERWORLD);

    // create the puppet and push it onto the lua stack
    pushNPC(state, spawnPuppet(x, y, z, type, inst));
    return 1;
}

static int npc_exists(lua_State *state) {
    BaseNPC *npc = grabNPC(state, 1);

    lua_pushboolean(state, npc != NULL);
    return 1;
}

static int npc_moveto(lua_State *state) {
    PuppetData *puppet = grabPuppet(state, 1);
    int x = luaL_checkint(state, 2);
    int y = luaL_checkint(state, 3);
    int z = luaL_checkint(state, 4);

    // sanity check
    if (puppet == NULL)
        return 0;

    std::queue<WarpLocation> queue;
    WarpLocation from = {puppet->npc->appearanceData.iX, puppet->npc->appearanceData.iY, puppet->npc->appearanceData.iZ};
    WarpLocation to = {x, y, z};

    // create queue and add it to the transport manager
    TransportManager::lerp(&queue, from, to, puppet->speed);
    TransportManager::NPCQueues[puppet->npc->appearanceData.iNPC_ID] = queue;
    return 0;
}

static int npc_destroy(lua_State *state) {
    int32_t id = grabID(state, 1);

    if (id == -1)
        return 0;

    NPCManager::destroyNPC(id);
    return 0;
}

static const luaL_Reg methods[] = {
    {"new", npc_new},
    {"exists", npc_exists},
    {"moveTo", npc_moveto},
    {"destroy", npc_destroy},
    {0, 0}
};

// =============================================== [[ GETTERS ]] ===============================================

static int npc_getX(lua_State *state) {
    BaseNPC *npc = grabNPC(state, 1);

    // sanity check
    if (npc == NULL)
        return 0;

    lua_pushnumber(state, npc->appearanceData.iX);
    return 1;
}

static int npc_getY(lua_State *state) {
    BaseNPC *npc = grabNPC(state, 1);

    // sanity check
    if (npc == NULL)
        return 0;

    lua_pushnumber(state, npc->appearanceData.iY);
    return 1;
}

static int npc_getZ(lua_State *state) {
    BaseNPC *npc = grabNPC(state, 1);

    // sanity check
    if (npc == NULL)
        return 0;

    lua_pushnumber(state, npc->appearanceData.iZ);
    return 1;
}

static int npc_getSpeed(lua_State *state) {
    PuppetData *puppet = grabPuppet(state, 1);

    if (puppet == NULL)
        return 0;

    lua_pushnumber(state, puppet->speed);
    return 1;
}

static int npc_getOnDestroy(lua_State *state) {
    PuppetData *puppet = grabPuppet(state, 1);

    if (puppet == NULL)
        return 0;

    LuaManager::Event::push(state, puppet->onDestroy);
    return 1;
}

static const luaL_Reg getters[] = {
    {"x", npc_getX},
    {"y", npc_getY},
    {"z", npc_getZ},
    {"speed", npc_getSpeed},
    {"onDestroy", npc_getOnDestroy},
    {0, 0}
};

// =============================================== [[ SETTERS ]] ===============================================

static int npc_setSpeed(lua_State *state) {
    PuppetData *puppet = grabPuppet(state, 1);
    int newSpeed = luaL_checkint(state, 2);

    if (puppet == NULL)
        return 0;

    puppet->speed = newSpeed;
    return 0;
}

static const luaL_Reg setters[] = {
    {"speed", npc_setSpeed},
    {0, 0}
};

void LuaManager::NPC::init(lua_State *state) {
    // register our library as a global (and leave it on the stack)
    luaL_register(state, LIBNAME, methods);

    // create the meta table and populate it with our functions
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushcfunction(state, npc_index);
    lua_rawset(state, -3); // sets meta.__index = plr_index
    lua_pushstring(state, "__newindex");
    lua_pushcfunction(state, npc_newindex);
    lua_rawset(state, -3); // sets meta.__newindex = plr_newindex
    lua_pop(state, 2); // pop methods & meta

    // create the methods table
    lua_pushstring(state, "__npcMETHODS");
    lua_newtable(state);
    luaL_register(state, NULL, methods);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the getters table
    lua_pushstring(state, "__npcGETTERS");
    lua_newtable(state);
    luaL_register(state, NULL, getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the setters table
    lua_pushstring(state, "__npcSETTERS");
    lua_newtable(state);
    luaL_register(state, NULL, setters);
    lua_rawset(state, LUA_REGISTRYINDEX);
}

void LuaManager::NPC::push(lua_State *state, int32_t id) {
    pushNPC(state, id);
}

void LuaManager::NPC::removeNPC(int32_t id) {
    auto iter = puppetMap.find(id);

    // if the puppet exists, remove it from the hashmap
    if (iter != puppetMap.end()) {
        (*iter).second.onDestroy->call((*iter).second.npc);
        delete (*iter).second.onDestroy;
        puppetMap.erase(iter);
    }
}
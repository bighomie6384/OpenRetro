/*
    This loads the Event library, basically a wrapper for lEvents, allows the lua script to register callbacks or wait for an event to be fired
*/

#include "EventWrapper.hpp"
#include "LuaWrapper.hpp"

#define LIBNAME "event"
#define LISTNR "listener"

typedef lEvent* eventData;

struct lstnrData {
    lEvent *event;
    lEvent::rawEvent *rawListener;
};

static void pushListener(lua_State *state, lEvent *event, lEvent::rawEvent *listener) {
    lstnrData *lstnr = (lstnrData*)lua_newuserdata(state, sizeof(lstnrData));
    lstnr->event = event;
    lstnr->rawListener = listener;

    // attaches our metatable from the registry to the udata
    luaL_getmetatable(state, LISTNR);
    lua_setmetatable(state, -2);
}

static void pushEvent(lua_State *state, lEvent *event) {
    eventData *eData = (eventData*)lua_newuserdata(state, sizeof(eventData));
    *eData = event;

    // attaches our metatable from the registry to the udata
    luaL_getmetatable(state, LIBNAME);
    lua_setmetatable(state, -2);
}

static lEvent *grabEvent(lua_State *state, int indx) {
    // first, make sure its a userdata
    luaL_checktype(state, indx, LUA_TUSERDATA);

    // now, check and make sure its our libraries metatable attached to this userdata
    eventData *event = (eventData*)luaL_checkudata(state, indx, LIBNAME);
    if (event == NULL) {
        luaL_typerror(state, indx, LIBNAME);
        return NULL;
    }

    // return the pointer to the lEvent
    return *event;
}

static lstnrData *grabListener(lua_State *state, int indx) {
    // first, make sure its a userdata
    luaL_checktype(state, indx, LUA_TUSERDATA);

    // now, check and make sure its our libraries metatable attached to this userdata
    lstnrData *lstnr = (lstnrData*)luaL_checkudata(state, indx, LISTNR);
    if (lstnr == NULL) {
        luaL_typerror(state, indx, LISTNR);
        return NULL;
    }

    // now check and make sure that the event is still active
    if (!lstnr->event->checkAlive(lstnr->rawListener)) {
        luaL_argerror(state, indx, "Listener has been disconnnected!");
        return NULL;
    }

    // return the pointer to the lEvent
    return lstnr;
}

// connects a callback to an event
static int evnt_listen(lua_State *state) {
    int nargs = lua_gettop(state);
    lEvent *event = grabEvent(state, 1);

    // for each argument passed, check that it's a function and add it to the event
    for (int i = 2; i <= nargs; i++) {
        luaL_checktype(state, i, LUA_TFUNCTION);
        lua_pushvalue(state, i);
        pushListener(state, event, event->addCallback(state, luaL_ref(state, LUA_REGISTRYINDEX)));
    }

    // we return this many listeners
    return nargs - 1;
}

// yields the thread until the event is triggered
static int evnt_wait(lua_State *state) {
    lEvent *event = grabEvent(state, 1);

    event->addWait(state);
    return lua_yield(state, 0);
}

static int lstnr_disconnect(lua_State *state) {
    lstnrData *lstnr = grabListener(state, 1);

    // sanity check
    if (lstnr == NULL)
        return 0;

    // disconnect the event
    lstnr->event->clear(lstnr->rawListener);
    return 0;
}

static luaL_Reg evnt_methods[] = {
    {"listen", evnt_listen},
    {"wait", evnt_wait},
    {0, 0}
};

static luaL_Reg lstnr_methods[] = {
    {"disconnect", lstnr_disconnect},
    {0, 0}
};

void LuaManager::Event::init(lua_State *state) {
    // register the library & pop it
    luaL_register(state, LIBNAME, evnt_methods);

    // create the event metatable
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushvalue(state, -3);
    lua_rawset(state, -3);

    // create the listener metatable
    luaL_newmetatable(state, LISTNR);
    lua_pushstring(state, "__index");
    lua_newtable(state);
    luaL_register(state, NULL, lstnr_methods);
    lua_rawset(state, -3);

    // pop the tables off the stack
    lua_pop(state, 3);
}

// just a wrapper for pushEvent()
void LuaManager::Event::push(lua_State *state, lEvent *event) {
    pushEvent(state, event);
}
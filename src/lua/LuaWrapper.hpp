#pragma once
/*
    This file contains nice-to-haves that tie all of the wrappers together, like auto pushing with
        support for our custom libraries and event support with nice calling templates.
*/

#include <string>
#include <vector>

#include "LuaManager.hpp"
#include "PlayerWrapper.hpp"

#define safeCall(state, nargs, nres) \
    if (lua_pcall(state, nargs, nres, 0) != 0) { \
        std::cout << "[LUA ERROR]: " << lua_tostring(state, -1) << std::endl; \
    }

inline static int lua_autoPush(lua_State* state, int nargs) {
    // return the number of pushed arguments :)
    return nargs;
}

/*
    This function will automatically push all of the passed arguments onto the stack and returns the # of pushed values

    Supported datatypes are:
        double or int : LUA_TNUMBER
        char* or const char* : LUA_TSTRING
        bool : LUA_TBOOLEAN
        lRegistry : grabs the object from the lua registry and pushes it onto the stack
        CNSocket* : creates a player object and pushes it onto the stack
*/
template<typename T, class... Rest>
inline static int lua_autoPush(lua_State* state, int nargs, T arg, Rest... rest) {
    // pick which branch to compile based on the type of arg
    if constexpr(std::is_same<T, int>::value || std::is_same<T, double>::value) {
        lua_pushnumber(state, (lua_Number)arg);
    } else if constexpr(std::is_same<T, char*>::value || std::is_same<T, const char*>::value) {
        lua_pushstring(state, (const char*)arg);
    } else if constexpr(std::is_same<T, lRegistry>::value) {
        // grab the value from the registry
        lua_rawgeti(state, LUA_REGISTRYINDEX, (int)arg);
    } else if constexpr(std::is_same<T, CNSocket*>::value) {
        // create a new player object and push it onto the stack
        LuaManager::Player::push(state, arg);
    } else if constexpr(std::is_same<T, bool>::value) {
        lua_pushboolean(state, arg);
    }

    // recursively call, expanding rest and pushing the left-most rvalue into arg
    return lua_autoPush(state, ++nargs, rest...);
}

// callback event handler, allows multiple callbacks connected to one event
class lEvent {
private:
    struct callable {
        lua_State *state;
        lRegistry reg;
    };
    std::vector<callable> refs; // references given by luaL_ref to our callable value passed by lua

public:
    lEvent() {
        refs = std::vector<callable>();
    }

    void addCallback(lua_State *state, lRegistry ref) {
        refs.push_back({state, ref});
    }

    // walks through the refs and unregister them from the state
    void clear() {
        for (callable &e : refs) {
            luaL_unref(e.state, LUA_REGISTRYINDEX, e.reg);
        }
    }

    template<class... Args> inline void call(Args... args) {
        for (callable &e : refs) {
            // push the callable first, the push all the arguments
            lua_rawgeti(e.state, LUA_REGISTRYINDEX, (int)e.reg);
            int nargs = lua_autoPush(e.state, 0, args...);

            // then call it :)
            safeCall(e.state, nargs, 1);
            lua_pop(e.state, 1);
        }
    }
};
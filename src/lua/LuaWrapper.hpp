#pragma once
/*
    This file contains nice-to-haves that tie all of the wrappers together, like auto pushing with
        support for our custom libraries and event support with nice calling templates.
*/

#include <string>
#include <map>
#include <vector>

#include "LuaManager.hpp"
#include "PlayerWrapper.hpp"

#define yieldCall(state, nargs) \
    int _retCode = lua_resume(state, nargs); \
    if (_retCode != 0 && _retCode != LUA_YIELD) { \
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

enum eventType {
    EVENT_CALLBACK, // standard callback
    EVENT_WAIT // state needs to be resumed with the arguments
};


class lEvent {
public:
    struct rawEvent {
        eventType type;
        lRegistry callback; // unused for EVENT_WAIT
    };

private:
    std::map<lua_State*, std::vector<rawEvent*>> refs; // references given by luaL_ref to our callable value passed by lua

    void registerEvent(lua_State *state, rawEvent *event) {
        auto iter = refs.find(state);

        // if the state hasn't registered any callbacks, make the hashmap index and vector
        if (iter == refs.end())
            (refs[state] = std::vector<rawEvent*>()).push_back(event);
        else // else just push it to the already existing vector
            (*iter).second.push_back(event);
    }

    void freeEvent(lua_State *state, rawEvent *event) {
        if (event->type == EVENT_CALLBACK)
            luaL_unref(state, LUA_REGISTRYINDEX, event->callback);
        delete event;
    }

public:
    lEvent() {
        refs = std::map<lua_State*, std::vector<rawEvent*>>();
    }

    rawEvent* addCallback(lua_State *state, lRegistry ref) {
        rawEvent *newEvent = new rawEvent();
        newEvent->type = EVENT_CALLBACK;
        newEvent->callback = ref;
        registerEvent(state, newEvent);

        return newEvent;
    }

    // yields the thread until the event is called
    rawEvent* addWait(lua_State *state) {
        rawEvent *newEvent = new rawEvent();
        newEvent->type = EVENT_WAIT;
        registerEvent(state, newEvent);

        return newEvent;
    }

    // returns true if the event is still active
    bool checkAlive(rawEvent *event) {
        for (auto &e : refs)
            for (rawEvent *ref : e.second)
                if (ref == event)
                    return true;

        return false;
    }

    // walks through the refs and unregister them from the state
    void clear() {
        for (auto &e : refs) {
            for (rawEvent *ref : e.second) {
                freeEvent(e.first, ref);
            }
        }

        refs.clear();
    }

    // disconnects a specific event
    void clear(rawEvent *event) {
        for (auto &e : refs) {
            for (auto iter = e.second.begin(); iter != e.second.end();) {
                // if this is the event, remove it and return!
                if ((*iter) == event) {
                    freeEvent(e.first, *iter);
                    e.second.erase(iter);
                    return;
                } else
                    ++iter;
            }
        }
    }

    // disconnects all events to this state
    void clear(lua_State *state) {
        // grab the iterator, if the state doesn't exist in this event just return
        auto iter = refs.find(state);
        if (iter == refs.end())
            return;

        // walk through the refs and unref() them from the state
        for (rawEvent *ref : (*iter).second) {
            freeEvent(state, ref);
        }

        // finally, erase it from the hashmap
        refs.erase(iter);
    }

    template<class... Args> inline void call(Args... args) {
        for (auto &e : refs) {
            for (auto iter = e.second.begin(); iter != e.second.end();) {
                rawEvent *ref = (*iter);
                switch (ref->type) {
                    case EVENT_CALLBACK: {
                        // make thread for this callback
                        lua_State *nThread = lua_newthread(e.first);

                        // push the callable first, the push all the arguments
                        lua_rawgeti(nThread, LUA_REGISTRYINDEX, (int)ref->callback);
                        int nargs = lua_autoPush(nThread, 0, args...);

                        // then call it :)
                        yieldCall(nThread, nargs);

                        // increment the iterator
                        ++iter;
                        break;
                    }
                    case EVENT_WAIT: {
                        // remove this event from the queue
                        freeEvent(e.first, ref);
                        e.second.erase(iter);

                        // the :wait() will return the passed arguments
                        int nargs = lua_autoPush(e.first, 0, args...);
                        yieldCall(e.first, nargs);
                        break;
                    }
                    default:
                        std::cout << "[WARN] INVALID EVENT TYPE : " << ref->type << std::endl;
                        ++iter;
                        break;
                }
            }
        }
    }
};
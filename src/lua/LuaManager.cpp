#include "LuaManager.hpp"
#include "PlayerWrapper.hpp"
#include "WorldWrapper.hpp"
#include "LuaWrapper.hpp"
#include "EventWrapper.hpp"
#include "NPCWrapper.hpp"

#include "../settings.hpp"

#include <experimental/filesystem>
#include <vector>

time_t getTime();
class Script;

// our "main" state, holds our environment
lua_State *global;
std::map<lua_State*, Script*> activeScripts;

/*
    Basically each script is treated as a coroutine, when wait() is called it gets yielded and is pushed onto the scheduler queue to be resumed.
*/
class Script {
private:
    lua_State *thread;
    lRegistry threadRef; // we'll need to unref this when closing this state

public: 
    Script(std::string source) {
        // make the thread & register it in the registry
        thread = lua_newthread(global);
        threadRef = luaL_ref(global, LUA_REGISTRYINDEX);

        // add this script to the map
        activeScripts[thread] = this;

        // compile & run the script, if it error'd, print the error
        int _retCode;
        if (luaL_loadfile(thread, source.c_str()) || ((_retCode = lua_resume(thread, 0)) != 0 && (_retCode != LUA_YIELD))) {
            std::cout << "[LUA ERROR]: " << lua_tostring(thread, -1) << std::endl; 
        }
    }

    // unregister all of our events from the wrappers
    ~Script() {
        LuaManager::clearState(thread);

        // remove it from the global registry
        luaL_unref(global, LUA_REGISTRYINDEX, threadRef);
    }

    // c++ moment....
    lua_State* getState() {
        return thread;
    }
};

std::map<lua_State*, time_t> scheduleQueue;

// pauses the script for x seconds
int OF_wait(lua_State *state) {
    double seconds = luaL_checknumber(state, 1);

    // yield the state and push the state onto our scheduler queue
    scheduleQueue[state] = (int)(seconds*1000) + getTime();
    return lua_yield(state, 0);
}

void luaScheduler(CNServer *serv, time_t currtime) {
    for (auto iter = scheduleQueue.begin(); iter != scheduleQueue.end();) {
        time_t event = (*iter).second;
        lua_State *thread = (*iter).first;
        // is it time to run the event?
        if (event <= currtime) {
            // remove from the scheduler queue
            scheduleQueue.erase(iter++);
            
            // resume the state, (wait() returns the delta time since call)
            lua_pushnumber(thread, ((double)currtime - event)/10);
            yieldCall(thread, 1);
        } else // go to the next iteration
            ++iter;
    }
}

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
    Event::init(global);
    Player::init(global);
    World::init(global);
    NPC::init(global);

    // add wait()
    lua_register(global, "wait", OF_wait);

    activeScripts = std::map<lua_State*, Script*>();

    REGISTER_SHARD_TIMER(luaScheduler, 200);

    // for each file in the scripts director, load the script
    std::experimental::filesystem::path dir(settings::SCRIPTSDIR);
    for (auto &d : std::experimental::filesystem::directory_iterator(dir)) {
        if (d.path().extension().u8string() == ".lua")
            runScript(d.path().u8string());
    }
}

void LuaManager::runScript(std::string filename) {
    new Script(filename);
}

void LuaManager::stopScripts() {
    // clear the scheduler queue
    scheduleQueue.clear();

    // free all the scripts, they'll take care of everything for us :)
    for (auto as : activeScripts) {
        delete as.second;
    }

    // finally clear the map
    activeScripts.clear();
}

void LuaManager::clearState(lua_State *state) {
    Player::clearState(state);
    World::clearState(state);
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
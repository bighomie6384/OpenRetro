#include "LuaManager.hpp"
#include "LuaWrapper.hpp"

namespace LuaManager {
    namespace Event {
        void init(lua_State *state);

        void push(lua_State *state, lEvent *event);
    }
}
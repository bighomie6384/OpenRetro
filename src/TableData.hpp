#pragma once
#include <map>

#include "JSON.hpp"
#include "NPCManager.hpp"

namespace TableData {
    extern std::map<int32_t, std::vector<WarpLocation>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;
    extern std::map<int32_t, int> RunningNPCMapNumbers;
    extern std::map<int32_t, BaseNPC*> RunningMobs;
    extern std::map<int32_t, BaseNPC*> RunningGroups;
    extern std::map<int32_t, BaseNPC*> RunningEggs;

    void init();
    void flush();
}

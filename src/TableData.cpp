#include "TableData.hpp"
#include "NPCManager.hpp"
#include "Transport.hpp"
#include "Items.hpp"
#include "settings.hpp"
#include "Missions.hpp"
#include "Chunking.hpp"
#include "Nanos.hpp"
#include "Racing.hpp"
#include "Vendor.hpp"
#include "Abilities.hpp"
#include "Eggs.hpp"

#include "JSON.hpp"

#include <fstream>
#include <cmath>

using namespace TableData;

std::map<int32_t, std::vector<WarpLocation>> TableData::RunningSkywayRoutes;
std::map<int32_t, int> TableData::RunningNPCRotations;
std::map<int32_t, int> TableData::RunningNPCMapNumbers;
std::map<int32_t, BaseNPC*> TableData::RunningMobs;
std::map<int32_t, BaseNPC*> TableData::RunningGroups;
std::map<int32_t, BaseNPC*> TableData::RunningEggs;

class TableException : public std::exception {
public:
    std::string msg;

    TableException(std::string m) : std::exception() { msg = m; }

    const char *what() const throw() { return msg.c_str(); }
};

/*
 * Create a full and properly-paced path by interpolating between keyframes.
 */
static void constructPathSkyway(nlohmann::json::iterator _pathData) {
    auto pathData = _pathData.value();
    // Interpolate
    nlohmann::json pathPoints = pathData["points"];
    std::queue<WarpLocation> points;
    nlohmann::json::iterator _point = pathPoints.begin();
    auto point = _point.value();
    WarpLocation last = { point["iX"] , point["iY"] , point["iZ"] }; // start pos
    // use some for loop trickery; start position should not be a point
    for (_point++; _point != pathPoints.end(); _point++) {
        point = _point.value();
        WarpLocation coords = { point["iX"] , point["iY"] , point["iZ"] };
        Transport::lerp(&points, last, coords, pathData["iMonkeySpeed"]);
        points.push(coords); // add keyframe to the queue
        last = coords; // update start pos
    }
    Transport::SkywayPaths[pathData["iRouteID"]] = points;
}

static void constructPathNPC(nlohmann::json::iterator _pathData, int32_t id=0, int offsetX=0, int offsetY=0) {
    auto pathData = _pathData.value();
    // Interpolate
    nlohmann::json pathPoints = pathData["points"];
    std::queue<WarpLocation> points;
    nlohmann::json::iterator _point = pathPoints.begin();
    auto point = _point.value();
    WarpLocation from = { point["iX"] , point["iY"] , point["iZ"] }; // point A coords
    from.x += offsetX;
    from.y += offsetY;
    int stopTime = point["stop"];
    for (_point++; _point != pathPoints.end(); _point++) { // loop through all point Bs
        point = _point.value();
        for(int i = 0; i < stopTime + 1; i++) // repeat point if it's a stop
            points.push(from); // add point A to the queue
        WarpLocation to = { point["iX"] , point["iY"] , point["iZ"] }; // point B coords
        to.x += offsetX;
        to.y += offsetY;
        Transport::lerp(&points, from, to, pathData["iBaseSpeed"]); // lerp from A to B
        from = to; // update point A
        stopTime = point["stop"];
    }

    if (id == 0)
        id = pathData["iNPCID"];

    Transport::NPCQueues[id] = points;
}

/*
 * Load paths from paths JSON.
 */
static void loadPaths(int* nextId) {
    try {
        std::ifstream inFile(settings::PATHJSON);
        nlohmann::json pathData;

        // read file into json
        inFile >> pathData;

        // skyway paths
        nlohmann::json pathDataSkyway = pathData["skyway"];
        for (nlohmann::json::iterator skywayPath = pathDataSkyway.begin(); skywayPath != pathDataSkyway.end(); skywayPath++) {
            constructPathSkyway(skywayPath);
        }
        std::cout << "[INFO] Loaded " << Transport::SkywayPaths.size() << " skyway paths" << std::endl;

        // slider circuit
        nlohmann::json pathDataSlider = pathData["slider"];
        // lerp between keyframes
        std::queue<WarpLocation> route;
        // initial point
        nlohmann::json::iterator _point = pathDataSlider.begin(); // iterator
        auto point = _point.value();
        WarpLocation from = { point["iX"] , point["iY"] , point["iZ"] }; // point A coords
        int stopTime = point["stop"] ? SLIDER_STOP_TICKS : 0; // arbitrary stop length
        // remaining points
        for (_point++; _point != pathDataSlider.end(); _point++) { // loop through all point Bs
            point = _point.value();
            for (int i = 0; i < stopTime + 1; i++) { // repeat point if it's a stop
                route.push(from); // add point A to the queue
            }
            WarpLocation to = { point["iX"] , point["iY"] , point["iZ"] }; // point B coords
            // we may need to change this later; right now, the speed is cut before and after stops (no accel)
            float curve = 1;
            if (stopTime > 0) { // point A is a stop
                curve = 0.375f;//2.0f;
            } else if (point["stop"]) { // point B is a stop
                curve = 0.375f;//0.35f;
            }
            Transport::lerp(&route, from, to, SLIDER_SPEED * curve, 1); // lerp from A to B (arbitrary speed)
            from = to; // update point A
            stopTime = point["stop"] ? SLIDER_STOP_TICKS : 0; // set stop ticks for next point A
        }
        // Uniform distance calculation
        int passedDistance = 0;
        // initial point
        int pos = 0;
        WarpLocation lastPoint = route.front();
        route.pop();
        route.push(lastPoint);
        for (pos = 1; pos < route.size(); pos++) {
            WarpLocation point = route.front();
            passedDistance += hypot(point.x - lastPoint.x, point.y - lastPoint.y);
            if (passedDistance >= SLIDER_GAP_SIZE) { // space them out uniformaly
                passedDistance -= SLIDER_GAP_SIZE; // step down
                // spawn a slider
                BaseNPC* slider = new BaseNPC(point.x, point.y, point.z, 0, INSTANCE_OVERWORLD, 1, (*nextId)++, NPC_BUS);
                NPCManager::NPCs[slider->appearanceData.iNPC_ID] = slider;
                NPCManager::updateNPCPosition(slider->appearanceData.iNPC_ID, slider->appearanceData.iX, slider->appearanceData.iY, slider->appearanceData.iZ, INSTANCE_OVERWORLD, 0);
                Transport::NPCQueues[slider->appearanceData.iNPC_ID] = route;
            }
            // rotate
            route.pop();
            route.push(point);
            lastPoint = point;
        }

        // npc paths
        nlohmann::json pathDataNPC = pathData["npc"];
        /*
        for (nlohmann::json::iterator npcPath = pathDataNPC.begin(); npcPath != pathDataNPC.end(); npcPath++) {
            constructPathNPC(npcPath);
        }
        */

        // mob paths
        pathDataNPC = pathData["mob"];
        for (nlohmann::json::iterator npcPath = pathDataNPC.begin(); npcPath != pathDataNPC.end(); npcPath++) {
            for (auto& pair : MobAI::Mobs) {
                if (pair.second->appearanceData.iNPCType == npcPath.value()["iNPCType"]) {
                    std::cout << "[INFO] Using static path for mob " << pair.second->appearanceData.iNPCType << " with ID " << pair.first << std::endl;

                    auto firstPoint = npcPath.value()["points"][0];
                    if (firstPoint["iX"] != pair.second->spawnX || firstPoint["iY"] != pair.second->spawnY) {
                        std::cout << "[FATAL] The first point of the route for mob " << pair.first <<
                            " (type " << pair.second->appearanceData.iNPCType << ") does not correspond with its spawn point." << std::endl;
                        exit(1);
                    }

                    constructPathNPC(npcPath, pair.first);
                    pair.second->staticPath = true;
                    for (int i = 0; i < 4; i++) {
                        int groupMember = pair.second->groupMember[i];
                        if (groupMember != 0) {
                            constructPathNPC(npcPath, groupMember, MobAI::Mobs[groupMember]->offsetX, MobAI::Mobs[groupMember]->offsetY);
                            MobAI::Mobs[groupMember]->staticPath = true;
                        }
                    }
                    break; // only one NPC per path
                }
            }
        }
        std::cout << "[INFO] Loaded " << Transport::NPCQueues.size() << " NPC paths" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed paths.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load drops data from JSON.
 * This has to be called after reading xdt because it reffers to ItemData!!!
 */
static void loadDrops() {
    try {
        std::ifstream inFile(settings::DROPSJSON);
        nlohmann::json dropData;

        // read file into json
        inFile >> dropData;

        // MobDropChances
        nlohmann::json mobDropChances = dropData["MobDropChances"];
        for (nlohmann::json::iterator _dropChance = mobDropChances.begin(); _dropChance != mobDropChances.end(); _dropChance++) {
            auto dropChance = _dropChance.value();
            MobDropChance toAdd = {};
            toAdd.dropChance = (int)dropChance["DropChance"];
            for (nlohmann::json::iterator _cratesRatio = dropChance["CratesRatio"].begin(); _cratesRatio != dropChance["CratesRatio"].end(); _cratesRatio++) {
                toAdd.cratesRatio.push_back((int)_cratesRatio.value());
            }
            Items::MobDropChances[(int)dropChance["Type"]] = toAdd;
        }

        // MobDrops
        nlohmann::json mobDrops = dropData["MobDrops"];
        for (nlohmann::json::iterator _drop = mobDrops.begin(); _drop != mobDrops.end(); _drop++) {
            auto drop = _drop.value();
            MobDrop toAdd = {};
            for (nlohmann::json::iterator _crates = drop["CrateIDs"].begin(); _crates != drop["CrateIDs"].end(); _crates++) {
                toAdd.crateIDs.push_back((int)_crates.value());
            }

            toAdd.dropChanceType = (int)drop["DropChance"];
            // Check if DropChance exists
            if (Items::MobDropChances.find(toAdd.dropChanceType) == Items::MobDropChances.end()) {
                throw TableException(" MobDropChance not found: " + std::to_string((toAdd.dropChanceType)));
            }
            // Check if number of crates is correct
            if (!(Items::MobDropChances[(int)drop["DropChance"]].cratesRatio.size() == toAdd.crateIDs.size())) {
                throw TableException(" DropType " + std::to_string((int)drop["DropType"]) + " contains invalid number of crates");
            }

            toAdd.taros = (int)drop["Taros"];
            toAdd.fm = (int)drop["FM"];
            toAdd.boosts = (int)drop["Boosts"];
            Items::MobDrops[(int)drop["DropType"]] = toAdd;
        }

        std::cout << "[INFO] Loaded " << Items::MobDrops.size() << " Mob Drop Types"<<  std::endl;

        // Rarity Ratios
        nlohmann::json rarities = dropData["RarityRatios"];
        for (nlohmann::json::iterator _rarity = rarities.begin(); _rarity != rarities.end(); _rarity++) {
            auto rarity = _rarity.value();
            std::vector<int> toAdd;
            for (nlohmann::json::iterator _ratio = rarity["Ratio"].begin(); _ratio != rarity["Ratio"].end(); _ratio++){
                toAdd.push_back((int)_ratio.value());
            }
            Items::RarityRatios[(int)rarity["Type"]] = toAdd;
        }

        // Crates
        nlohmann::json crates = dropData["Crates"];
        for (nlohmann::json::iterator _crate = crates.begin(); _crate != crates.end(); _crate++) {
            auto crate = _crate.value();
            Crate toAdd;
            toAdd.rarityRatioId = (int)crate["RarityRatio"];
            for (nlohmann::json::iterator _itemSet = crate["ItemSets"].begin(); _itemSet != crate["ItemSets"].end(); _itemSet++) {
                toAdd.itemSets.push_back((int)_itemSet.value());
            }
            Items::Crates[(int)crate["Id"]] = toAdd;
        }

        // Crate Items
        nlohmann::json items = dropData["Items"];
        int itemCount = 0;
        for (nlohmann::json::iterator _item = items.begin(); _item != items.end(); _item++) {
            auto item = _item.value();
            std::pair<int32_t, int32_t> itemSetkey = std::make_pair((int)item["ItemSet"], (int)item["Rarity"]);
            std::pair<int32_t, int32_t> itemDataKey = std::make_pair((int)item["Id"], (int)item["Type"]);

            if (Items::ItemData.find(itemDataKey) == Items::ItemData.end()) {
                char buff[255];
                sprintf(buff, "Unknown item with Id %d and Type %d", (int)item["Id"], (int)item["Type"]);
                throw TableException(std::string(buff));
            }

            std::map<std::pair<int32_t, int32_t>, Items::Item>::iterator toAdd = Items::ItemData.find(itemDataKey);

            // if item collection doesn't exist, start a new one
            if (Items::CrateItems.find(itemSetkey) == Items::CrateItems.end()) {
                std::vector<std::map<std::pair<int32_t, int32_t>, Items::Item>::iterator> vector;
                vector.push_back(toAdd);
                Items::CrateItems[itemSetkey] = vector;
            } else // else add a new element to existing collection
                Items::CrateItems[itemSetkey].push_back(toAdd);

            itemCount++;
        }

#ifdef ACADEMY
        nlohmann::json capsules = dropData["NanoCapsules"];

        for (nlohmann::json::iterator _capsule = capsules.begin(); _capsule != capsules.end(); _capsule++) {
            auto capsule = _capsule.value();
            Items::NanoCapsules[(int)capsule["Crate"]] = (int)capsule["Nano"];
        }
#endif
        nlohmann::json codes = dropData["CodeItems"];
        for (nlohmann::json::iterator _code = codes.begin(); _code != codes.end(); _code++) {
            auto code = _code.value();
            std::string codeStr = code["Code"];
            std::pair<int32_t, int32_t> item = std::make_pair((int)code["Id"], (int)code["Type"]);

            if (Items::CodeItems.find(codeStr) == Items::CodeItems.end())
                Items::CodeItems[codeStr] = std::vector<std::pair<int32_t, int32_t>>();
            Items::CodeItems[codeStr].push_back(item);
        }

        std::cout << "[INFO] Loaded " << Items::Crates.size() << " Crates containing "
                  << itemCount << " items" << std::endl;

        // Racing rewards
        nlohmann::json racing = dropData["Racing"];
        for (nlohmann::json::iterator _race = racing.begin(); _race != racing.end(); _race++) {
            auto race = _race.value();
            int raceEPID = race["EPID"];

            // find the instance data corresponding to the EPID
            int EPMap = -1;
            for (auto it = Racing::EPData.begin(); it != Racing::EPData.end(); it++) {
                if (it->second.EPID == raceEPID) {
                    EPMap = it->first;
                }
            }

            if (EPMap == -1) { // not found
                std::cout << "[WARN] EP with ID " << raceEPID << " not found, skipping" << std::endl;
                continue;
            }

            // time limit isn't stored in the XDT, so we include it in the reward table instead
            Racing::EPData[EPMap].maxTime = race["TimeLimit"];

            // score cutoffs
            std::vector<int> rankScores;
            for (nlohmann::json::iterator _rankScore = race["RankScores"].begin(); _rankScore != race["RankScores"].end(); _rankScore++) {
                rankScores.push_back((int)_rankScore.value());
            }

            // reward IDs for each rank
            std::vector<int> rankRewards;
            for (nlohmann::json::iterator _rankReward = race["Rewards"].begin(); _rankReward != race["Rewards"].end(); _rankReward++) {
                rankRewards.push_back((int)_rankReward.value());
            }

            if (rankScores.size() != 5 || rankScores.size() != rankRewards.size()) {
                char buff[255];
                sprintf(buff, "Race in EP %d doesn't have exactly 5 score/reward pairs", raceEPID);
                throw TableException(std::string(buff));
            }

            Racing::EPRewards[raceEPID] = std::make_pair(rankScores, rankRewards);
        }

        std::cout << "[INFO] Loaded rewards for " << Racing::EPRewards.size() << " IZ races" << std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed drops.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

static void loadEggs(int32_t* nextId) {
    try {
        std::ifstream inFile(settings::EGGSJSON);
        nlohmann::json eggData;

        // read file into json
        inFile >> eggData;

        // EggTypes
        nlohmann::json eggTypes = eggData["EggTypes"];
        for (nlohmann::json::iterator _eggType = eggTypes.begin(); _eggType != eggTypes.end(); _eggType++) {
            auto eggType = _eggType.value();
            EggType toAdd = {};
            toAdd.dropCrateId = (int)eggType["DropCrateId"];
            toAdd.effectId = (int)eggType["EffectId"];
            toAdd.duration = (int)eggType["Duration"];
            toAdd.regen= (int)eggType["Regen"];
            Eggs::EggTypes[(int)eggType["Id"]] = toAdd;
        }

        // Egg instances
        auto eggs = eggData["Eggs"];
        for (auto _egg = eggs.begin(); _egg != eggs.end(); _egg++) {
            auto egg = _egg.value();
            int id = (*nextId)++;
            uint64_t instanceID = egg.find("iMapNum") == egg.end() ? INSTANCE_OVERWORLD : (int)egg["iMapNum"];

            Egg* addEgg = new Egg((int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, (int)egg["iType"], id, false);
            NPCManager::NPCs[id] = addEgg;
            Eggs::Eggs[id] = addEgg;
            NPCManager::updateNPCPosition(id, (int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, 0);
        }

        std::cout << "[INFO] Loaded " <<Eggs::Eggs.size()<<" eggs" <<std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed eggs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

// load gruntwork output; if it exists
static void loadGruntwork(int32_t *nextId) {
    try {
        std::ifstream inFile(settings::GRUNTWORKJSON);
        nlohmann::json gruntwork;

        // skip if there's no gruntwork to load
        if (inFile.fail())
            return;

        inFile >> gruntwork;

        // skyway paths
        auto skyway = gruntwork["skyway"];
        for (auto _route = skyway.begin(); _route != skyway.end(); _route++) {
            auto route = _route.value();
            std::vector<WarpLocation> points;

            for (auto _point = route["points"].begin(); _point != route["points"].end(); _point++) {
                auto point = _point.value();
                points.push_back(WarpLocation{point["x"], point["y"], point["z"]});
            }

            RunningSkywayRoutes[(int)route["iRouteID"]] = points;
        }

        // npc rotations
        auto npcRot = gruntwork["rotations"];
        for (auto _rot = npcRot.begin(); _rot != npcRot.end(); _rot++) {
            int32_t npcID = _rot.value()["iNPCID"];
            int angle = _rot.value()["iAngle"];
            if (NPCManager::NPCs.find(npcID) == NPCManager::NPCs.end())
                continue; // NPC not found
            BaseNPC* npc = NPCManager::NPCs[npcID];
            npc->appearanceData.iAngle = angle;

            RunningNPCRotations[npcID] = angle;
        }

        // npc map numbers
        auto npcMap = gruntwork["instances"];
        for (auto _map = npcMap.begin(); _map != npcMap.end(); _map++) {
            int32_t npcID = _map.value()["iNPCID"];
            uint64_t instanceID = _map.value()["iMapNum"];
            if (NPCManager::NPCs.find(npcID) == NPCManager::NPCs.end())
                continue; // NPC not found
            BaseNPC* npc = NPCManager::NPCs[npcID];
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->appearanceData.iX, npc->appearanceData.iY,
                npc->appearanceData.iZ, instanceID, npc->appearanceData.iAngle);

            RunningNPCMapNumbers[npcID] = instanceID;
        }

        // mobs
        auto mobs = gruntwork["mobs"];
        for (auto _mob = mobs.begin(); _mob != mobs.end(); _mob++) {
            auto mob = _mob.value();
            BaseNPC *npc;
            int id = (*nextId)++;
            uint64_t instanceID = mob.find("iMapNum") == mob.end() ? INSTANCE_OVERWORLD : (int)mob["iMapNum"];

            if (NPCManager::NPCData[(int)mob["iNPCType"]]["m_iTeam"] == 2) {
                npc = new Mob(mob["iX"], mob["iY"], mob["iZ"], instanceID, mob["iNPCType"],
                    NPCManager::NPCData[(int)mob["iNPCType"]], id);

                // re-enable respawning
                ((Mob*)npc)->summoned = false;

                MobAI::Mobs[npc->appearanceData.iNPC_ID] = (Mob*)npc;
            } else {
                npc = new BaseNPC(mob["iX"], mob["iY"], mob["iZ"], mob["iAngle"], instanceID, mob["iNPCType"], id);
            }

            NPCManager::NPCs[npc->appearanceData.iNPC_ID] = npc;
            RunningMobs[npc->appearanceData.iNPC_ID] = npc;
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, mob["iX"], mob["iY"], mob["iZ"], instanceID, mob["iAngle"]);
        }

        // mob groups
        auto groups = gruntwork["groups"];
        for (auto _group = groups.begin(); _group != groups.end(); _group++) {
            auto leader = _group.value();
            auto td = NPCManager::NPCData[(int)leader["iNPCType"]];
            uint64_t instanceID = leader.find("iMapNum") == leader.end() ? INSTANCE_OVERWORLD : (int)leader["iMapNum"];

            Mob* tmp = new Mob(leader["iX"], leader["iY"], leader["iZ"], leader["iAngle"], instanceID, leader["iNPCType"], td, *nextId);

            // re-enable respawning
            ((Mob*)tmp)->summoned = false;

            NPCManager::NPCs[*nextId] = tmp;
            MobAI::Mobs[*nextId] = (Mob*)NPCManager::NPCs[*nextId];
            NPCManager::updateNPCPosition(*nextId, leader["iX"], leader["iY"], leader["iZ"], instanceID, leader["iAngle"]);

            tmp->groupLeader = *nextId;

            (*nextId)++;

            auto followers = leader["aFollowers"];
            if (followers.size() < 5) {
                int followerCount = 0;
                for (nlohmann::json::iterator _fol = followers.begin(); _fol != followers.end(); _fol++) {
                    auto follower = _fol.value();
                    auto tdFol = NPCManager::NPCData[(int)follower["iNPCType"]];
                    Mob* tmpFol = new Mob((int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], leader["iAngle"], instanceID, follower["iNPCType"], tdFol, *nextId);

                    // re-enable respawning
                    ((Mob*)tmp)->summoned = false;

                    NPCManager::NPCs[*nextId] = tmpFol;
                    MobAI::Mobs[*nextId] = (Mob*)NPCManager::NPCs[*nextId];
                    NPCManager::updateNPCPosition(*nextId, (int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], instanceID, leader["iAngle"]);

                    tmpFol->offsetX = follower.find("iOffsetX") == follower.end() ? 0 : (int)follower["iOffsetX"];
                    tmpFol->offsetY = follower.find("iOffsetY") == follower.end() ? 0 : (int)follower["iOffsetY"];
                    tmpFol->groupLeader = tmp->appearanceData.iNPC_ID;
                    tmp->groupMember[followerCount++] = *nextId;

                    (*nextId)++;
                }
            }
            else {
                std::cout << "[WARN] Mob group leader with ID " << *nextId << " has too many followers (" << followers.size() << ")\n";
            }

            RunningGroups[tmp->appearanceData.iNPC_ID] = tmp; // store as running
        }

        auto eggs = gruntwork["eggs"];
        for (auto _egg = eggs.begin(); _egg != eggs.end(); _egg++) {
            auto egg = _egg.value();
            int id = (*nextId)++;
            uint64_t instanceID = egg.find("iMapNum") == egg.end() ? INSTANCE_OVERWORLD : (int)egg["iMapNum"];

            Egg* addEgg = new Egg((int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, (int)egg["iType"], id, false);
            NPCManager::NPCs[id] = addEgg;
            Eggs::Eggs[id] = addEgg;
            NPCManager::updateNPCPosition(id, (int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, 0);
            RunningEggs[id] = addEgg;
        }


        std::cout << "[INFO] Loaded gruntwork.json" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed gruntwork.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

void TableData::init() {
    int32_t nextId = 0;

    // load NPCs from NPC.json
    try {
        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;
        npcData = npcData["NPCs"];
        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            int instanceID = npc.find("mapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["mapNum"];
#ifdef ACADEMY
            // do not spawn NPCs in the future
            if (npc["x"] > 512000 && npc["y"] < 256000) {
                nextId++;
                continue;
            }
#endif
            BaseNPC *tmp = new BaseNPC(npc["x"], npc["y"], npc["z"], npc["angle"], instanceID, npc["id"], nextId);

            NPCManager::NPCs[nextId] = tmp;
            NPCManager::updateNPCPosition(nextId, npc["x"], npc["y"], npc["z"], instanceID, npc["angle"]);
            nextId++;

            if (npc["id"] == 641 || npc["id"] == 642)
                NPCManager::RespawnPoints.push_back({ npc["x"], npc["y"], ((int)npc["z"]) + RESURRECT_HEIGHT, instanceID });
        }
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

    // load everything else from xdttable
    std::cout << "[INFO] Parsing xdt.json..." << std::endl;
    std::ifstream infile(settings::XDTJSON);
    nlohmann::json xdtData;

    // read file into json
    infile >> xdtData;

    // data we'll need for summoned mobs
    NPCManager::NPCData = xdtData["m_pNpcTable"]["m_pNpcData"];

    try {
        // load warps
        nlohmann::json warpData = xdtData["m_pInstanceTable"]["m_pWarpData"];

        for (nlohmann::json::iterator _warp = warpData.begin(); _warp != warpData.end(); _warp++) {
            auto warp = _warp.value();
            WarpLocation warpLoc = { warp["m_iToX"], warp["m_iToY"], warp["m_iToZ"], warp["m_iToMapNum"], warp["m_iIsInstance"], warp["m_iLimit_TaskID"], warp["m_iNpcNumber"] };
            if (warp["m_iNpcNumber"] == 3339) {
                warpLoc.x = 630500;
                warpLoc.y = 809000;
                warpLoc.z = 4000;
            } else if (warp["m_iNpcNumber"] == 3345) {
                warpLoc.x = 126872;
                warpLoc.y = 698884;
                warpLoc.z = -4200;
            } else if (warp["m_iNpcNumber"] == 661) {
                warpLoc.x = 346416;
                warpLoc.y = 552433;
                warpLoc.z = -4586;
            }
            
            int warpID = warp["m_iWarpNumber"];
            NPCManager::Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] Populated " << NPCManager::Warps.size() << " Warps" << std::endl;

        // load transport routes and locations
        nlohmann::json transRouteData = xdtData["m_pTransportationTable"]["m_pTransportationData"];
        nlohmann::json transLocData = xdtData["m_pTransportationTable"]["m_pTransportationWarpLocation"];

        for (nlohmann::json::iterator _tLoc = transLocData.begin(); _tLoc != transLocData.end(); _tLoc++) {
            auto tLoc = _tLoc.value();
            TransportLocation transLoc = { tLoc["m_iNPCID"], tLoc["m_iXpos"], tLoc["m_iYpos"], tLoc["m_iZpos"] };
            if (tLoc["m_iNPCID"] == 974) {
                transLoc.x = 213100;
                transLoc.y = 107100;
                transLoc.z = -5695;
            }
            Transport::Locations[tLoc["m_iLocationID"]] = transLoc;
        }
        std::cout << "[INFO] Loaded " << Transport::Locations.size() << " S.C.A.M.P.E.R. locations" << std::endl;

        for (nlohmann::json::iterator _tRoute = transRouteData.begin(); _tRoute != transRouteData.end(); _tRoute++) {
            auto tRoute = _tRoute.value();
            TransportRoute transRoute = { tRoute["m_iMoveType"], tRoute["m_iStartLocation"], tRoute["m_iEndLocation"],
                tRoute["m_iCost"] , tRoute["m_iSpeed"], tRoute["m_iRouteNum"] };
            Transport::Routes[tRoute["m_iVehicleID"]] = transRoute;
        }
        std::cout << "[INFO] Loaded " << Transport::Routes.size() << " transportation routes" << std::endl;

        // load mission-related data
        nlohmann::json tasks = xdtData["m_pMissionTable"]["m_pMissionData"];

        for (auto _task = tasks.begin(); _task != tasks.end(); _task++) {
            auto task = _task.value();

            // rewards
            if (task["m_iSUReward"] != 0) {
                auto _rew = xdtData["m_pMissionTable"]["m_pRewardData"][(int)task["m_iSUReward"]];
                Reward *rew = new Reward(_rew["m_iMissionRewardID"], _rew["m_iMissionRewarItemType"],
                        _rew["m_iMissionRewardItemID"], _rew["m_iCash"], _rew["m_iFusionMatter"]);

                Missions::Rewards[task["m_iHTaskID"]] = rew;
            }

            // everything else lol. see TaskData comment.
            Missions::Tasks[task["m_iHTaskID"]] = new TaskData(task);
        }
        std::cout << "[INFO] Loaded " << Transport::Locations.size() << " S.C.A.M.P.E.R. locations" << std::endl;

        std::cout << "[INFO] Loaded mission-related data" << std::endl;

        /*
        * load all equipment data. i'm sorry. it has to be done
        * NOTE: please don't change the ordering. it determines the types, since type and equipLoc are used inconsistently
        */
        const char* setNames[11] = { "m_pWeaponItemTable", "m_pShirtsItemTable", "m_pPantsItemTable", "m_pShoesItemTable",
        "m_pHatItemTable", "m_pGlassItemTable", "m_pBackItemTable", "m_pGeneralItemTable", "",
        "m_pChestItemTable", "m_pVehicleItemTable" };
        nlohmann::json itemSet;
        for (int i = 0; i < 11; i++) {
            if (i == 8)
                continue; // there is no type 8, of course

            itemSet = xdtData[setNames[i]]["m_pItemData"];
            for (nlohmann::json::iterator _item = itemSet.begin(); _item != itemSet.end(); _item++) {
                auto item = _item.value();
                int itemID = item["m_iItemNumber"];
                INITSTRUCT(Items::Item, itemData);
                itemData.tradeable = item["m_iTradeAble"] == 1;
                itemData.sellable = item["m_iSellAble"] == 1;
                itemData.buyPrice = item["m_iItemPrice"];
                itemData.sellPrice = item["m_iItemSellPrice"];
                itemData.stackSize = item["m_iStackNumber"];
                if (i != 7 && i != 9) {
                    itemData.rarity = item["m_iRarity"];
                    itemData.level = item["m_iMinReqLev"];
                    itemData.pointDamage = item["m_iPointRat"];
                    itemData.groupDamage = item["m_iGroupRat"];
                    itemData.fireRate = item["m_iDelayTime"];
                    itemData.defense = item["m_iDefenseRat"];
                    itemData.gender = item["m_iReqSex"];
                    itemData.weaponType = item["m_iEquipType"];
                } else {
                    itemData.rarity = 1;
                }
                Items::ItemData[std::make_pair(itemID, i)] = itemData;
            }
        }

        std::cout << "[INFO] Loaded " << Items::ItemData.size() << " items" << std::endl;

        // load player limits from m_pAvatarTable.m_pAvatarGrowData

        nlohmann::json growth = xdtData["m_pAvatarTable"]["m_pAvatarGrowData"];

        for (int i = 0; i < 37; i++) {
            Missions::AvatarGrowth[i] = growth[i];
        }

        // load vendor listings
        nlohmann::json listings = xdtData["m_pVendorTable"]["m_pItemData"];

        for (nlohmann::json::iterator _lst = listings.begin(); _lst != listings.end(); _lst++) {
            auto lst = _lst.value();
            VendorListing vListing = { lst["m_iSortNumber"], lst["m_iItemType"], lst["m_iitemID"] };
            Vendor::VendorTables[lst["m_iNpcNumber"]].push_back(vListing);
        }

        std::cout << "[INFO] Loaded " << Vendor::VendorTables.size() << " vendor tables" << std::endl;

        // load crocpot entries
        nlohmann::json crocs = xdtData["m_pCombiningTable"]["m_pCombiningData"];

        for (nlohmann::json::iterator croc = crocs.begin(); croc != crocs.end(); croc++) {
            CrocPotEntry crocEntry = { croc.value()["m_iStatConstant"], croc.value()["m_iLookConstant"], croc.value()["m_fLevelGapStandard"],
                croc.value()["m_fSameGrade"], croc.value()["m_fOneGrade"], croc.value()["m_fTwoGrade"], croc.value()["m_fThreeGrade"] };
            Items::CrocPotTable[croc.value()["m_iLevelGap"]] = crocEntry;
        }

        std::cout << "[INFO] Loaded " << Items::CrocPotTable.size() << " croc pot value sets" << std::endl;

        // load nano info
        nlohmann::json nanoInfo = xdtData["m_pNanoTable"]["m_pNanoData"];
        for (nlohmann::json::iterator _nano = nanoInfo.begin(); _nano != nanoInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoData nanoData;
            nanoData.style = nano["m_iStyle"];
            Nanos::NanoTable[Nanos::NanoTable.size()] = nanoData;
        }

        std::cout << "[INFO] Loaded " << Nanos::NanoTable.size() << " nanos" << std::endl;

        nlohmann::json nanoTuneInfo = xdtData["m_pNanoTable"]["m_pNanoTuneData"];
        for (nlohmann::json::iterator _nano = nanoTuneInfo.begin(); _nano != nanoTuneInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoTuning nanoData;
            nanoData.reqItems = nano["m_iReqItemID"];
            nanoData.reqItemCount = nano["m_iReqItemCount"];
            Nanos::NanoTunings[nano["m_iSkillID"]] = nanoData;
        }

        std::cout << "[INFO] Loaded " << Nanos::NanoTable.size() << " nano tunings" << std::endl;

        // load nano powers
        nlohmann::json skills = xdtData["m_pSkillTable"]["m_pSkillData"];

        for (nlohmann::json::iterator _skills = skills.begin(); _skills != skills.end(); _skills++) {
            auto skills = _skills.value();
            SkillData skillData = {skills["m_iSkillType"], skills["m_iTargetType"], skills["m_iBatteryDrainType"], skills["m_iEffectArea"]};
            for (int i = 0; i < 4; i++) {
                skillData.batteryUse[i] = skills["m_iBatteryDrainUse"][i];
                skillData.durationTime[i] = skills["m_iDurationTime"][i];
                skillData.powerIntensity[i] = skills["m_iValueA"][i];
            }
            Nanos::SkillTable[skills["m_iSkillNumber"]] = skillData;
        }

        std::cout << "[INFO] Loaded " << Nanos::SkillTable.size() << " nano skills" << std::endl;

        // load EP data
        nlohmann::json instances = xdtData["m_pInstanceTable"]["m_pInstanceData"];

        for (nlohmann::json::iterator _instance = instances.begin(); _instance != instances.end(); _instance++) {
            auto instance = _instance.value();
            EPInfo epInfo = {instance["m_iZoneX"], instance["m_iZoneY"], instance["m_iIsEP"], (int)instance["m_ScoreMax"]};
            Racing::EPData[instance["m_iInstanceNameID"]] = epInfo;
        }

        std::cout << "[INFO] Loaded " << Racing::EPData.size() << " instances" << std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed xdt.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

    // load mobs
    try {
        std::ifstream inFile(settings::MOBJSON);
        nlohmann::json npcData, groupData;

        // read file into json
        inFile >> npcData;
        groupData = npcData["groups"];
        npcData = npcData["mobs"];

        // single mobs
        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            auto td = NPCManager::NPCData[(int)npc["iNPCType"]];
            uint64_t instanceID = npc.find("iMapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["iMapNum"];

#ifdef ACADEMY
            // do not spawn NPCs in the future
            if (npc["iX"] > 512000 && npc["iY"] < 256000) {
                nextId++;
                continue;
            }
#endif

            Mob *tmp = new Mob(npc["iX"], npc["iY"], npc["iZ"], npc["iAngle"], instanceID, npc["iNPCType"], td, nextId);

            NPCManager::NPCs[nextId] = tmp;
            MobAI::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
            NPCManager::updateNPCPosition(nextId, npc["iX"], npc["iY"], npc["iZ"], instanceID, npc["iAngle"]);

            nextId++;
        }

        // mob groups
        // single mobs
        for (nlohmann::json::iterator _group = groupData.begin(); _group != groupData.end(); _group++) {
            auto leader = _group.value();
            auto td = NPCManager::NPCData[(int)leader["iNPCType"]];
            uint64_t instanceID = leader.find("iMapNum") == leader.end() ? INSTANCE_OVERWORLD : (int)leader["iMapNum"];
            auto followers = leader["aFollowers"];

#ifdef ACADEMY
            // do not spawn NPCs in the future
            if (leader["iX"] > 512000 && leader["iY"] < 256000) {
                nextId++;
                nextId += followers.size();
                continue;
            }
#endif

            Mob* tmp = new Mob(leader["iX"], leader["iY"], leader["iZ"], leader["iAngle"], instanceID, leader["iNPCType"], td, nextId);

            NPCManager::NPCs[nextId] = tmp;
            MobAI::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
            NPCManager::updateNPCPosition(nextId, leader["iX"], leader["iY"], leader["iZ"], instanceID, leader["iAngle"]);

            tmp->groupLeader = nextId;

            nextId++;

            if (followers.size() < 5) {
                int followerCount = 0;
                for (nlohmann::json::iterator _fol = followers.begin(); _fol != followers.end(); _fol++) {
                    auto follower = _fol.value();
                    auto tdFol = NPCManager::NPCData[(int)follower["iNPCType"]];
                    Mob* tmpFol = new Mob((int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], leader["iAngle"], instanceID, follower["iNPCType"], tdFol, nextId);

                    NPCManager::NPCs[nextId] = tmpFol;
                    MobAI::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
                    NPCManager::updateNPCPosition(nextId, (int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], instanceID, leader["iAngle"]);

                    tmpFol->offsetX = follower.find("iOffsetX") == follower.end() ? 0 : (int)follower["iOffsetX"];
                    tmpFol->offsetY = follower.find("iOffsetY") == follower.end() ? 0 : (int)follower["iOffsetY"];
                    tmpFol->groupLeader = tmp->appearanceData.iNPC_ID;
                    tmp->groupMember[followerCount++] = nextId;

                    nextId++;
                }
            } else {
                std::cout << "[WARN] Mob group leader with ID " << nextId << " has too many followers (" << followers.size() << ")\n";
            }
        }

        std::cout << "[INFO] Populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed mobs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

    try {
        std::ifstream inFile(settings::VENDORJSON);
        nlohmann::json vendorData;

        inFile >> vendorData;

        nlohmann::json listings = vendorData["m_pItemData"];

        for (nlohmann::json::iterator _lst = listings.begin(); _lst != listings.end(); _lst++) {
            auto lst = _lst.value();
            VendorListing vListing = { lst["m_iSortNumber"], lst["m_iItemType"], lst["m_iitemID"] };
            Vendor::VendorOverrideTables[lst["m_iNpcNumber"]].push_back(vListing);
        }

        std::cout << "[INFO] Loaded " << Vendor::VendorOverrideTables.size() << " vendor override tables" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed vendor.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

#ifdef ACADEMY
    // load Academy NPCs from academy.json
    try {
        std::ifstream inFile(settings::ACADEMYJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;
        npcData = npcData["NPCs"];
        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            int instanceID = npc.find("iMapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["iMapNum"];

            int team = NPCManager::NPCData[(int)npc["iNPCType"]]["m_iTeam"];

            if (team == 2) {
                NPCManager::NPCs[nextId] = new Mob(npc["iX"], npc["iY"], npc["iZ"], npc["iAngle"], instanceID, npc["iNPCType"], NPCManager::NPCData[(int)npc["iNPCType"]], nextId);
                MobAI::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
            } else
                NPCManager::NPCs[nextId] = new BaseNPC(npc["iX"], npc["iY"], npc["iZ"], npc["iAngle"], instanceID, npc["iNPCType"], nextId);

            NPCManager::updateNPCPosition(nextId, npc["iX"], npc["iY"], npc["iZ"], instanceID, npc["iAngle"]);
            nextId++;

            if (npc["iNPCType"] == 641 || npc["iNPCType"] == 642)
                NPCManager::RespawnPoints.push_back({ npc["iX"], npc["iY"], ((int)npc["iZ"]) + RESURRECT_HEIGHT, instanceID });
        }
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed academy.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
#endif

    loadDrops();

    loadEggs(&nextId);

    loadPaths(&nextId); // load paths

    loadGruntwork(&nextId);

    NPCManager::nextId = nextId;
}

// write gruntwork output to file
void TableData::flush() {
    std::ofstream file(settings::GRUNTWORKJSON);
    nlohmann::json gruntwork;

    for (auto& pair : RunningSkywayRoutes) {
        nlohmann::json route;

        route["iRouteID"] = (int)pair.first;
        route["iMonkeySpeed"] = 1500;

        std::cout << "serializing mss route " << (int)pair.first << std::endl;
        for (WarpLocation& point : pair.second) {
            nlohmann::json tmp;

            tmp["x"] = point.x;
            tmp["y"] = point.y;
            tmp["z"] = point.z;

            route["points"].push_back(tmp);
        }

        gruntwork["skyway"].push_back(route);
    }

    for (auto& pair : RunningNPCRotations) {
        nlohmann::json rotation;

        rotation["iNPCID"] = (int)pair.first;
        rotation["iAngle"] = pair.second;

        gruntwork["rotations"].push_back(rotation);
    }

    for (auto& pair : RunningNPCMapNumbers) {
        nlohmann::json mapNumber;

        mapNumber["iNPCID"] = (int)pair.first;
        mapNumber["iMapNum"] = pair.second;

        gruntwork["instances"].push_back(mapNumber);
    }

    for (auto& pair : RunningMobs) {
        nlohmann::json mob;
        BaseNPC *npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;

        int x, y, z;
        if (npc->npcClass == NPC_MOB) {
            Mob *m = (Mob*)npc;
            x = m->spawnX;
            y = m->spawnY;
            z = m->spawnZ;
        } else {
            x = npc->appearanceData.iX;
            y = npc->appearanceData.iY;
            z = npc->appearanceData.iZ;
        }

        // NOTE: this format deviates slightly from the one in mobs.json
        mob["iNPCType"] = (int)npc->appearanceData.iNPCType;
        mob["iX"] = x;
        mob["iY"] = y;
        mob["iZ"] = z;
        mob["iMapNum"] = MAPNUM(npc->instanceID);
        // this is a bit imperfect, since this is a live angle, not a spawn angle so it'll change often, but eh
        mob["iAngle"] = npc->appearanceData.iAngle;

        // it's called mobs, but really it's everything
        gruntwork["mobs"].push_back(mob);
    }

    for (auto& pair : RunningGroups) {
        nlohmann::json mob;
        BaseNPC* npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;

        int x, y, z;
        std::vector<Mob*> followers;
        if (npc->npcClass == NPC_MOB) {
            Mob* m = (Mob*)npc;
            x = m->spawnX;
            y = m->spawnY;
            z = m->spawnZ;
            if (m->groupLeader != m->appearanceData.iNPC_ID) { // make sure this is a leader
                std::cout << "[WARN] Non-leader mob found in running groups; ignoring\n";
                continue;
            }

            // add follower data to vector; go until OOB or until follower ID is 0
            for (int i = 0; i < 4 && m->groupMember[i] > 0; i++) {
                if (MobAI::Mobs.find(m->groupMember[i]) == MobAI::Mobs.end()) {
                    std::cout << "[WARN] Follower with ID " << m->groupMember[i] << " not found; skipping\n";
                    continue;
                }
                followers.push_back(MobAI::Mobs[m->groupMember[i]]);
            }
        }
        else {
            x = npc->appearanceData.iX;
            y = npc->appearanceData.iY;
            z = npc->appearanceData.iZ;
        }

        // NOTE: this format deviates slightly from the one in mobs.json
        mob["iNPCType"] = (int)npc->appearanceData.iNPCType;
        mob["iX"] = x;
        mob["iY"] = y;
        mob["iZ"] = z;
        mob["iMapNum"] = MAPNUM(npc->instanceID);
        // this is a bit imperfect, since this is a live angle, not a spawn angle so it'll change often, but eh
        mob["iAngle"] = npc->appearanceData.iAngle;

        // followers
        while (followers.size() > 0) {
            Mob* follower = followers.back();
            followers.pop_back(); // remove from vector

            // populate JSON entry
            nlohmann::json fol;
            fol["iNPCType"] = follower->appearanceData.iNPCType;
            fol["iOffsetX"] = follower->offsetX;
            fol["iOffsetY"] = follower->offsetY;

            mob["aFollowers"].push_back(fol); // add to follower array
        }

        // it's called mobs, but really it's everything
        gruntwork["groups"].push_back(mob);
    }

    for (auto& pair : RunningEggs) {
        nlohmann::json egg;
        BaseNPC* npc = pair.second;

        if (Eggs::Eggs.find(pair.first) == Eggs::Eggs.end())
            continue;
        egg["iX"] = npc->appearanceData.iX;
        egg["iY"] = npc->appearanceData.iY;
        egg["iZ"] = npc->appearanceData.iZ;
        int mapnum = MAPNUM(npc->instanceID);
        if (mapnum != 0)
            egg["iMapNum"] = mapnum;
        egg["iType"] = npc->appearanceData.iNPCType;

        gruntwork["eggs"].push_back(egg);
    }

    file << gruntwork << std::endl;
}

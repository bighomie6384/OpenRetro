#include "servers/CNShardServer.hpp"
#include "Items.hpp"
#include "PlayerManager.hpp"
#include "Nanos.hpp"
#include "NPCManager.hpp"
#include "Player.hpp"
#include "Abilities.hpp"
#include "Missions.hpp"
#include "Eggs.hpp"

#include <string.h> // for memset()
#include <assert.h>

using namespace Items;

std::map<std::pair<int32_t, int32_t>, Items::Item> Items::ItemData;
std::map<int32_t, CrocPotEntry> Items::CrocPotTable;
std::map<int32_t, std::vector<int>> Items::RarityRatios;
std::map<int32_t, Crate> Items::Crates;
// pair Itemset, Rarity -> vector of pointers (map iterators) to records in ItemData
std::map<std::pair<int32_t, int32_t>, std::vector<std::map<std::pair<int32_t, int32_t>, Items::Item>::iterator>> Items::CrateItems;
std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> Items::CodeItems;

std::map<int32_t, MobDropChance> Items::MobDropChances;
std::map<int32_t, MobDrop> Items::MobDrops;

#ifdef ACADEMY
std::map<int32_t, int32_t> Items::NanoCapsules; // crate id -> nano id

static void nanoCapsuleHandler(CNSocket* sock, int slot, sItemBase *chest) {
    Player* plr = PlayerManager::getPlayer(sock);
    int32_t nanoId = NanoCapsules[chest->iID];

    // chest opening acknowledgement packet
    INITSTRUCT(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, resp);
    resp.iSlotNum = slot;

    // in order to remove capsule form inventory, we have to send item reward packet with empty item
    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);

    // we know it's only one trailing struct, so we can skip full validation
    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
    sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // maintain stats
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;

    item->iSlotNum = slot;
    item->eIL = 1;
    
    // update player serverside
    plr->Inven[slot] = item->sItem;

    // transmit item
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);

    // transmit chest opening acknowledgement packet
    sock->sendPacket((void*)&resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC));

    // check if player doesn't already have this nano
    if (plr->Nanos[nanoId].iID != 0) {
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        msg.iDuringTime = 4;
        std::string text = "You have already aquired this nano!";
        U8toU16(text, msg.szAnnounceMsg, sizeof(text));
        sock->sendPacket((void*)&msg, P_FE2CL_GM_REP_PC_ANNOUNCE, sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE));
        return;
    }
    Nanos::addNano(sock, nanoId, -1, false);
}
#endif

static int getRarity(Crate& crate, int itemSetId) {
    // find rarity ratio
    if (RarityRatios.find(crate.rarityRatioId) == RarityRatios.end()) {
        std::cout << "[WARN] Rarity Ratio " << crate.rarityRatioId << " not found!" << std::endl;
        return -1;
    }

    std::vector<int> rarityRatio = RarityRatios[crate.rarityRatioId];

    /*
     * First we have to check if specified item set contains items with all specified rarities,
     * and if not eliminate them from the draw
     * it is simpler to do here than to fix individually in the file
     */

    // remember that rarities start from 1!
    for (int i = 0; i < rarityRatio.size(); i++){
        if (CrateItems.find(std::make_pair(itemSetId, i+1)) == CrateItems.end())
            rarityRatio[i] = 0;
    }

    int total = 0;
    for (int value : rarityRatio)
        total += value;

    if (total == 0) {
        std::cout << "Item Set " << itemSetId << " has no items assigned?!" << std::endl;
        return -1;
    }

    // now return a random rarity number
    int randomNum = rand() % total;
    int rarity = 0;
    int sum = 0;
    do {
        sum += rarityRatio[rarity];
        rarity++;
    } while (sum <= randomNum);

    return rarity;
}

static int getCrateItem(sItemBase& result, int itemSetId, int rarity, int playerGender) {
    auto key = std::make_pair(itemSetId, rarity);

    if (CrateItems.find(key) == CrateItems.end()) {
        std::cout << "[WARN] Item Set ID " << itemSetId << " Rarity " << rarity << " does not exist" << std::endl;
        return -1;
    }

    // only take into account items that have correct gender
    std::vector<std::map<std::pair<int32_t, int32_t>, Item>::iterator> items;
    for (auto crateitem : CrateItems[key]) {
        int gender = crateitem->second.gender;
        // if gender is incorrect, exclude item
        if (gender != 0 && gender != playerGender)
            continue;
        items.push_back(crateitem);
    }

    if (items.size() == 0) {
        std::cout << "[WARN] Set ID " << itemSetId << " Rarity " << rarity << " contains no valid items" << std::endl;
        return -1;
    }

    auto item = items[rand() % items.size()];

    result.iID = item->first.first;
    result.iType = item->first.second;
    result.iOpt = 1;

    return 0;
}

static int getItemSetId(Crate& crate, int crateId) {
    int itemSetsCount = crate.itemSets.size();
    if (itemSetsCount == 0) {
        std::cout << "[WARN] Crate " << crateId << " has no item sets assigned?!" << std::endl;
        return -1;
    }

    // if crate points to multiple itemSets, choose a random one
    int itemSetIndex = rand() % itemSetsCount;
    return crate.itemSets[itemSetIndex];
}

static void itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    INITSTRUCT(sP_FE2CL_PC_ITEM_MOVE_SUCC, resp);

    Player* plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (itemmove->iToSlotNum < 0 || itemmove->iFromSlotNum < 0)
        return;
    // NOTE: sending a no-op, "move in-place" packet is not necessary

    if (plr->isTrading) {
        std::cout << "[WARN] Player attempted to move item while trading" << std::endl;
        return;
    }

    // get the fromItem
    sItemBase *fromItem;
    switch ((SlotType)itemmove->eFrom) {
    case SlotType::EQUIP:
        if (itemmove->iFromSlotNum >= AEQUIP_COUNT)
            return;

        fromItem = &plr->Equip[itemmove->iFromSlotNum];
        break;
    case SlotType::INVENTORY:
        if (itemmove->iFromSlotNum >= AINVEN_COUNT)
            return;

        fromItem = &plr->Inven[itemmove->iFromSlotNum];
        break;
    case SlotType::BANK:
        if (itemmove->iFromSlotNum >= 200)
            return;
        fromItem = &plr->Bank[(plr->activeBank * 200) + itemmove->iFromSlotNum];
        break;
    default:
        std::cout << "[WARN] MoveItem submitted unknown Item Type?! " << itemmove->eFrom << std::endl;
        return;
    }

    // get the toItem
    sItemBase* toItem;
    switch ((SlotType)itemmove->eTo) {
    case SlotType::EQUIP:
        if (itemmove->iToSlotNum >= AEQUIP_COUNT)
            return;

        toItem = &plr->Equip[itemmove->iToSlotNum];
        break;
    case SlotType::INVENTORY:
        if (itemmove->iToSlotNum >= AINVEN_COUNT)
            return;

        toItem = &plr->Inven[itemmove->iToSlotNum];
        break;
    case SlotType::BANK:
        if (itemmove->iToSlotNum >= 200)
            return;
        toItem = &plr->Bank[(plr->activeBank * 200) + itemmove->iToSlotNum];
        break;
    default:
        std::cout << "[WARN] MoveItem submitted unknown Item Type?! " << itemmove->eTo << std::endl;
        return;
    }

    // if equipping an item, validate that it's of the correct type for the slot
    if ((SlotType)itemmove->eTo == SlotType::EQUIP) {
        if (fromItem->iType == 10 && itemmove->iToSlotNum != 8)
            return; // vehicle in wrong slot
        else if (fromItem->iType != 10
              && !(fromItem->iType == 0 && itemmove->iToSlotNum == 7)
              && fromItem->iType != itemmove->iToSlotNum)
            return; // something other than a vehicle or a weapon in a non-matching slot
        else if (itemmove->iToSlotNum >= AEQUIP_COUNT) // TODO: reject slots >= 9?
            return; // invalid slot
    }

    // save items to response
    resp.eTo = itemmove->eFrom;
    resp.eFrom = itemmove->eTo;
    resp.ToSlotItem = *toItem;
    resp.FromSlotItem = *fromItem;

    // swap/stack items in session
    Item* itemDat = getItemData(toItem->iID, toItem->iType);
    Item* itemDatFrom = getItemData(fromItem->iID, fromItem->iType);
    if (itemDat != nullptr && itemDatFrom != nullptr && itemDat->stackSize > 1 && itemDat == itemDatFrom && fromItem->iOpt < itemDat->stackSize && toItem->iOpt < itemDat->stackSize) {
        // items are stackable, identical, and not maxed, so run stacking logic

        toItem->iOpt += fromItem->iOpt; // sum counts
        fromItem->iOpt = 0; // deplete from item
        if (toItem->iOpt > itemDat->stackSize) {
            // handle overflow
            fromItem->iOpt += (toItem->iOpt - itemDat->stackSize); // add overflow to fromItem
            toItem->iOpt = itemDat->stackSize; // set toItem count to max
        }

        if (fromItem->iOpt == 0) { // from item count depleted
            // delete item
            fromItem->iID = 0;
            fromItem->iType = 0;
            fromItem->iTimeLimit = 0;
        }

        resp.iFromSlotNum = itemmove->iFromSlotNum;
        resp.iToSlotNum = itemmove->iToSlotNum;
        resp.FromSlotItem = *fromItem;
        resp.ToSlotItem = *toItem;
    } else {
        // items not stackable; just swap them
        sItemBase temp = *toItem;
        *toItem = *fromItem;
        *fromItem = temp;
        resp.iFromSlotNum = itemmove->iToSlotNum;
        resp.iToSlotNum = itemmove->iFromSlotNum;
    }

    // send equip change to viewable players
    if (itemmove->eFrom == (int)SlotType::EQUIP || itemmove->eTo == (int)SlotType::EQUIP) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, equipChange);

        equipChange.iPC_ID = plr->iID;
        if (itemmove->eTo == (int)SlotType::EQUIP) {
            equipChange.iEquipSlotNum = itemmove->iToSlotNum;
            equipChange.EquipSlotItem = resp.FromSlotItem;
        } else {
            equipChange.iEquipSlotNum = itemmove->iFromSlotNum;
            equipChange.EquipSlotItem = resp.ToSlotItem;
        }

        // unequip vehicle if equip slot 8 is 0
        if (plr->Equip[8].iID == 0)
            plr->iPCState = 0;

        // send equip event to other players
        PlayerManager::sendToViewable(sock, (void*)&equipChange, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));

        // set equipment stats serverside
        setItemStats(plr);
    }

    // send response
    sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
}

static void itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ITEM_DELETE* itemdel = (sP_CL2FE_REQ_PC_ITEM_DELETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC, resp);

    Player* plr = PlayerManager::getPlayer(sock);

    resp.eIL = itemdel->eIL;
    resp.iSlotNum = itemdel->iSlotNum;

    // so, im not sure what this eIL thing does since you always delete items in inventory and not equips
    plr->Inven[itemdel->iSlotNum].iID = 0;
    plr->Inven[itemdel->iSlotNum].iType = 0;
    plr->Inven[itemdel->iSlotNum].iOpt = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC));
}

static void itemUseHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_USE))
        return; // ignore the malformed packet
    sP_CL2FE_REQ_ITEM_USE* request = (sP_CL2FE_REQ_ITEM_USE*)data->buf;
    Player* player = PlayerManager::getPlayer(sock);

    if (request->iSlotNum < 0 || request->iSlotNum >= AINVEN_COUNT)
        return; // sanity check

    // gumball can only be used from inventory, so we ignore eIL
    sItemBase gumball = player->Inven[request->iSlotNum];
    sNano nano = player->Nanos[player->equippedNanos[request->iNanoSlot]];

    // sanity check, check if gumball exists
    if (!(gumball.iOpt > 0 && gumball.iType == 7 && ((gumball.iID>=119 && gumball.iID<=121) || gumball.iID == 5 || gumball.iID == 29 || gumball.iID == 30 || gumball.iID == 32))) {
        std::cout << "[WARN] Gumball not found" << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_USE_FAIL, response);
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_ITEM_USE_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_USE_FAIL));
        return;
    }

    if (gumball.iID>=119 && gumball.iID<=121) {
        // sanity check, check if gumball type matches nano style
        int nanoStyle = Nanos::nanoStyle(nano.iID);
        if (!((gumball.iID == 119 && nanoStyle == 0) ||
            (  gumball.iID == 120 && nanoStyle == 1) ||
            (  gumball.iID == 121 && nanoStyle == 2))) {
            std::cout << "[WARN] Gumball type doesn't match nano type" << std::endl;
            INITSTRUCT(sP_FE2CL_REP_PC_ITEM_USE_FAIL, response);
            sock->sendPacket((void*)&response, P_FE2CL_REP_PC_ITEM_USE_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_USE_FAIL));
            return;
        }

        gumball.iOpt -= 1;
        if (gumball.iOpt == 0)
            gumball = {};

        size_t resplen = sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC) + sizeof(sSkillResult_Buff);

        // validate response packet
        if (!validOutVarPacket(sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC), 1, sizeof(sSkillResult_Buff))) {
            std::cout << "[WARN] bad sP_FE2CL_REP_PC_ITEM_USE_SUCC packet size" << std::endl;
            return;
        }

        if (gumball.iOpt == 0)
            gumball = {};

        uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
        memset(respbuf, 0, resplen);

        sP_FE2CL_REP_PC_ITEM_USE_SUCC *resp = (sP_FE2CL_REP_PC_ITEM_USE_SUCC*)respbuf;
        sSkillResult_Buff *respdata = (sSkillResult_Buff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));
        resp->iPC_ID = player->iID;
        resp->eIL = 1;
        resp->iSlotNum = request->iSlotNum;
        resp->RemainItem = gumball;
        resp->iTargetCnt = 1;
        resp->eST = EST_NANOSTIMPAK;
        resp->iSkillID = 144;

        int value1 = CSB_BIT_STIMPAKSLOT1 << request->iNanoSlot;
        int value2 = ECSB_STIMPAKSLOT1 + request->iNanoSlot;

        respdata->eCT = 1;
        respdata->iID = player->iID;
        respdata->iConditionBitFlag = value1;

        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
        pkt.eCSTB = value2; // eCharStatusTimeBuffID
        pkt.eTBU = 1; // eTimeBuffUpdate
        pkt.eTBT = 1; // eTimeBuffType 1 means nano
        pkt.iConditionBitFlag = player->iConditionBitFlag |= value1;
        sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));

        sock->sendPacket((void*)&respbuf, P_FE2CL_REP_PC_ITEM_USE_SUCC, resplen);
        // update inventory serverside
        player->Inven[resp->iSlotNum] = resp->RemainItem;

        std::pair<CNSocket*, int32_t> key = std::make_pair(sock, value1);
        time_t until = getTime() + (time_t)Nanos::SkillTable[144].durationTime[0] * 100;
        Eggs::EggBuffs[key] = until;
    } else {
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_USE_SUCC, resp);
        resp.iPC_ID = player->iID;
        resp.eIL = 1;
        resp.iSlotNum = request->iSlotNum;
        resp.iTargetCnt = 1;
        std::cout << gumball.iID << std::endl;
        switch (gumball.iID) {
            case 5: // Gold card
                if ((player->BankOwnership & 8) == 0) {
                    player->BankOwnership |= 8;
                    resp.eST = 1000;
                } else {
                    resp.eST = 1002;
                }
                break;
            case 32: // Sapphire card
                if ((player->BankOwnership & 4) == 0) {
                    player->BankOwnership |= 4;
                    resp.eST = 1000;
                } else {
                    resp.eST = 1002;
                }
                break;
            case 29: // Emerald card
                if ((player->BankOwnership & 2) == 0) {
                    player->BankOwnership |= 2;
                    resp.eST = 1000;
                } else {
                    resp.eST = 1002;
                }
                break;
            case 30: // Ruby card
                if ((player->BankOwnership & 1) == 0) {
                    player->BankOwnership |= 1;
                    resp.eST = 1000;
                } else {
                    resp.eST = 1002;
                }
                break;
            default:
                resp.eST = 1001;
        }
        resp.iSkillID = 144;
        gumball.iOpt -= 1;
        if (gumball.iOpt == 0)
            gumball = {};
        resp.RemainItem = gumball;
        player->Inven[resp.iSlotNum] = resp.RemainItem;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_USE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC));
    }
}

static void itemBankOpenHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_BANK_OPEN))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_BANK_OPEN *pkt = (sP_CL2FE_REQ_PC_BANK_OPEN *)data->buf;
    bool fail = false;
    switch (NPCManager::NPCs[pkt->iNPC_ID]->appearanceData.iNPCType) {
        case 2586: // Gold banker
            if ((plr->BankOwnership & 8) == 0) {
                fail = true;
                break;
            }
            plr->activeBank = 4;
            break;
        case 3129: // Sapphire banker
            if ((plr->BankOwnership & 4) == 0) {
                fail = true;
                break;
            }
            plr->activeBank = 3;
            break;
        case 2507: // Emerald banker
            if ((plr->BankOwnership & 2) == 0) {
                fail = true;
                break;
            }
            plr->activeBank = 2;
            break;
        case 3124: // Ruby banker
            if ((plr->BankOwnership & 1) == 0) {
                fail = true;
                break;
            }
            plr->activeBank = 1;
            break;
        default:
            plr->activeBank = 0;
            break;
    }

    if (fail) {
        INITSTRUCT(sP_FE2CL_REP_PC_BANK_OPEN_FAIL, resp);
        resp.iErrorCode = 2;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_BANK_OPEN_FAIL, sizeof(sP_FE2CL_REP_PC_BANK_OPEN_FAIL));
        return;
    }

    std::cout << "Player opening bank " << plr->activeBank << std::endl;

    // just send bank inventory
    INITSTRUCT(sP_FE2CL_REP_PC_BANK_OPEN_SUCC, resp);
    for (int i = 0; i < 200; i++) {
        resp.aBank[i] = plr->Bank[(plr->activeBank * 200) + i];
    }
    resp.iExtraBank = 1;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_BANK_OPEN_SUCC, sizeof(sP_FE2CL_REP_PC_BANK_OPEN_SUCC));
}

static void chestOpenHandler(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_CHEST_OPEN))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_CHEST_OPEN *pkt = (sP_CL2FE_REQ_ITEM_CHEST_OPEN *)data->buf;

    // sanity check
    if (pkt->eIL != 1 || pkt->iSlotNum < 0 || pkt->iSlotNum >= AINVEN_COUNT)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    sItemBase *chest = &plr->Inven[pkt->iSlotNum];
    // we could reject the packet if the client thinks the item is different, but eh

    if (chest->iType != 9) {
        std::cout << "[WARN] Player tried to open a crate with incorrect iType ?!" << std::endl;
        return;
    }

#ifdef ACADEMY
    // check if chest isn't a nano capsule
    if (NanoCapsules.find(chest->iID) != NanoCapsules.end())
        return nanoCapsuleHandler(sock, pkt->iSlotNum, chest);
#endif

    // chest opening acknowledgement packet
    INITSTRUCT(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, resp);
    resp.iSlotNum = pkt->iSlotNum;

    // item giving packet
    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // maintain stats
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;

    item->iSlotNum = pkt->iSlotNum;
    item->eIL = 1;

    int itemSetId = -1, rarity = -1, ret = -1;
    bool failing = false;

    // find the crate
    if (Crates.find(chest->iID) == Crates.end()) {
        std::cout << "[WARN] Crate " << chest->iID << " not found!" << std::endl;
        failing = true;
    }
    Crate& crate = Crates[chest->iID];

    if (!failing)
        itemSetId = getItemSetId(crate, chest->iID);
    if (itemSetId == -1)
        failing = true;

    if (!failing)
        rarity = getRarity(crate, itemSetId);
    if (rarity == -1)
        failing = true;

    if (!failing)
        ret = getCrateItem(item->sItem, itemSetId, rarity, plr->PCStyle.iGender);
    if (ret == -1)
        failing = true;

    // if we failed to open a crate, at least give the player a gumball (suggested by Jade)
    if (failing) {
        item->sItem.iType = 7;
        item->sItem.iID = 119 + (rand() % 3);
        item->sItem.iOpt = 1;
    }
    // update player
    plr->Inven[pkt->iSlotNum] = item->sItem;

    // transmit item
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);

    // transmit chest opening acknowledgement packet
    std::cout << "opening chest..." << std::endl;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC));
}

// TODO: use this in cleaned up Items
int Items::findFreeSlot(Player *plr) {
    int i;

    for (i = 0; i < AINVEN_COUNT; i++)
        if (plr->Inven[i].iType == 0 && plr->Inven[i].iID == 0 && plr->Inven[i].iOpt == 0)
            return i;

    // not found
    return -1;
}

Item* Items::getItemData(int32_t id, int32_t type) {
    if(ItemData.find(std::make_pair(id, type)) !=  ItemData.end())
        return &ItemData[std::make_pair(id, type)];
    return nullptr;
}

void Items::checkItemExpire(CNSocket* sock, Player* player) {
    if (player->toRemoveVehicle.eIL == 0 && player->toRemoveVehicle.iSlotNum == 0)
        return;

    /* prepare packet
    * yes, this is a varadic packet, however analyzing client behavior and code
    * it only checks takes the first item sent into account
    * yes, this is very stupid
    * therefore, we delete all but 1 expired vehicle while loading player
    * to delete the last one here so player gets a notification
    */

    const size_t resplen = sizeof(sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM) + sizeof(sTimeLimitItemDeleteInfo2CL);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation
    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM* packet = (sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM*)respbuf;
    sTimeLimitItemDeleteInfo2CL* itemData = (sTimeLimitItemDeleteInfo2CL*)(respbuf + sizeof(sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM));
    memset(respbuf, 0, resplen);

    packet->iItemListCount = 1;
    itemData->eIL = player->toRemoveVehicle.eIL;
    itemData->iSlotNum = player->toRemoveVehicle.iSlotNum;
    sock->sendPacket((void*)&respbuf, P_FE2CL_PC_DELETE_TIME_LIMIT_ITEM, resplen);

    // delete serverside
    if (player->toRemoveVehicle.eIL == 0)
        memset(&player->Equip[8], 0, sizeof(sItemBase));
    else
        memset(&player->Inven[player->toRemoveVehicle.iSlotNum], 0, sizeof(sItemBase));

    player->toRemoveVehicle.eIL = 0;
    player->toRemoveVehicle.iSlotNum = 0;
}

void Items::setItemStats(Player* plr) {

    plr->pointDamage = 8 + plr->level * 2;
    plr->groupDamage = 8 + plr->level * 2;
    plr->fireRate = 0;
    plr->defense = 16 + plr->level * 4;

    Item* itemStatsDat;

    for (int i = 0; i < 4; i++) {
        itemStatsDat = getItemData(plr->Equip[i].iID, plr->Equip[i].iType);
        if (itemStatsDat == nullptr) {
            std::cout << "[WARN] setItemStats(): getItemData() returned NULL" << std::endl;
            continue;
        }
        plr->pointDamage += itemStatsDat->pointDamage;
        plr->groupDamage += itemStatsDat->groupDamage;
        plr->fireRate += itemStatsDat->fireRate;
        plr->defense += itemStatsDat->defense;
    }
}

// HACK: work around the invisible weapon bug
// TODO: I don't think this makes a difference at all? Check and remove, if necessary.
void Items::updateEquips(CNSocket* sock, Player* plr) {
    for (int i = 0; i < 4; i++) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, resp);

        resp.iPC_ID = plr->iID;
        resp.iEquipSlotNum = i;
        resp.EquipSlotItem = plr->Equip[i];

        PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
    }
}

static void getMobDrop(sItemBase *reward, MobDrop* drop, MobDropChance* chance, int rolled) {
    reward->iType = 9;
    reward->iOpt = 1;

    int total = 0;
    for (int ratio : chance->cratesRatio)
        total += ratio;

    // randomizing a crate
    int randomNum = rolled % total;
    int i = 0;
    int sum = 0;
    do {
        reward->iID = drop->crateIDs[i];
        sum += chance->cratesRatio[i];
        i++;
    }
    while (sum<=randomNum);
}

static void giveEventDrop(CNSocket* sock, Player* player, int rolled) {
    // random drop chance
    if (rand() % 100 > settings::EVENTCRATECHANCE)
        return;

    // no slot = no reward
    int slot = findFreeSlot(player);
    if (slot == -1)
        return;

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);

    uint8_t respbuf[resplen];
    sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
    sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // leave everything here as it is
    reward->m_iCandy = player->money;
    reward->m_iFusionMatter = player->fusionmatter;
    reward->m_iBatteryN = player->batteryN;
    reward->m_iBatteryW = player->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    // which crate to drop
    int crateId;
    switch (settings::EVENTMODE) {
    // knishmas
    case 1: crateId = 1187; break;
    // halloween
    case 2: crateId = 1181; break;
    // spring
    case 3: crateId = 1126; break;
    // what
    default:
        std::cout << "[WARN] Unknown event Id " << settings::EVENTMODE << std::endl;
        return;
    }

    item->sItem.iType = 9;
    item->sItem.iID = crateId;
    item->sItem.iOpt = 1;
    item->iSlotNum = slot;
    item->eIL = 1; // Inventory Location. 1 means player inventory.

    // update player
    player->Inven[slot] = item->sItem;
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
}

void Items::giveMobDrop(CNSocket *sock, Mob* mob, int rolledBoosts, int rolledPotions,
                            int rolledCrate, int rolledCrateType, int rolledEvent) {
    Player *plr = PlayerManager::getPlayer(sock);

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // sanity check
    if (MobDrops.find(mob->dropType) == MobDrops.end()) {
        std::cout << "[WARN] Drop Type " << mob->dropType << " was not found" << std::endl;
        return;
    }
    // find correct mob drop
    MobDrop& drop = MobDrops[mob->dropType];

    plr->money += drop.taros;
    // money nano boost
    if (plr->iConditionBitFlag & CSB_BIT_REWARD_CASH) {
        int boost = 0;
        if (Nanos::getNanoBoost(plr)) // for gumballs
            boost = 1;
        plr->money += drop.taros * (5 + boost) / 25;
    }
    // formula for scaling FM with player/mob level difference
    // TODO: adjust this better
    int levelDifference = plr->level - mob->level;
    int fm = drop.fm;
    if (levelDifference > 0)
        fm = levelDifference < 10 ? fm - (levelDifference * fm / 10) : 0;
    // scavenger nano boost
    if (plr->iConditionBitFlag & CSB_BIT_REWARD_BLOB) {
        int boost = 0;
        if (Nanos::getNanoBoost(plr)) // for gumballs
            boost = 1;
        fm += fm * (5 + boost) / 25;
    }

    Missions::updateFusionMatter(sock, fm);

    // give boosts 1 in 3 times
    if (drop.boosts > 0) {
        if (rolledPotions % 3 == 0)
            plr->batteryN += drop.boosts;
        if (rolledBoosts % 3 == 0)
            plr->batteryW += drop.boosts;
    }
    // caps
    if (plr->batteryW > 9999)
        plr->batteryW = 9999;
    if (plr->batteryN > 9999)
        plr->batteryN = 9999;

    // simple rewards
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    int slot = findFreeSlot(plr);

    bool awardDrop = false;
    MobDropChance *chance = nullptr;
    // sanity check
    if (MobDropChances.find(drop.dropChanceType) == MobDropChances.end()) {
        std::cout << "[WARN] Unknown Drop Chance Type: " << drop.dropChanceType << std::endl;
        return; // this also prevents holiday crate drops, but oh well
    } else {
        chance = &MobDropChances[drop.dropChanceType];
        awardDrop = (rolledCrate % 1000 < chance->dropChance);
    }

    // no drop
    if (slot == -1 || !awardDrop) {
        // no room for an item, but you still get FM and taros
        reward->iItemCnt = 0;
        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, sizeof(sP_FE2CL_REP_REWARD_ITEM));
    } else {
        // item reward
        getMobDrop(&item->sItem, &drop, chance, rolledCrateType);
        item->iSlotNum = slot;
        item->eIL = 1; // Inventory Location. 1 means player inventory.

        // update player
        plr->Inven[slot] = item->sItem;

        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
    }

    // event crates
    if (settings::EVENTMODE != 0)
        giveEventDrop(sock, plr, rolledEvent);
}

void Items::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    // this one is for gumballs
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_USE, itemUseHandler);
    // Bank
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BANK_OPEN, itemBankOpenHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_CHEST_OPEN, chestOpenHandler);
}

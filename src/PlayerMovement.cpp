#include "PlayerMovement.hpp"
#include "PlayerManager.hpp"
#include "core/Core.hpp"

static void movePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVE))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_MOVE* moveData = (sP_CL2FE_REQ_PC_MOVE*)data->buf;
    PlayerManager::updatePlayerPosition(sock, moveData->iX, moveData->iY, moveData->iZ, plr->instanceID, moveData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_MOVE, moveResponse);

    moveResponse.iID = plr->iID;
    moveResponse.cKeyValue = moveData->cKeyValue;

    moveResponse.iX = moveData->iX;
    moveResponse.iY = moveData->iY;
    moveResponse.iZ = moveData->iZ;
    moveResponse.iAngle = moveData->iAngle;
    moveResponse.fVX = moveData->fVX;
    moveResponse.fVY = moveData->fVY;
    moveResponse.fVZ = moveData->fVZ;

    moveResponse.iSpeed = moveData->iSpeed;
    moveResponse.iCliTime = moveData->iCliTime; // maybe don't send this??? seems unneeded...
    moveResponse.iSvrTime = tm;

    PlayerManager::sendToViewable(sock, (void*)&moveResponse, P_FE2CL_PC_MOVE, sizeof(sP_FE2CL_PC_MOVE));
}

static void stopPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_STOP))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_STOP* stopData = (sP_CL2FE_REQ_PC_STOP*)data->buf;
    PlayerManager::updatePlayerPosition(sock, stopData->iX, stopData->iY, stopData->iZ, plr->instanceID, plr->angle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_STOP, stopResponse);

    stopResponse.iID = plr->iID;

    stopResponse.iX = stopData->iX;
    stopResponse.iY = stopData->iY;
    stopResponse.iZ = stopData->iZ;

    stopResponse.iCliTime = stopData->iCliTime; // maybe don't send this??? seems unneeded...
    stopResponse.iSvrTime = tm;

    PlayerManager::sendToViewable(sock, (void*)&stopResponse, P_FE2CL_PC_STOP, sizeof(sP_FE2CL_PC_STOP));
}

static void jumpPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMP))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_JUMP* jumpData = (sP_CL2FE_REQ_PC_JUMP*)data->buf;
    PlayerManager::updatePlayerPosition(sock, jumpData->iX, jumpData->iY, jumpData->iZ, plr->instanceID, jumpData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_JUMP, jumpResponse);

    jumpResponse.iID = plr->iID;
    jumpResponse.cKeyValue = jumpData->cKeyValue;

    jumpResponse.iX = jumpData->iX;
    jumpResponse.iY = jumpData->iY;
    jumpResponse.iZ = jumpData->iZ;
    jumpResponse.iAngle = jumpData->iAngle;
    jumpResponse.iVX = jumpData->iVX;
    jumpResponse.iVY = jumpData->iVY;
    jumpResponse.iVZ = jumpData->iVZ;

    jumpResponse.iSpeed = jumpData->iSpeed;
    jumpResponse.iCliTime = jumpData->iCliTime; // maybe don't send this??? seems unneeded...
    jumpResponse.iSvrTime = tm;

    PlayerManager::sendToViewable(sock, (void*)&jumpResponse, P_FE2CL_PC_JUMP, sizeof(sP_FE2CL_PC_JUMP));
}

static void jumppadPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMPPAD))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_JUMPPAD* jumppadData = (sP_CL2FE_REQ_PC_JUMPPAD*)data->buf;
    PlayerManager::updatePlayerPosition(sock, jumppadData->iX, jumppadData->iY, jumppadData->iZ, plr->instanceID, jumppadData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_JUMPPAD, jumppadResponse);

    jumppadResponse.iPC_ID = plr->iID;
    jumppadResponse.cKeyValue = jumppadData->cKeyValue;

    jumppadResponse.iX = jumppadData->iX;
    jumppadResponse.iY = jumppadData->iY;
    jumppadResponse.iZ = jumppadData->iZ;
    jumppadResponse.iVX = jumppadData->iVX;
    jumppadResponse.iVY = jumppadData->iVY;
    jumppadResponse.iVZ = jumppadData->iVZ;

    jumppadResponse.iCliTime = jumppadData->iCliTime;
    jumppadResponse.iSvrTime = tm;

    PlayerManager::sendToViewable(sock, (void*)&jumppadResponse, P_FE2CL_PC_JUMPPAD, sizeof(sP_FE2CL_PC_JUMPPAD));
}

static void launchPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_LAUNCHER))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_LAUNCHER* launchData = (sP_CL2FE_REQ_PC_LAUNCHER*)data->buf;
    PlayerManager::updatePlayerPosition(sock, launchData->iX, launchData->iY, launchData->iZ, plr->instanceID, launchData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_LAUNCHER, launchResponse);

    launchResponse.iPC_ID = plr->iID;

    launchResponse.iX = launchData->iX;
    launchResponse.iY = launchData->iY;
    launchResponse.iZ = launchData->iZ;
    launchResponse.iVX = launchData->iVX;
    launchResponse.iVY = launchData->iVY;
    launchResponse.iVZ = launchData->iVZ;
    launchResponse.iSpeed = launchData->iSpeed;
    launchResponse.iAngle = launchData->iAngle;

    launchResponse.iCliTime = launchData->iCliTime;
    launchResponse.iSvrTime = tm;

    PlayerManager::sendToViewable(sock, (void*)&launchResponse, P_FE2CL_PC_LAUNCHER, sizeof(sP_FE2CL_PC_LAUNCHER));
}

static void ziplinePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ZIPLINE))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_ZIPLINE* ziplineData = (sP_CL2FE_REQ_PC_ZIPLINE*)data->buf;
    PlayerManager::updatePlayerPosition(sock, ziplineData->iX, ziplineData->iY, ziplineData->iZ, plr->instanceID, ziplineData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_ZIPLINE, ziplineResponse);

    ziplineResponse.iPC_ID = plr->iID;
    ziplineResponse.iCliTime = ziplineData->iCliTime;
    ziplineResponse.iSvrTime = tm;
    ziplineResponse.iX = ziplineData->iX;
    ziplineResponse.iY = ziplineData->iY;
    ziplineResponse.iZ = ziplineData->iZ;
    ziplineResponse.fVX = ziplineData->fVX;
    ziplineResponse.fVY = ziplineData->fVY;
    ziplineResponse.fVZ = ziplineData->fVZ;
    ziplineResponse.fMovDistance = ziplineData->fMovDistance;
    ziplineResponse.fMaxDistance = ziplineData->fMaxDistance;
    ziplineResponse.fDummy = ziplineData->fDummy; // wtf is this for?
    ziplineResponse.iStX = ziplineData->iStX;
    ziplineResponse.iStY = ziplineData->iStY;
    ziplineResponse.iStZ = ziplineData->iStZ;
    ziplineResponse.bDown = ziplineData->bDown;
    ziplineResponse.iSpeed = ziplineData->iSpeed;
    ziplineResponse.iAngle = ziplineData->iAngle;
    ziplineResponse.iRollMax = ziplineData->iRollMax;
    ziplineResponse.iRoll = ziplineData->iRoll;

    PlayerManager::sendToViewable(sock, (void*)&ziplineResponse, P_FE2CL_PC_ZIPLINE, sizeof(sP_FE2CL_PC_ZIPLINE));
}

static void movePlatformPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVEPLATFORM))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_MOVEPLATFORM* platformData = (sP_CL2FE_REQ_PC_MOVEPLATFORM*)data->buf;
    PlayerManager::updatePlayerPosition(sock, platformData->iX, platformData->iY, platformData->iZ, plr->instanceID, platformData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_MOVEPLATFORM, platResponse);

    platResponse.iPC_ID = plr->iID;
    platResponse.iCliTime = platformData->iCliTime;
    platResponse.iSvrTime = tm;
    platResponse.iX = platformData->iX;
    platResponse.iY = platformData->iY;
    platResponse.iZ = platformData->iZ;
    platResponse.iAngle = platformData->iAngle;
    platResponse.fVX = platformData->fVX;
    platResponse.fVY = platformData->fVY;
    platResponse.fVZ = platformData->fVZ;
    platResponse.iLcX = platformData->iLcX;
    platResponse.iLcY = platformData->iLcY;
    platResponse.iLcZ = platformData->iLcZ;
    platResponse.iSpeed = platformData->iSpeed;
    platResponse.bDown = platformData->bDown;
    platResponse.cKeyValue = platformData->cKeyValue;
    platResponse.iPlatformID = platformData->iPlatformID;

    PlayerManager::sendToViewable(sock, (void*)&platResponse, P_FE2CL_PC_MOVEPLATFORM, sizeof(sP_FE2CL_PC_MOVEPLATFORM));
}

static void moveSliderPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVETRANSPORTATION))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_MOVETRANSPORTATION* sliderData = (sP_CL2FE_REQ_PC_MOVETRANSPORTATION*)data->buf;
    PlayerManager::updatePlayerPosition(sock, sliderData->iX, sliderData->iY, sliderData->iZ, plr->instanceID, sliderData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_MOVETRANSPORTATION, sliderResponse);

    sliderResponse.iPC_ID = plr->iID;
    sliderResponse.iCliTime = sliderData->iCliTime;
    sliderResponse.iSvrTime = tm;
    sliderResponse.iX = sliderData->iX;
    sliderResponse.iY = sliderData->iY;
    sliderResponse.iZ = sliderData->iZ;
    sliderResponse.iAngle = sliderData->iAngle;
    sliderResponse.fVX = sliderData->fVX;
    sliderResponse.fVY = sliderData->fVY;
    sliderResponse.fVZ = sliderData->fVZ;
    sliderResponse.iLcX = sliderData->iLcX;
    sliderResponse.iLcY = sliderData->iLcY;
    sliderResponse.iLcZ = sliderData->iLcZ;
    sliderResponse.iSpeed = sliderData->iSpeed;
    sliderResponse.cKeyValue = sliderData->cKeyValue;
    sliderResponse.iT_ID = sliderData->iT_ID;

    PlayerManager::sendToViewable(sock, (void*)&sliderResponse, P_FE2CL_PC_MOVETRANSPORTATION, sizeof(sP_FE2CL_PC_MOVETRANSPORTATION));
}

static void moveSlopePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_SLOPE))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    sP_CL2FE_REQ_PC_SLOPE* slopeData = (sP_CL2FE_REQ_PC_SLOPE*)data->buf;
    PlayerManager::updatePlayerPosition(sock, slopeData->iX, slopeData->iY, slopeData->iZ, plr->instanceID, slopeData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_SLOPE, slopeResponse);

    slopeResponse.iPC_ID = plr->iID;
    slopeResponse.iCliTime = slopeData->iCliTime;
    slopeResponse.iSvrTime = tm;
    slopeResponse.iX = slopeData->iX;
    slopeResponse.iY = slopeData->iY;
    slopeResponse.iZ = slopeData->iZ;
    slopeResponse.iAngle = slopeData->iAngle;
    slopeResponse.fVX = slopeData->fVX;
    slopeResponse.fVY = slopeData->fVY;
    slopeResponse.fVZ = slopeData->fVZ;
    slopeResponse.iSpeed = slopeData->iSpeed;
    slopeResponse.cKeyValue = slopeData->cKeyValue;
    slopeResponse.iSlopeID = slopeData->iSlopeID;

    PlayerManager::sendToViewable(sock, (void*)&slopeResponse, P_FE2CL_PC_SLOPE, sizeof(sP_FE2CL_PC_SLOPE));
}

void PlayerMovement::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVE, movePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_STOP, stopPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_JUMP, jumpPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_JUMPPAD, jumppadPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_LAUNCHER, launchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ZIPLINE, ziplinePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVEPLATFORM, movePlatformPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVETRANSPORTATION, moveSliderPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SLOPE, moveSlopePlayer);
}

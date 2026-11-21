#include "CreatorLevel.h"
#include "../world/Difficulty.h"
#include "../world/entity/player/Player.h"
#include "../world/level/chunk/ChunkSource.h"
#include "../world/level/MobSpawner.h"
#include "../util/PerfTimer.h"
#include "../network/RakNetInstance.h"
#include "../network/packet/SetEntityDataPacket.h"
#include "../network/packet/SetTimePacket.h"
#include "../world/level/tile/LevelEvent.h"

CreatorLevel::CreatorLevel(LevelStorage* levelStorage, const std::string& levelName, const LevelSettings& settings, int generatorVersion, Dimension* fixedDimension /* = NULL */)
:	super(levelStorage, levelName, settings, generatorVersion, fixedDimension)
{
}

void CreatorLevel::updateSleepingPlayerList() {
	bool allPlayersWasSleeping = allPlayersAreSleeping;
	allPlayersAreSleeping = !players.empty();
	for(PlayerList::iterator it = players.begin(); it != players.end(); ++it) {
		Player* player = *it;
		if(!player->isSleeping()) {
			allPlayersAreSleeping = false;
			break;
		}
	}
	if(!allPlayersWasSleeping && allPlayersAreSleeping) {
		// CreatorLevel is also the authoritative level while the UWP host is
		// open to LAN, so notify remote clients just like ServerLevel does.
		levelEvent(NULL, LevelEvent::ALL_PLAYERS_SLEEPING, 0, 0, 0, 0);
		for(PlayerList::iterator it = players.begin(); it != players.end(); ++it) {
			(*it)->setAllPlayersSleeping();
		}
	}
}

void CreatorLevel::awakenAllPlayers() {
	allPlayersAreSleeping = false;
	for(PlayerList::iterator it = players.begin(); it != players.end(); ++it) {
		Player* player = *it;
		if(player->isSleeping()) {
			player->stopSleepInBed(false, false, true);
			player->health = Player::MAX_HEALTH;
			player->lastHealth = Player::MAX_HEALTH;
		}
	}
}

bool CreatorLevel::allPlayersSleeping() {
	if(allPlayersAreSleeping && !isClientSide) {
		for(PlayerList::iterator it = players.begin(); it != players.end(); ++it) {
			if(!(*it)->isSleepingLongEnough()) {
				return false;
			}
		}
		return true;
	}
	return false;
}

void CreatorLevel::tick() {
	// CreatorLevel is used by the local UWP world as the authoritative
	// single-player Level.  Its historical lightweight tick omitted the two
	// client-visible parts of Level::tick(): updating the cached sky darkness
	// and invoking the original mob spawner.  Keep the local/offline tick free
	// of server-side world events, but restore those original responsibilities.
	if (!isClientSide && levelData.getSpawnMobs()) {
		static int mobSpawnTick = 0;
		const int mobSpawnInterval = 2;
		if (++mobSpawnTick >= mobSpawnInterval) {
			mobSpawnTick = 0;
			TIMER_PUSH("mobSpawner");
			MobSpawner::tick(this,
				_spawnEnemies && difficulty > Difficulty::PEACEFUL,
				_spawnFriendlies && (levelData.getTime() % 400) < mobSpawnInterval);
			TIMER_POP();
		}
	}

	TIMER_PUSH("chunkSource");
	_chunkSource->tick();

	// LevelRenderer listens for skyColorChanged() and rebuilds the affected
	// lighting state.  Without this call skyDarken stayed at the value from
	// world load until the next reload.
	updateSkyDarken();

	if(isNightMode()) {
		// Nether reactor night mode uses the same accelerated transition as
		// Level::tick(); time packets keep connected clients in sync as well.
		long curTime = levelData.getTime();
		if(curTime % TICKS_PER_DAY != MIDDLE_OF_NIGHT_TIME) {
			if(curTime % TICKS_PER_DAY < MIDDLE_OF_NIGHT_TIME &&
				(curTime + 20) % TICKS_PER_DAY > MIDDLE_OF_NIGHT_TIME) {
				curTime = MIDDLE_OF_NIGHT_TIME;
				SetTimePacket packet(curTime);
				raknetInstance->send(packet);
			}
			else {
				curTime += 20;
				if(curTime % 20 == 0) {
					SetTimePacket packet(curTime);
					raknetInstance->send(packet);
				}
			}
			levelData.setTime(curTime % TICKS_PER_DAY);
		}
	}
	else {
		long time = levelData.getTime() + 1;
		levelData.setTime(time);
		if ((time & 255) == 0) {
			SetTimePacket packet(time);
			raknetInstance->send(packet);
		}
	}

	TIMER_POP_PUSH("tickPending");
	tickPendingTicks(false);

	// CreatorLevel intentionally avoids Level::tick() because that method
	// emits server packets.  Keep its random block ticks, which drive leaf
	// decay and other scheduled world behaviour.
	TIMER_POP_PUSH("tickTiles");
	tickTiles();
	TIMER_POP();

	// Match the original single-player/server behaviour: once every player
	// has stayed in bed long enough, move to the next day and wake them.
	if(allPlayersSleeping()) {
		long newTime = levelData.getTime() + TICKS_PER_DAY;
		levelData.setTime(newTime - (newTime % TICKS_PER_DAY));
		SetTimePacket packet(levelData.getTime());
		raknetInstance->send(packet);
		awakenAllPlayers();
	}

	// Publish sleeping/waking flags for remote players and the host player.
	// Entity data is packed as dirty, so this only sends state that changed.
	if (!isClientSide && raknetInstance) {
		for (unsigned int i = 0; i < entities.size(); ++i) {
			Entity* entity = entities[i];
			SynchedEntityData* data = entity->getEntityData();
			if (data && data->isDirty()) {
				SetEntityDataPacket packet(entity->entityId, *data);
				raknetInstance->send(packet);
			}
		}
	}
}

/*
void CreatorLevel::addToTickNextTick(int x, int y, int z, int tileId, int tickDelay) {
	if (tileId >= Tile::water->id && tileId <= calmLava)
		super::addToTickNextTick(x, y, z, tileId, tickDelay);
}
*/

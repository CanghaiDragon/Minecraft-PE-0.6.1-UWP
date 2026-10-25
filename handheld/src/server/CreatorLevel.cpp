#include "CreatorLevel.h"
#include "../world/Difficulty.h"
#include "../world/entity/player/Player.h"
#include "../world/level/chunk/ChunkSource.h"
#include "../world/level/MobSpawner.h"
#include "../util/PerfTimer.h"

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
		// The local UWP world has no server event stream, but it still needs
		// to start the same sleep countdown as the original ServerLevel.
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
	// of server packet emission, but restore those original responsibilities.
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
		// Level::tick(), but the local UWP world must not emit server packets.
		long curTime = levelData.getTime();
		if(curTime % TICKS_PER_DAY != MIDDLE_OF_NIGHT_TIME) {
			if(curTime % TICKS_PER_DAY < MIDDLE_OF_NIGHT_TIME &&
				(curTime + 20) % TICKS_PER_DAY > MIDDLE_OF_NIGHT_TIME) {
				curTime = MIDDLE_OF_NIGHT_TIME;
			}
			else {
				curTime += 20;
			}
			levelData.setTime(curTime % TICKS_PER_DAY);
		}
	}
	else {
		long time = levelData.getTime() + 1;
		levelData.setTime(time);
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
		awakenAllPlayers();
	}
}

/*
void CreatorLevel::addToTickNextTick(int x, int y, int z, int tileId, int tickDelay) {
	if (tileId >= Tile::water->id && tileId <= calmLava)
		super::addToTickNextTick(x, y, z, tileId, tickDelay);
}
*/

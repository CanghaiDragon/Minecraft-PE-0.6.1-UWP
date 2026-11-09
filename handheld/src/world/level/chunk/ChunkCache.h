#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

//package net.minecraft.world.level.chunk;

#include "ChunkSource.h"
#include "storage/ChunkStorage.h"
#include "EmptyLevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../tile/entity/TileEntity.h"
#include "../../../platform/log.h"
#include <unordered_map>
#include <cstdint>
#include <vector>

class ChunkCache: public ChunkSource {
    //static const int CHUNK_CACHE_WIDTH = CHUNK_CACHE_WIDTH; // WAS 32;
    static const int MAX_SAVES = 2;
public:
    ChunkCache(Level* level_, ChunkStorage* storage_, ChunkSource* source_)
	:	xLast(-999999999),
		zLast(-999999999),
		last(NULL),
		level(level_),
		storage(storage_),
		source(source_)
	{
		isChunkCache = true;
		emptyChunk = new EmptyLevelChunk(level_, NULL, 0, 0);
    }

	~ChunkCache() {
		delete source;
		delete emptyChunk;
		for (auto& kv : chunks) {
			if (kv.second && kv.second != emptyChunk) {
				kv.second->deleteBlockData();
				delete kv.second;
			}
		}
	}

	static int64_t chunkKey(int x, int z) {
		return ((int64_t)(uint32_t)x << 32) | (uint32_t)z;
	}

    bool fits(int x, int z) {
        if (level->isInfinite()) return true;
        return (x >= 0 && z >= 0 && x < CHUNK_CACHE_WIDTH && z < CHUNK_CACHE_WIDTH);
    }

    bool hasChunk(int x, int z) {
        if (!fits(x, z)) return true;
        if (x == xLast && z == zLast && last != NULL) return true;
        int64_t key = chunkKey(x, z);
        auto it = chunks.find(key);
        return it != chunks.end() && (it->second == emptyChunk || it->second->isAt(x, z));
    }

    // Storage maintenance needs to inspect resident chunks without calling
    // getChunk(), because getChunk() creates/generates a missing chunk.
    void getLoadedChunks(std::vector<LevelChunk*>& result) {
        result.clear();
        result.reserve(chunks.size());
        for (auto& entry : chunks) {
            if (entry.second != NULL && entry.second != emptyChunk)
                result.push_back(entry.second);
        }
    }

    LevelChunk* create(int x, int z) {
        return getChunk(x, z);
    }

    LevelChunk* getChunk(int x, int z) {
		if (x == xLast && z == zLast && last != NULL) return last;
		if (!fits(x, z)) return emptyChunk;

        int64_t key = chunkKey(x, z);
        if (!hasChunk(x, z)) {
            // No eviction needed — hash map holds each (x,z) independently
			LevelChunk* newChunk = load(x, z);
			bool updateLights = false;
			bool generatedChunk = false;
            if (newChunk == NULL) {
                if (source == NULL) newChunk = emptyChunk;
				else {
					newChunk = source->getChunk(x, z);
					generatedChunk = newChunk != NULL && newChunk != emptyChunk;
				}
			} else {
				updateLights = true;
            }
            chunks[key] = newChunk;
            newChunk->lightLava();

			// A generated chunk runs recalcHeightmap() inside its source before
			// ChunkCache has inserted it into the resident-chunk map.  Any
			// lightGaps() calls made during that first pass can therefore fail
			// Level::hasChunkAt() for the chunk itself and silently enqueue no
			// light work.  Repeat the height/light initialization after insertion
			// so newly generated Infinite and Sky chunks can submit their light
			// updates with the correct resident-chunk state.
			if (generatedChunk)
				newChunk->recalcHeightmap();

			// Match the Universal client: a chunk loaded from storage gets
			// targeted column updates.  Freshly generated chunks receive the
			// post-insertion recalcHeightmap() pass above.
			if (updateLights) {
				for (int cx = 0; cx < 16; cx++) {
					for (int cz = 0; cz < 16; cz++) {
						int height = level->getHeightmap(cx + x * 16, cz + z * 16);
						for (int cy = height; cy >= 0; cy--) {
							level->updateLight(LightLayer::Sky,
								cx + x * 16, cy, cz + z * 16,
								cx + x * 16, cy, cz + z * 16);
							level->updateLight(LightLayer::Block,
								cx + x * 16 - 1, cy, cz + z * 16 - 1,
								cx + x * 16 + 1, cy, cz + z * 16 + 1);
						}
					}
				}
			}

            LevelChunk* stored = chunks[key];
            if (stored != NULL) stored->load();

            // Tell the renderer (via LevelListener) that this chunk's blocks exist now,
            // so it rebuilds the faces at the boundary with already-rendered neighbours.
            level->setTilesDirty(x * 16, 0, z * 16, x * 16 + 15, 127, z * 16 + 15);

            if (!chunks[key]->terrainPopulated && hasChunk(x + 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x + 1, z)) postProcess(this, x, z);
            if (hasChunk(x - 1, z) && !getChunk(x - 1, z)->terrainPopulated && hasChunk(x - 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z);
            if (hasChunk(x, z - 1) && !getChunk(x, z - 1)->terrainPopulated && hasChunk(x + 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x + 1, z)) postProcess(this, x, z - 1);
            if (hasChunk(x - 1, z - 1) && !getChunk(x - 1, z - 1)->terrainPopulated && hasChunk(x - 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z - 1);
        }
        xLast = x;
        zLast = z;
        last = chunks[key];
        return chunks[key];
    }

	Biome::MobList getMobsAt(const MobCategory& mobCategory, int x, int y, int z) {
		return source->getMobsAt(mobCategory, x, y, z);
	}

    void postProcess(ChunkSource* parent, int x, int z) {
		if (!fits(x, z)) return;
        LevelChunk* chunk = getChunk(x, z);
        if (!chunk->terrainPopulated) {
			LOGI("ChunkCache postProcess begin: chunk=%d,%d terrainPopulated=%d unsaved=%d\n",
				x, z, chunk->terrainPopulated ? 1 : 0, chunk->unsaved ? 1 : 0);
            if (source != NULL) {
                source->postProcess(parent, x, z);
				chunk->clearUpdateMap();
            }
			// Mark the chunk populated only after all terrain features (trees,
			// snow, ores, etc.) have been generated.  Post-processing modifies
			// blocks outside the initial terrain pass, so force the completed
			// result to be written on the next save.
			chunk->terrainPopulated = true;
			chunk->markUnsaved();
			LOGI("ChunkCache postProcess end: chunk=%d,%d terrainPopulated=%d unsaved=%d\n",
				x, z, chunk->terrainPopulated ? 1 : 0, chunk->unsaved ? 1 : 0);
        }
    }

	// Finish population passes before a forced save.  The normal getChunk() path
	// waits until (x + 1,z + 1), (x,z + 1), and (x + 1,z) exist before calling
	// postProcess().  Calling postProcess() unconditionally here can run snow
	// and foliage placement while the loading-edge neighbours are still absent;
	// the pass then marks the chunk populated and the missing features can never
	// be retried after the next load.  The save-time support pass below supplies
	// only the required neighbours first.  If a chunk still cannot complete, it
	// is serialized with its incomplete state so population can continue later.
	void finishPostProcess() {
		size_t initialPending = 0;
		std::vector<ChunkPos> pendingPopulation;
		for (auto& entry : chunks) {
			LevelChunk* chunk = entry.second;
			if (chunk != NULL && chunk != emptyChunk && !chunk->terrainPopulated) {
				++initialPending;
				pendingPopulation.push_back(ChunkPos(chunk->x, chunk->z));
			}
		}

		// Infinite worlds load chunks on demand.  A chunk at the loading edge can
		// therefore be resident while the three +X/+Z neighbours required by the
		// original population order are still absent.  In that state postProcess()
		// cannot run, so snow/foliage written by the population pass may be missing
		// when the world is saved.  The population pass itself can also write into
		// the first +X/+Z neighbour layer.  At save time only, create a bounded
		// two-chunk-deep +X/+Z support area so that both the original chunk and its
		// directly modified neighbour chunks can finish postProcess().  This does
		// not enlarge the normal runtime loading radius and the ready-loop below
		// still controls the order in which population is completed.
		if (level->isInfinite()) {
			for (size_t i = 0; i < pendingPopulation.size(); ++i) {
				const ChunkPos& pos = pendingPopulation[i];
				for (int dx = 0; dx <= 2; ++dx) {
					for (int dz = 0; dz <= 2; ++dz) {
						if (dx == 0 && dz == 0)
							continue;
						getChunk(pos.x + dx, pos.z + dz);
					}
				}
			}
		}

		size_t processed = 0;
		while (true) {
			// Collect keys first because postProcess() can generate a neighbour
			// and mutate the cache while the pass is running.
			std::vector<int64_t> ready;
			for (auto& entry : chunks) {
				LevelChunk* chunk = entry.second;
				if (chunk == NULL || chunk == emptyChunk || chunk->terrainPopulated)
					continue;
				if (hasChunk(chunk->x + 1, chunk->z + 1) &&
					hasChunk(chunk->x, chunk->z + 1) &&
					hasChunk(chunk->x + 1, chunk->z))
					ready.push_back(entry.first);
			}
			if (ready.empty())
				break;

			for (size_t i = 0; i < ready.size(); ++i) {
				auto entry = chunks.find(ready[i]);
				if (entry == chunks.end() || entry->second == NULL ||
					entry->second == emptyChunk || entry->second->terrainPopulated)
					continue;
				LevelChunk* chunk = entry->second;
				postProcess(this, chunk->x, chunk->z);
				++processed;
			}
		}

		size_t remaining = 0;
		for (auto& entry : chunks) {
			LevelChunk* chunk = entry.second;
			if (chunk != NULL && chunk != emptyChunk && !chunk->terrainPopulated)
				++remaining;
		}
		LOGI("ChunkCache finishPostProcess: initial=%u processed=%u remaining=%u\n",
			static_cast<unsigned int>(initialPending),
			static_cast<unsigned int>(processed),
			static_cast<unsigned int>(remaining));
	}

	// Requeue a bounded 3x3 area around the player. Each vertical slice is
	// deliberately small so the existing 500-update budget can consume the
	// repair over several frames instead of blocking on one full-chunk pass.
	void queueLightUpdatesAround(int chunkX, int chunkZ) override {
		for (int dx = -1; dx <= 1; ++dx)
			for (int dz = -1; dz <= 1; ++dz)
				queueLightUpdatesForChunk(chunkX + dx, chunkZ + dz);
	}

    //bool save(bool force, ProgressListener progressListener) {
    //    int saves = 0;
    //    int count = 0;
    //    if (progressListener != NULL) {
    //        for (int i = 0; i < chunks.length; i++) {
    //            if (chunks[i] != NULL && chunks[i].shouldSave(force)) {
    //                count++;
    //            }
    //        }
    //    }
    //    int cc = 0;
    //    for (int i = 0; i < chunks.length; i++) {
    //        if (chunks[i] != NULL) {
    //            if (force && !chunks[i].dontSave) saveEntities(chunks[i]);
    //            if (chunks[i].shouldSave(force)) {
    //                save(chunks[i]);
    //                chunks[i].unsaved = false;
    //                if (++saves == MAX_SAVES && !force) return false;
    //                if (progressListener != NULL) {
    //                    if (++cc % 10 == 0) {
    //                        progressListener.progressStagePercentage(cc * 100 / count);
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    if (force) {
    //        if (storage == NULL) return true;
    //        storage.flush();
    //    }
    //    return true;
    //}

    bool tick() {
        if (storage != NULL) storage->tick();
        return source->tick();
    }

    bool shouldSave() {
        return true;
    }

    std::string gatherStats() {
        return "ChunkCache: 1024";
    }
	
	void saveAll(bool onlyUnsaved) {
		finishPostProcess();
		if (storage != NULL) {
			std::vector<LevelChunk*> chunkList;
			for (auto& kv : chunks) {
				LevelChunk* chunk = kv.second;
				#if defined(_DEBUG)
				bool hasChest = false;
				if (chunk != NULL) {
					const LevelChunk::TEMap& tileEntities = chunk->getTileEntityMap();
					for (LevelChunk::TEMapCIterator te = tileEntities.begin();
						te != tileEntities.end(); ++te) {
						if (te->second != NULL && te->second->type == TileEntityType::Chest) {
							hasChest = true;
							break;
						}
					}
				}
				#endif
				// A population pass can write snow/foliage into a neighbouring chunk
				// before that chunk has completed its own postProcess().  Such a chunk
				// must still be serialized: the UWP chunk record now carries its
				// terrainPopulated state so it can continue population after reload.
				// A newly generated chunk may have had its unsaved bit moved into
				// ExternalFileLevelStorage's delayed queue already.  Include it in
				// an explicit shutdown save until one successful serialization has
				// recorded that it is persisted.
				if (chunk && chunk != emptyChunk &&
					(chunk->terrainPopulated || level->isInfinite()))
					if (!onlyUnsaved || chunk->shouldSave(false) || !chunk->createdFromSave)
						chunkList.push_back(chunk);
				#if defined(_DEBUG)
				if (hasChest)
					LOGI("[SAVE_ALL] chunk=%d,%d candidate=%d onlyUnsaved=%d unsaved=%d createdFromSave=%d terrainPopulated=%d\n",
						chunk->x, chunk->z,
						(chunk && chunk != emptyChunk &&
							(chunk->terrainPopulated || level->isInfinite()) &&
							(!onlyUnsaved || chunk->shouldSave(false) || !chunk->createdFromSave)) ? 1 : 0,
						onlyUnsaved ? 1 : 0, chunk->unsaved ? 1 : 0,
						chunk->createdFromSave ? 1 : 0,
						chunk->terrainPopulated ? 1 : 0);
				#endif
			}
			LOGI("ChunkCache saveAll: onlyUnsaved=%d loaded=%u candidates=%u\n",
				onlyUnsaved ? 1 : 0,
				static_cast<unsigned int>(chunks.size()),
				static_cast<unsigned int>(chunkList.size()));
			storage->saveAll(level, chunkList);
		}
	}
private:
	void queueLightUpdatesForChunk(int x, int z) {
		if (!fits(x, z)) return;

		auto it = chunks.find(chunkKey(x, z));
		if (it == chunks.end() || it->second == NULL || it->second == emptyChunk)
			return;

		LevelChunk* chunk = it->second;

		const int x0 = x * CHUNK_WIDTH;
		const int z0 = z * CHUNK_WIDTH;
		const int x1 = x0 + CHUNK_WIDTH - 1;
		const int z1 = z0 + CHUNK_WIDTH - 1;
		const int lightSliceHeight = 8;
		for (int y0 = 0; y0 < Level::DEPTH; y0 += lightSliceHeight) {
			int y1 = y0 + lightSliceHeight - 1;
			if (y1 >= Level::DEPTH)
				y1 = Level::DEPTH - 1;

			level->updateLight(LightLayer::Sky, x0, y0, z0, x1, y1, z1);
			level->updateLight(LightLayer::Block, x0, y0, z0, x1, y1, z1);
		}

		// Lighting propagation changes the serialized skyLight/blockLight arrays,
		// but LevelChunk::setBrightness() deliberately does not mark a chunk dirty
		// for every individual light cell.  Old chunks therefore used to be repaired
		// only in memory and reverted to their stale lighting after a reload.  The
		// bounded player-area repair is the right place to persist that repair once,
		// without turning normal lighting propagation into a continuous save stream.
		if (chunk->createdFromSave)
			chunk->markUnsaved();
	}

    LevelChunk* load(int x, int z) {
        if (storage == NULL) return NULL;
        LevelChunk* levelChunk = storage->load(level, x, z);
        if (levelChunk != NULL) {
            levelChunk->lastSaveTime = level->getTime();
        }
        return levelChunk;
    }

    void saveEntities(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            storage->saveEntities(level, levelChunk);
        //} catch (Error e) {
        //    e.printStackTrace();
        //}
    }

    void save(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            levelChunk->lastSaveTime = level->getTime();
            storage->save(level, levelChunk);
        //} catch (IOException e) {
        //    e.printStackTrace();
        //}
    }

public:
	int xLast;
    int zLast;
private:
    LevelChunk* emptyChunk;
    ChunkSource* source;
    ChunkStorage* storage;
    std::unordered_map<int64_t, LevelChunk*> chunks;
    Level* level;

    LevelChunk* last;

};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/

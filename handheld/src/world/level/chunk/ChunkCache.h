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
#include <unordered_set>
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

    // Storage maintenance must inspect the cache without calling getChunk(),
    // because getChunk() creates and generates a missing chunk.  The returned
    // pointers are borrowed and remain owned by this cache.
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
            if (newChunk == NULL) {
                if (source == NULL) newChunk = emptyChunk;
                else newChunk = source->getChunk(x, z);
            } else {
				updateLights = true;
            }
            chunks[key] = newChunk;
            newChunk->lightLava();

			if (updateLights) {
				for (int cx = 0; cx < 16; cx++) {
					for (int cz = 0; cz < 16; cz++) {
						int height = level->getHeightmap(cx + x * 16, cz + z * 16);
						for (int cy = height; cy >= 0; cy--) {
							level->updateLight(LightLayer::Sky, cx + x * 16, cy, cz + z * 16, cx + x * 16, cy, cz + z * 16);
							level->updateLight(LightLayer::Block, cx + x * 16 - 1, cy, cz + z * 16 - 1, cx + x * 16 + 1, cy, cz + z * 16 + 1);
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

	// Finish population for every resident chunk before a forced save.  The
	// renderer may keep a much larger ring than the simulation area, but a
	// visible edge chunk is still a real generated chunk: writing its base
	// terrain while leaving postProcess pending permanently loses trees, leaves
	// and snow because the legacy loader restores every serialized chunk as
	// terrainPopulated.  Do not call getChunk() while collecting; only chunks
	// already resident at the start of this flush are candidates.  A feature
	// can touch another resident chunk, so that chunk is included in the same
	// snapshot.  If a feature had to create one dirty support chunk just outside
	// the ring, allow one bounded follow-up pass for that chunk as well; this
	// preserves cross-boundary leaves/snow without allowing the save to chase an
	// unbounded chain of newly generated chunks.
	void finishPostProcess() {
		std::vector<int64_t> pending;
		std::unordered_set<int64_t> resident;
		for (auto& entry : chunks) {
			LevelChunk* chunk = entry.second;
			if (chunk == NULL || chunk == emptyChunk || chunk->terrainPopulated)
				continue;
			resident.insert(entry.first);
			pending.push_back(entry.first);
		}
		const size_t initialPending = pending.size();
		size_t cursor = 0;
		size_t processed = 0;
		while (cursor < pending.size()) {
			const int64_t key = pending[cursor++];
			auto entry = chunks.find(key);
			if (entry == chunks.end() || entry->second == NULL ||
				entry->second == emptyChunk || entry->second->terrainPopulated)
				continue;

			LevelChunk* chunk = entry->second;
			postProcess(this, chunk->x, chunk->z);
			++processed;
		}

		// Population can create a neighboring resident chunk while processing a
		// feature at the edge of the render ring.  A single follow-up scan is not
		// sufficient: that neighbor can in turn expose another edge chunk.  Walk
		// to a bounded fixed point, just like the original chunk source does, while
		// never revisiting a coordinate in this save operation.  The bound keeps a
		// malformed/corrupt source from turning shutdown into an infinite wait.
		std::unordered_set<int64_t> attempted;
		for (size_t i = 0; i < pending.size(); ++i)
			attempted.insert(pending[i]);
		size_t supportProcessed = 0;
		size_t supportRounds = 0;
		const size_t MAX_SUPPORT_ROUNDS = 8;
		while (supportRounds < MAX_SUPPORT_ROUNDS) {
			std::vector<int64_t> supportPending;
			for (auto& entry : chunks) {
				LevelChunk* chunk = entry.second;
				if (chunk == NULL || chunk == emptyChunk || chunk->terrainPopulated ||
					attempted.find(entry.first) != attempted.end())
					continue;
				supportPending.push_back(entry.first);
			}
			if (supportPending.empty())
				break;

			++supportRounds;
			for (size_t i = 0; i < supportPending.size(); ++i) {
				const int64_t key = supportPending[i];
				attempted.insert(key);
				auto entry = chunks.find(key);
				if (entry == chunks.end() || entry->second == NULL ||
					entry->second == emptyChunk || entry->second->terrainPopulated)
					continue;
				LevelChunk* chunk = entry->second;
				postProcess(this, chunk->x, chunk->z);
				++supportProcessed;
			}
		}

		size_t remaining = 0;
		for (auto& entry : chunks) {
			LevelChunk* chunk = entry.second;
			if (chunk != NULL && chunk != emptyChunk && !chunk->terrainPopulated &&
				resident.find(entry.first) != resident.end())
				++remaining;
		}
		size_t newIncomplete = 0;
		for (auto& entry : chunks) {
			LevelChunk* chunk = entry.second;
			if (chunk != NULL && chunk != emptyChunk && !chunk->terrainPopulated &&
				resident.find(entry.first) == resident.end())
				++newIncomplete;
		}
		LOGI("ChunkCache finishPostProcess: resident=%u initial=%u processed=%u support=%u rounds=%u remaining=%u newIncomplete=%u\n",
			static_cast<unsigned int>(resident.size()),
			static_cast<unsigned int>(initialPending),
			static_cast<unsigned int>(processed),
			static_cast<unsigned int>(supportProcessed),
			static_cast<unsigned int>(supportRounds),
			static_cast<unsigned int>(remaining),
			static_cast<unsigned int>(newIncomplete));
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
				// Never serialize base terrain whose population pass has not
				// completed.  The legacy chunk format restores loaded chunks as
				// terrainPopulated, so writing one here would permanently suppress
				// the later tree/snow/plant pass on the next load.
				if (chunk && chunk != emptyChunk && chunk->terrainPopulated)
					// A newly generated chunk may have had its unsaved bit moved into
					// ExternalFileLevelStorage's delayed queue already.  Include it in
					// an explicit shutdown save until one successful serialization has
					// recorded that it is persisted.
					if (!onlyUnsaved || chunk->shouldSave(false) || !chunk->createdFromSave)
						chunkList.push_back(chunk);
				#if defined(_DEBUG)
				if (hasChest)
					LOGI("[SAVE_ALL] chunk=%d,%d candidate=%d onlyUnsaved=%d unsaved=%d createdFromSave=%d terrainPopulated=%d\n",
						chunk->x, chunk->z,
						(chunk && chunk != emptyChunk && chunk->terrainPopulated &&
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

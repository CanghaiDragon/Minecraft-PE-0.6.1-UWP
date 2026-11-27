#include "TileInclude.h"
#include "../Level.h"
#include "../../Facing.h"
#include "../../entity/player/Player.h"
#include "../../entity/item/ItemEntity.h"
#include "../../item/Item.h"
#include "../../item/TileItem.h"

#include "../../../util/Random.h"
//#include "locale/Descriptive.h"
//#include "stats/Stats.h"
#include "../../entity/Entity.h"
#include "../LevelSource.h"
#include "../material/Material.h"
#include "../../phys/AABB.h"
#include "../../phys/HitResult.h"
#include "../../phys/Vec3.h"
#include "../../../locale/I18n.h"
#include "../../item/ClothTileItem.h"

#include "../../item/AuxDataTileItem.h"
#include "../../item/LeafTileItem.h"
#include "../../item/StoneSlabTileItem.h"
#include "../../item/SaplingTileItem.h"
#include "../../item/ItemCategory.h"
#include "../../item/TilePlanterItem.h"

// The 0.8 crops share their growth and farmland rules. Their atlas has four
// visible stages, while the saved metadata still uses the 0-7 growth range.
class LegacyCropVisualTile : public CropTile {
public:
	LegacyCropVisualTile(int id, int tex, int resultItemId, int seedItemId)
		: CropTile(id, tex), baseTexture(tex), resultItemId(resultItemId), seedItemId(seedItemId) {}
	int getTexture(int face, int data) {
		int stage = (data & 7) >> 1;
		return baseTexture + stage;
	}
	int getTexture(LevelSource* level, int x, int y, int z, int face) {
		return getTexture(face, level->getData(x, y, z));
	}
	int getResource(int data, Random* random) {
		return (data == 7) ? resultItemId : -1;
	}
	int getResourceCount(Random* random) { return 1; }
	void spawnResources(Level* level, int x, int y, int z, int data, float odds) {
		Tile::spawnResources(level, x, y, z, data, odds);
		if (level->isClientSide || seedItemId <= 0 || Item::items[seedItemId] == NULL) return;
		for (int i = 0; i < 3; ++i) {
			if (level->random.nextInt(15) > data) continue;
			float offset = 0.15f;
			float span = 0.7f;
			ItemEntity* item = new ItemEntity(level,
				x + level->random.nextFloat() * span + offset,
				y + level->random.nextFloat() * span + offset,
				z + level->random.nextFloat() * span + offset,
				ItemInstance(Item::items[seedItemId]));
			item->throwTime = 10;
			level->addEntity(item);
		}
	}
private:
	int baseTexture;
	int resultItemId;
	int seedItemId;
};

class LegacyMetadataTile : public Tile {
public:
	LegacyMetadataTile(int id, int baseTex, const Material* material)
		: Tile(id, baseTex, material), baseTexture(baseTex) {}
	int getTexture(int face, int data) { return baseTexture + (data & 3); }
protected:
	int getSpawnResourcesAuxValue(int data) { return data & 3; }
private:
	int baseTexture;
};

class LegacyWoodSlabTile : public StoneSlabTile {
public:
	LegacyWoodSlabTile(int id, bool full) : StoneSlabTile(id, full) {}
	int getTexture(int face, int data) { return 303 + (data & 3); }
};

class LegacyWoodStairTile : public StairTile {
public:
	LegacyWoodStairTile(int id, Tile* base, int tex) : StairTile(id, base), texture(tex) {}
	int getTexture(int face, int data) { return texture; }
	int getTexture(int face) { return texture; }
	int getTexture(LevelSource* level, int x, int y, int z, int face) { return texture; }
private:
	int texture;
};

class LegacyCakeVisualTile : public Tile {
public:
	LegacyCakeVisualTile(int id) : Tile(id, 287, Material::cake) {
		setShape(0.0625f, 0, 0.0625f, 0.9375f, 0.5f, 0.9375f);
	}
	int getTexture(int face, int data) {
		if (face == Facing::UP) return 287;
		if (face == Facing::DOWN) return 285;
		return (face == Facing::NORTH && (data & 0x7) != 0) ? 286 : 288;
	}
	bool isSolidRender() { return false; }
	bool isCubeShaped() { return false; }
};

class LegacyPumpkinVisualTile : public Tile {
public:
	LegacyPumpkinVisualTile(int id, bool lit) : Tile(id, lit ? 292 : 289, Material::wood), lit(lit) {}
	bool mayPlace(Level* level, int x, int y, int z) {
		int existing = level->getTile(x, y, z);
		if (existing != 0 && !Tile::tiles[existing]->material->isReplaceable()) return false;
		return level->isSolidBlockingTile(x, y - 1, z);
	}
	void setPlacedBy(Level* level, int x, int y, int z, Mob* by) {
		if (by == NULL) return;
		float facing = by->getYRot() * 4.0f / 360.0f + 2.5f;
		int data = (int)facing;
		if (facing < data) --data;
		level->setData(x, y, z, data & 3);
	}
	int getTexture(int face, int data) {
		// The 0.8 atlas stores the stemmed top, ribbed side, unlit face,
		// and lit face in separate cells.
		if (face == Facing::UP || face == Facing::DOWN) return 289;
		// Both pumpkin variants have a directional face. The lit variant uses
		// the brighter face cell; ordinary pumpkins use the unlit face cell.
		// Legacy metadata order: 0=south, 1=west, 2=north, 3=east.
		int front = Facing::SOUTH;
		switch (data & 3) {
		case 1: front = Facing::WEST; break;
		case 2: front = Facing::NORTH; break;
		case 3: front = Facing::EAST; break;
		}
		if (face == front) return lit ? 292 : 291;
		return 290;
	}
	int getTexture(LevelSource* level, int x, int y, int z, int face) {
		return getTexture(face, level->getData(x, y, z));
	}
private:
	bool lit;
};

class LegacyRailVisualTile : public Tile {
public:
	LegacyRailVisualTile(int id, bool powered) : Tile(id, powered ? 297 : 295, Material::decoration), powered(powered) {
		setShape(0, 0, 0, 1, 0.0625f, 1);
	}
	int getRenderShape() { return SHAPE_RAIL; }
	void updateShape(LevelSource* level, int x, int y, int z) {
		int data = level->getData(x, y, z);
		int path = powered ? (data & 0x7) : data;
		setShape(0, 0, 0, 1, (path >= 2 && path <= 5) ? 0.625f : 0.125f, 1);
	}
	int getTexture(int face, int data) {
		// In the 0.8 source, every golden rail supplies minecart acceleration;
		// render it in the active state even when no redstone bit is stored.
		if (powered) return 298;
		// Normal rails use metadata 6-9 for the four curves. Unlike powered
		// rails, those values are not a direction field plus a power bit.
		return data >= 6 ? 296 : 295;
	}
	// Rails are only visual and must not collide with minecart bounding boxes.
	// The default Tile::getAABB uses setShape's thin top surface; that overlaps a
	// cart seated on the rail and Entity::move then clears its horizontal motion.
	AABB* getAABB(Level*, int, int, int) { return NULL; }
	bool isSolidRender() { return false; }
	bool isCubeShaped() { return false; }
	bool mayPlace(Level* level, int x, int y, int z) {
		return level->isSolidBlockingTile(x, y - 1, z);
	}
	void onPlace(Level* level, int x, int y, int z) { updateNearbyRails(level, x, y, z); }
	void neighborChanged(Level* level, int x, int y, int z, int) {
		if (!level->isSolidBlockingTile(x, y - 1, z)) {
			level->setTile(x, y, z, 0);
			return;
		}
		updateNearbyRails(level, x, y, z);
	}
private:
	static bool railAt(Level* level, int x, int y, int z) {
		int id = level->getTile(x, y, z);
		return (Tile::railCompat && id == Tile::railCompat->id) ||
			(Tile::goldenRailCompat && id == Tile::goldenRailCompat->id);
	}
	static bool hasRail(Level* level, int x, int y, int z) {
		return railAt(level, x, y, z) || railAt(level, x, y + 1, z) || railAt(level, x, y - 1, z);
	}
	static void updateNearbyRails(Level* level, int x, int y, int z) {
		// A rail can meet a ramp diagonally in x/y or z/y. Refresh the same-level
		// neighbors plus one block above and below so placement order does not
		// leave the first rail in a straight, stale shape.
		for (int dx = -1; dx <= 1; ++dx) for (int dz = -1; dz <= 1; ++dz) {
			if (dx != 0 && dz != 0) continue;
			for (int dy = -1; dy <= 1; ++dy) {
				int rx = x + dx, ry = y + dy, rz = z + dz;
				if (!railAt(level, rx, ry, rz)) continue;
				int railId = level->getTile(rx, ry, rz);
				updateRail(level, rx, ry, rz,
					Tile::goldenRailCompat && railId == Tile::goldenRailCompat->id);
			}
		}
	}
	static void updateRail(Level* level, int x, int y, int z, bool poweredRail) {
		bool n = hasRail(level, x, y, z - 1), s = hasRail(level, x, y, z + 1);
		bool w = hasRail(level, x - 1, y, z), e = hasRail(level, x + 1, y, z);
		int data = level->getData(x, y, z);
		int shape = poweredRail ? (data & 0x8) : 0; // normal rails use 6-9 for curves
		int path = 0;
		// Match BaseRailTile::Rail::place in 0.8: unpowered rails choose a
		// corner when exactly two adjacent directions connect.
		if (!poweredRail && s && e && !n && !w) path = 6;
		else if (!poweredRail && s && w && !n && !e) path = 7;
		else if (!poweredRail && n && w && !s && !e) path = 8;
		else if (!poweredRail && n && e && !s && !w) path = 9;
		// The elevation check must precede straight-track selection. A ramp has
		// an ordinary neighbor at its low end and a neighbor one block higher;
		// treating both as a plain east/west or north/south line erased the slope.
		else if (e && railAt(level, x + 1, y + 1, z)) path = 2;
		else if (w && railAt(level, x - 1, y + 1, z)) path = 3;
		else if (n && railAt(level, x, y + 1, z - 1)) path = 4;
		else if (s && railAt(level, x, y + 1, z + 1)) path = 5;
		else if (n && s && !w && !e) path = 0;
		else if (w && e && !n && !s) path = 1;
		else if (n || s) path = 0;
		else if (w || e) path = 1;
		int current = level->getData(x, y, z);
		int value = path | shape;
		if ((current & 0xF) != value && level->setDataNoUpdate(x, y, z, value))
			level->setTileDirty(x, y, z);
	}
	bool powered;
};

class LegacyPumpkinStemVisualTile : public StemTile {
public:
	LegacyPumpkinStemVisualTile(int id, Tile* pumpkin) : StemTile(id, pumpkin) {}
	void spawnResources(Level* level, int x, int y, int z, int data, float odds) {
		Tile::spawnResources(level, x, y, z, data, odds);
		if (level->isClientSide || Item::items[361] == NULL) return;
		for (int i = 0; i < 3; ++i) {
			if (level->random.nextInt(15) > data) continue;
			popResource(level, x, y, z, ItemInstance(Item::items[361]));
		}
	}
};

class LegacyDeadBushTile : public TallGrass {
public:
	LegacyDeadBushTile(int id, int tex) : TallGrass(id, tex) {}
	int getTexture(int face, int data) { return 316; }
};

class LegacyTallGrassTile : public TallGrass {
public:
	LegacyTallGrassTile(int id, int tex) : TallGrass(id, tex) {}
	int getTexture(int face, int data) {
		if (data == DEAD_SHRUB) return 316;
		// PE 0.8 stores fern as metadata 2 (the Java-era class constant is 3).
		if (data == 2 || data == FERN) return 319;
		return 318;
	}
};

class LegacyHayBlockTile : public Tile {
public:
	LegacyHayBlockTile(int id) : Tile(id, 265, Material::wood) {}
	int getTexture(int face, int data) {
		return (face == Facing::UP || face == Facing::DOWN) ? 265 : 266;
	}
	int getTexture(LevelSource* level, int x, int y, int z, int face) {
		return getTexture(face, level->getData(x, y, z));
	}
};

class LegacyCobbleWallTile : public FenceTile {
public:
	LegacyCobbleWallTile(int id) : FenceTile(id, 16, Material::stone) {}
	int getTexture(int face, int data) {
		return (data & 1) ? 36 : 16;
	}
	int getSpawnResourcesAuxValue(int data) { return data & 1; }
};

// Carpet metadata is already the wool color index in PE 0.8.
class LegacyCarpetVisualTile : public Tile {
public:
	LegacyCarpetVisualTile(int id) : Tile(id, 268, Material::cloth) {
		setShape(0, 0, 0, 1, 1.0f / 16.0f, 1);
	}
	int getTexture(int face, int data) { return 268 + (data & 15); }
	bool isSolidRender() { return false; }
	bool isCubeShaped() { return false; }
protected:
	int getSpawnResourcesAuxValue(int data) { return data & 15; }
};

class LegacyCarpetTileItem : public TileItem {
public:
	LegacyCarpetTileItem(int id) : TileItem(id) {
		setMaxDamage(0);
		setStackedByData(true);
	}
	int getIcon(int auxValue) {
		return Tile::carpetCompat->getTexture(2, auxValue);
	}
	int getLevelDataForAuxValue(int auxValue) { return auxValue; }
};

const int Tile::RENDERLAYER_OPAQUE          = 0;
const int Tile::RENDERLAYER_ALPHATEST       = 1;
const int Tile::RENDERLAYER_BLEND           = 2;

const std::string Tile::TILE_DESCRIPTION_PREFIX("tile.");

const Tile::SoundType Tile::SOUND_NORMAL("stone", 1, 1);
const Tile::SoundType Tile::SOUND_WOOD("wood", 1, 1);
const Tile::SoundType Tile::SOUND_GRAVEL("gravel", 1, 1);
const Tile::SoundType Tile::SOUND_GRASS("grass", 0.5f, 1);
const Tile::SoundType Tile::SOUND_STONE("stone", 1, 1);
const Tile::SoundType Tile::SOUND_METAL("stone", 1, 1.5f);
const Tile::SoundType Tile::SOUND_GLASS("stone", "random.glass", 1, 1);
const Tile::SoundType Tile::SOUND_CLOTH("cloth", 1, 1);

#ifdef PRE_ANDROID23
	const Tile::SoundType Tile::SOUND_SAND("sand", 0.45f, 1);
#else
	const Tile::SoundType Tile::SOUND_SAND("sand", "step.gravel", 1, 1);
#endif

const Tile::SoundType Tile::SOUND_SILENT("", 0, 0);

Tile* Tile::tiles[] = {NULL};
int Tile::lightBlock[] = {0};
int Tile::lightEmission[] = {0};
bool Tile::solid[] = {false};
bool Tile::isEntityTile[] = {false};
bool Tile::translucent[] = {true, false}; // @trans: translucent, @trans "asbMax", some more like "*conditon"
bool Tile::shouldTick[] = {false};
bool Tile::sendTileData[] = {false};
bool Tile::betaVisuals = false;
bool Tile::betaFancyGraphics = false;
bool Tile::alphaTextures = false;

Tile* Tile::sand        = NULL;
Tile* Tile::sandStone   = NULL;
const int SANDSTONE_TEXTURES[] = { 0 + 16 * 12, 5 + 16 * 14, 6 + 16 * 14 };
const int SANDSTONE_TEXTURE_COUNT = 3;

Tile* Tile::stoneBrick  = NULL;
Tile* Tile::redBrick    = NULL;
Tile* Tile::wood        = NULL;
Tile* Tile::sapling     = NULL;
Tile* Tile::glass       = NULL;
Tile* Tile::web	        = NULL;
Tile* Tile::thinGlass   = NULL;
Tile* Tile::calmWater   = NULL;
Tile* Tile::calmLava    = NULL;
Tile* Tile::gravel      = NULL;
Tile* Tile::rock        = NULL;
Tile* Tile::unbreakable = NULL;
Tile* Tile::dirt        = NULL;
Tile* Tile::grass       = NULL;
Tile* Tile::ice         = NULL;
Tile* Tile::clay        = NULL;
Tile* Tile::farmland    = NULL;
Tile* Tile::stoneSlab   = NULL;
Tile* Tile::stoneSlabHalf=NULL;
Tile* Tile::sponge = NULL;
Tile* Tile::goldenRailCompat = NULL;
Tile* Tile::deadBushCompat = NULL;
Tile* Tile::unused_28 = NULL;
Tile* Tile::unused_29 = NULL;
Tile* Tile::unused_30 = NULL;
Tile* Tile::unused_33 = NULL;
Tile* Tile::unused_34 = NULL;
Tile* Tile::unused_36 = NULL;
Tile* Tile::cloth       = NULL;
Tile* Tile::flower      = NULL;
Tile* Tile::rose        = NULL;
Tile* Tile::mushroom1   = NULL;
Tile* Tile::mushroom2   = NULL;
Tile* Tile::topSnow     = NULL;
Tile* Tile::treeTrunk   = NULL;
Tile* Tile::snow        = NULL;
LeafTile* Tile::leaves  = NULL;
Tile* Tile::emeraldOre  = NULL;
Tile* Tile::redStoneOre = NULL;
Tile* Tile::redStoneOre_lit = NULL;
Tile* Tile::goldOre     = NULL;
Tile* Tile::ironOre     = NULL;
Tile* Tile::coalOre     = NULL;
Tile* Tile::lapisOre    = NULL;
Tile* Tile::lapisBlock	= NULL;
Tile* Tile::reeds       = NULL;
Tile* Tile::ladder      = NULL;
Tile* Tile::obsidian    = NULL;
Tile* Tile::tnt         = NULL;
Tile* Tile::bookshelf	= NULL;
Tile* Tile::sign        = NULL;
Tile* Tile::wallSign    = NULL;
Tile* Tile::mossStone	= NULL;
Tile* Tile::torch       = NULL;
Tile* Tile::water       = NULL;
Tile* Tile::lava        = NULL;
FireTile* Tile::fire    = NULL;
Tile* Tile::invisible_bedrock = NULL;
Tile* Tile::goldBlock   = NULL;
Tile* Tile::ironBlock   = NULL;
Tile* Tile::emeraldBlock= NULL;
Tile* Tile::workBench   = NULL;
Tile* Tile::stonecutterBench = NULL;
Tile* Tile::crops		= NULL;
Tile* Tile::furnace     = NULL;
Tile* Tile::furnace_lit = NULL;
Tile* Tile::chest		= NULL;
Tile* Tile::lightGem    = NULL;
Tile* Tile::stairs_wood = NULL;
Tile* Tile::stairs_stone= NULL;
Tile* Tile::stairs_brick= NULL;
Tile* Tile::door_wood   = NULL;
Tile* Tile::door_iron   = NULL;
Tile* Tile::cactus      = NULL;

Tile* Tile::melon       = NULL;
Tile* Tile::melonStem   = NULL;

Tile* Tile::bed         = NULL;
Tile* Tile::tallgrass   = NULL;
Tile* Tile::trapdoor    = NULL;
Tile* Tile::stoneBrickSmooth = NULL;
const int STONE_BRICK_TEXTURES[] = { 6 + 16 * 3, 4 + 16 * 6, 5 + 16 * 6 };
const int STONE_BRICK_TEXTURE_COUNT = 3;

Tile* Tile::fence       = NULL;
Tile* Tile::fenceGate   = NULL;

Tile* Tile::info_updateGame1 = NULL;
Tile* Tile::info_updateGame2 = NULL;
Tile* Tile::info_reserved6   = NULL;
Tile* Tile::grass_carried    = NULL;
LeafTile* Tile::leaves_carried   = NULL;

Tile* Tile::netherReactor = NULL;
Tile* Tile::glowingObsidian = NULL;

Tile* Tile::stairs_stoneBrickSmooth   = NULL;
Tile* Tile::netherBrick   = NULL;
Tile* Tile::netherrack   = NULL;
Tile* Tile::stairs_netherBricks   = NULL;
Tile* Tile::stairs_sandStone   = NULL;
Tile* Tile::quartzBlock   = NULL;
Tile* Tile::stairs_quartz   = NULL;
Tile* Tile::beetrootCompat = NULL;
Tile* Tile::pumpkinStemCompat = NULL;
Tile* Tile::railCompat = NULL;
Tile* Tile::pumpkinCompat = NULL;
Tile* Tile::litPumpkinCompat = NULL;
Tile* Tile::cakeCompat = NULL;
Tile* Tile::woodSlabCompat = NULL;
Tile* Tile::doubleWoodSlabCompat = NULL;
Tile* Tile::stairsSpruceCompat = NULL;
Tile* Tile::stairsBirchCompat = NULL;
Tile* Tile::stairsJungleCompat = NULL;
Tile* Tile::ironBarsCompat = NULL;
Tile* Tile::cobbleWallCompat = NULL;
Tile* Tile::carrotsCompat = NULL;
Tile* Tile::potatoesCompat = NULL;
Tile* Tile::hayBlockCompat = NULL;
Tile* Tile::carpetCompat = NULL;
Tile* Tile::coalBlockCompat = NULL;

/*static*/
void Tile::initTiles() {
	rock        = (new StoneTile(1, 1))->init()->setDestroyTime(1.5f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stone");
	grass       = (GrassTile*) (new GrassTile(2))->init()->setDestroyTime(0.6f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("grass");
	dirt        = (new DirtTile(3, 2))->init()->setDestroyTime(0.5f)->setSoundType(SOUND_GRAVEL)->setCategory(ItemCategory::Structures)->setDescriptionId("dirt");
	stoneBrick  = (new CobblestoneTile(4, 16))->init()->setDestroyTime(2.0f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stonebrick");
	wood        = (new LegacyMetadataTile(5, 303, Material::wood))->init()->setDestroyTime(2.0f)->setExplodeable(5)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("wood");
	sapling     = (new Sapling(6, 15))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("sapling");//->sendTileData();
	unbreakable = (new Tile(7, 17, Material::stone))->init()->setDestroyTime(-1)->setExplodeable(6000000)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("bedrock");
	water       = (new LiquidTileDynamic(8, Material::water))->init()->setDestroyTime(100.0f)->setLightBlock(3)->setCategory(ItemCategory::Structures)->setDescriptionId("water");
	calmWater   = (new LiquidTileStatic(9, Material::water))->init()->setDestroyTime(100.0f)->setLightBlock(3)->setCategory(ItemCategory::Structures)->setDescriptionId("water");
	lava        = (new LiquidTileDynamic(10, Material::lava))->init()->setDestroyTime(00.0f)->setLightEmission(1.0f)->setLightBlock(255)->setCategory(ItemCategory::Structures)->setDescriptionId("lava"); // 00.0?
	calmLava    = (new LiquidTileStatic(11, Material::lava))->init()->setDestroyTime(100.0f)->setLightEmission(1.0f)->setLightBlock(255)->setCategory(ItemCategory::Structures)->setDescriptionId("lava");
	sand        = (new HeavyTile(12, 18))->init()->setDestroyTime(0.5f)->setSoundType(SOUND_SAND)->setCategory(ItemCategory::Structures)->setDescriptionId("sand");
	gravel      = (new GravelTile(13, 19))->init()->setDestroyTime(0.6f)->setSoundType(SOUND_GRAVEL)->setCategory(ItemCategory::Structures)->setDescriptionId("gravel");
	goldOre     = (new OreTile(14, 32))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Decorations)->setDescriptionId("oreGold");
	ironOre     = (new OreTile(15, 33))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Decorations)->setDescriptionId("oreIron");
	coalOre     = (new OreTile(16, 34))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("oreCoal");
	treeTrunk   = (new TreeTile(17))->init()->setDestroyTime(2.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("log");
	leaves      = (LeafTile*) (new LeafTile(18, 4 + 3 * 16))->init()->setDestroyTime(0.2f)->setLightBlock(1)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("leaves");

	glass       = (new GlassTile(20, 49, Material::glass, false))->init()->setDestroyTime(0.3f)->setSoundType(SOUND_GLASS)->setCategory(ItemCategory::Structures)->setDescriptionId("glass");
	lapisOre    = (new OreTile(21, 10 * 16))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("oreLapis");
	lapisBlock	= (new Tile(22, 9 * 16, Material::stone))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Decorations)->setDescriptionId("blockLapis");
	sandStone   = (new SandStoneTile(24, (const int*)&SANDSTONE_TEXTURES, SANDSTONE_TEXTURE_COUNT))->init()->setSoundType(SOUND_STONE)->setDestroyTime(0.8f)->setCategory(ItemCategory::Structures)->setDescriptionId("sandStone");
	bed         = (new BedTile(26))->init()->setDestroyTime(0.2f)->setCategory(ItemCategory::Structures)->setDescriptionId("bed");
	web	        = (new WebTile(30, 11))->init()->setLightBlock(1)->setDestroyTime(4.0f)->setCategory(ItemCategory::Decorations)->setDescriptionId("web");
	tallgrass   = (new LegacyTallGrassTile(31, 315))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("tallgrass");
	deadBushCompat = (new LegacyDeadBushTile(32, 315))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("deadbush");
	cloth       = (new ClothTile(35))->init()->setDestroyTime(0.8f)->setSoundType(SOUND_CLOTH)->setCategory(ItemCategory::Structures)->setDescriptionId("cloth");
	sponge      = (new Tile(19, 284, Material::sponge))->init()->setDestroyTime(0.6f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("sponge");

	flower      = (new Bush(37, 13))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("flower");
	rose        = (new Bush(38, 12))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("rose");
	mushroom1   = (new Mushroom(39, 13 + 16))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setLightEmission(2 / 16.0f)->setCategory(ItemCategory::Decorations)->setDescriptionId("mushroom");
	mushroom2   = (new Mushroom(40, 12 + 16))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("mushroom");
	goldBlock   = (new MetalTile(41, 39 - 16))->init()->setDestroyTime(3.0f)->setExplodeable(10)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Decorations)->setDescriptionId("blockGold");
	ironBlock   = (new MetalTile(42, 38 - 16))->init()->setDestroyTime(5.0f)->setExplodeable(10)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Decorations)->setDescriptionId("blockIron");
	stoneSlab   = (new StoneSlabTile(43, true))->init()->setDestroyTime(2.0f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stoneSlab");
	stoneSlabHalf=(new StoneSlabTile(44, false))->init()->setDestroyTime(2.0f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stoneSlab");
	redBrick    = (new Tile(45, 7, Material::stone))->init()->setDestroyTime(2.0f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("brick");
	tnt         = (new TntTile(46, 8))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Tools)->setDescriptionId("tnt");
	bookshelf	= (new BookshelfTile(47, 35))->init()->setDestroyTime(1.5f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Decorations)->setDescriptionId("bookshelf");
	mossStone	= (new Tile(48, 36, Material::stone))->init()->setDestroyTime(2.0f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stoneMoss");
	obsidian    = (new ObsidianTile(49, 37, false))->init()->setDestroyTime(10.0f)->setExplodeable(2000)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("obsidian");
	torch       = (new TorchTile(50, 5 * 16))->init()->setDestroyTime(0.0f)->setLightEmission(15 / 16.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Tools)->setDescriptionId("torch");

	stairs_wood = (new StairTile(53, wood))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsWood");
	// Pocket Edition 0.8 uses its own block IDs here (157/158); 125/126 are
	// Java Edition IDs and do not match the IDs stored in PE world chunks.
	doubleWoodSlabCompat = (new LegacyWoodSlabTile(157, true))->init()->setDestroyTime(2.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("woodSlab");
	woodSlabCompat = (new LegacyWoodSlabTile(158, false))->init()->setDestroyTime(2.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("woodSlab");
	stairsSpruceCompat = (new LegacyWoodStairTile(134, wood, 304))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsSpruce");
	stairsBirchCompat = (new LegacyWoodStairTile(135, wood, 305))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsBirch");
	stairsJungleCompat = (new LegacyWoodStairTile(136, wood, 306))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsJungle");
	chest		= (new ChestTile(54))->init()->setCategory(ItemCategory::Structures)->setDestroyTime(2.5f)->setSoundType(SOUND_WOOD)->setDescriptionId("chest");//->sendTileData();

	emeraldOre  = (new OreTile(56, 16 * 3 + 2))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Decorations)->setDescriptionId("oreDiamond");
	emeraldBlock= (new MetalTile(57, 40 - 16))->init()->setDestroyTime(5.0f)->setExplodeable(10)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Decorations)->setDescriptionId("blockDiamond");
	workBench   = (new WorkbenchTile(58))->init()->setDestroyTime(2.5f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("workbench");
	crops		= (new CropTile(59, 8 + 5 * 16))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("crops");//->sendTileData();
	farmland    = (new FarmTile(60))->init()->setDestroyTime(0.6f)->setSoundType(SOUND_GRAVEL)->setCategory(ItemCategory::Structures)->setDescriptionId("farmland");
	furnace		= (new FurnaceTile(61, false))->init()->setDestroyTime(3.5f)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("furnace");//.sendTileData();
	furnace_lit = (new FurnaceTile(62, true))->init()->setDestroyTime(3.5f)->setSoundType(SOUND_STONE)->setLightEmission(14 / 16.0f)->setCategory(ItemCategory::Structures)->setDescriptionId("furnace");//.sendTileData();
    sign        = (new SignTile(63, TileEntityType::Sign, true))->init()->setDestroyTime(1.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Decorations)->setDescriptionId("sign");//->sendTileData();
	door_wood   = (new DoorTile(64, Material::wood))->init()->setDestroyTime(3.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("doorWood");
	ladder      = (new LadderTile(65, 3 + 5 * 16))->init()->setDestroyTime(0.4f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("ladder");

	stairs_stone= (new StairTile(67, stoneBrick))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsStone");
    wallSign    = (new SignTile(68, TileEntityType::Sign, false))->init()->setDestroyTime(1.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Decorations)->setDescriptionId("sign");//->sendTileData();

	door_iron   = (new DoorTile(71, Material::metal))->init()->setDestroyTime(5.0f)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Structures)->setDescriptionId("doorIron");

	redStoneOre = (new RedStoneOreTile(73, 16 * 3 + 3, false))->init()->setDestroyTime(3.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Mechanisms)->setDescriptionId("oreRedstone");
	redStoneOre_lit = (new RedStoneOreTile(74, 16 * 3 + 3, true))->init()->setDestroyTime(3.0f)->setLightEmission(10 / 16.0f)->setExplodeable(5)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Mechanisms)->setDescriptionId("oreRedstone");

	topSnow     = (new TopSnowTile(78, 16 * 4 + 2))->init()->setDestroyTime(0.1f)->setSoundType(SOUND_CLOTH)->setCategory(ItemCategory::Structures)->setDescriptionId("snow");
	ice         = (new IceTile(79, 16 * 4 + 3))->init()->setDestroyTime(0.5f)->setLightBlock(3)->setSoundType(SOUND_GLASS)->setCategory(ItemCategory::Structures)->setDescriptionId("ice");
	snow		= (new SnowTile(80, 16 * 4 + 2))->init()->setDestroyTime(0.2f)->setSoundType(SOUND_CLOTH)->setCategory(ItemCategory::Structures)->setDescriptionId("snow");
    cactus      = (new CactusTile(81, 16 * 4 + 6))->init()->setDestroyTime(0.4f)->setSoundType(SOUND_CLOTH)->setCategory(ItemCategory::Structures)->setDescriptionId("cactus");
	clay        = (new ClayTile(82, 16 * 4 + 8))->init()->setDestroyTime(0.6f)->setSoundType(SOUND_GRAVEL)->setCategory(ItemCategory::Structures)->setDescriptionId("clay");
	reeds       = (new ReedTile(83, 16 * 4 + 9))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("reeds");

	fence		= (new FenceTile(85, 4))->init()->setDestroyTime(2.0f)->setExplodeable(5)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("fence");

	netherrack = (new Tile(87, 7 + 6 * 16, Material::stone))->init()->setDestroyTime(0.4f)->setSoundType(SOUND_STONE)->setDescriptionId("hellrock");
	//hellSand = (new HellSandTile(88, 8 + 6 * 16, Material::sand))->init()->setDestroyTime(0.5f)->setSoundType(SOUND_SAND)->setDescriptionId("hellsand");

	lightGem	= (new LightGemTile(89, 9 + 16 * 6, Material::glass))->init()->setDestroyTime(0.3f)->setCategory(ItemCategory::Structures)->setSoundType(SOUND_GLASS)->setLightEmission(1.0f)->setDescriptionId("lightgem");
	pumpkinCompat = (new LegacyPumpkinVisualTile(86, false))->init()->setDestroyTime(1.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("pumpkin");
	litPumpkinCompat = (new LegacyPumpkinVisualTile(91, true))->init()->setDestroyTime(1.0f)->setLightEmission(1.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("litpumpkin");
	cakeCompat = (new LegacyCakeVisualTile(92))->init()->setDestroyTime(0.5f)->setSoundType(SOUND_CLOTH)->setCategory(ItemCategory::FoodArmor)->setDescriptionId("cake");
	invisible_bedrock = (new InvisibleTile(95, 0, Material::stone))->init()->setDestroyTime(-1)->setExplodeable(6000000);//->setSoundType(SOUND_SILENT);
	trapdoor = (new TrapDoorTile(96, Material::wood))->init()->setDestroyTime(3.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("trapdoor");//->sendTileData();

	stoneBrickSmooth = (new MultiTextureTile(98, (const int*)&STONE_BRICK_TEXTURES, STONE_BRICK_TEXTURE_COUNT, Material::stone))->init()->setDestroyTime(1.5f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stonebricksmooth");

	thinGlass = (new ThinFenceTile(102, 1 + 3 * 16, 4 + 9 * 16, Material::glass, false))->init()->setDestroyTime(0.3f)->setSoundType(SOUND_GLASS)->setCategory(ItemCategory::Structures)->setDescriptionId("thinGlass");
	pumpkinStemCompat = (new LegacyPumpkinStemVisualTile(104, pumpkinCompat))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("pumpkinStem");
	melon = (new MelonTile(103))->init()->setDestroyTime(1.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::FoodArmor)->setDescriptionId("melon");
	melonStem = (new StemTile(105, Tile::melon))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::FoodArmor)->setDescriptionId("melonStem");//->sendTileData();
	railCompat = (new LegacyRailVisualTile(66, false))->init()->setDestroyTime(0.7f)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Mechanisms)->setDescriptionId("rail");
	goldenRailCompat = (new LegacyRailVisualTile(27, true))->init()->setDestroyTime(0.7f)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Mechanisms)->setDescriptionId("goldenRail");
	fenceGate = (new FenceGateTile(107, 4))->init()->setDestroyTime(2.0f)->setExplodeable(5)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("fenceGate");//->sendTileData();
	stairs_brick = (new StairTile(108, Tile::redBrick))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsBrick");//->sendTileData();

	stairs_stoneBrickSmooth = (new StairTile(109, Tile::stoneBrickSmooth))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsStoneBrickSmooth");//->sendTileData();
	netherBrick    = (new Tile(112, 0 + 14 * 16, Material::stone))->init()->setDestroyTime(2.0f)->setExplodeable(10)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("netherBrick");
	stairs_netherBricks = (new StairTile(114, Tile::netherBrick))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsNetherBrick");//->sendTileData();
	stairs_sandStone = (new StairTile(128, Tile::sandStone))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsSandStone");//->sendTileData();

	// MCPE 0.8 build 2+ block IDs. Their sprites use the extended lower half
	// of terrain.png so the original 0.6 atlas and legacy wool cells stay intact.
	ironBarsCompat = (new ThinFenceTile(101, 294, 294, Material::metal, true))->init()->setDestroyTime(5.0f)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Structures)->setDescriptionId("ironBars");
	cobbleWallCompat = (new LegacyCobbleWallTile(139))->init()->setDestroyTime(2.0f)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("cobbleWall");
	carrotsCompat = (new LegacyCropVisualTile(141, 257, 391, 391))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("carrots");
	potatoesCompat = (new LegacyCropVisualTile(142, 261, 392, 392))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("potatoes");
	hayBlockCompat = (new LegacyHayBlockTile(170))->init()->setDestroyTime(0.5f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("hayBlock");
	carpetCompat = (new LegacyCarpetVisualTile(171))->init()->setDestroyTime(0.1f)->setSoundType(SOUND_CLOTH)->setCategory(ItemCategory::Structures)->setDescriptionId("carpet");
	coalBlockCompat = (new Tile(173, 267, Material::stone))->init()->setDestroyTime(5.0f)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("blockCoal");
	beetrootCompat = (new LegacyCropVisualTile(244, 299, 457, 458))->init()->setDestroyTime(0.0f)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Decorations)->setDescriptionId("beetrootCrop");

	quartzBlock   = (new QuartzBlockTile(155))->init()->setSoundType(SOUND_STONE)->setDestroyTime(0.8f)->setCategory(ItemCategory::Structures)->setDescriptionId("quartzBlock");
	stairs_quartz = (new StairTile(156, Tile::quartzBlock))->init()->setCategory(ItemCategory::Structures)->setDescriptionId("stairsQuartz");//->sendTileData();

	//
	// Special tiles for Pocket Edition is placed at high IDs
	//
	stonecutterBench= (new StonecutterTile(245))->init()->setDestroyTime(2.5f)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("stonecutter");
	glowingObsidian = (new ObsidianTile(246, 10 + 16 * 13, true))->init()->setDestroyTime(10.0f)->setLightEmission(14 / 16.0f)->setExplodeable(2000)->setSoundType(SOUND_STONE)->setCategory(ItemCategory::Structures)->setDescriptionId("glowingobsidian");
	netherReactor	= (new NetherReactor(247, 10  + 14 * 16, Material::metal))->init()->setDestroyTime(3.0f)->setSoundType(SOUND_METAL)->setCategory(ItemCategory::Structures)->setDescriptionId("netherreactor");
	info_updateGame1= (new Tile(248, 252, Material::dirt))->init()->setDestroyTime(1.0f)->setSoundType(SOUND_GRAVEL)->setCategory(ItemCategory::Structures)->setDescriptionId("info_update");
	info_updateGame2= (new Tile(249, 253, Material::dirt))->init()->setDestroyTime(1.0f)->setSoundType(SOUND_GRAVEL)->setCategory(ItemCategory::Structures)->setDescriptionId("info_update");
	grass_carried   = (new CarriedTile(253, 3, 12*16 + 12))->init()->setDescriptionId("grass");
	leaves_carried  = (LeafTile*) (new LeafTile(254, 11 + 14 * 16))->init()->setDestroyTime(0.2f)->setLightBlock(1)->setSoundType(SOUND_GRASS)->setCategory(ItemCategory::Structures)->setDescriptionId("leaves");
	info_reserved6  = (new Tile(255, Material::dirt))->init();

    //
    // Stuff that need to be inited in a specific order (i.e. after the other tiles have been created)
    //
    fire     = (FireTile*) (new FireTile(51, 1 * 16 + 15))->init()->setDestroyTime(0.0f)->setLightEmission(1.0f)->setSoundType(SOUND_WOOD)->setCategory(ItemCategory::Structures)->setDescriptionId("fire");

	//
    // Special case for certain items since they can have different icons
	// @note: Make sure those different items are handled in ItemInHandRenderer::renderItem
	//
	Item::items[cloth->id] = (new ClothTileItem(cloth->id - 256))->setCategory(ItemCategory::Structures)->setDescriptionId("cloth");
	Item::items[treeTrunk->id] = (new AuxDataTileItem(treeTrunk->id - 256, treeTrunk))->setCategory(ItemCategory::Structures)->setDescriptionId("log");
	Item::items[stoneBrickSmooth->id] = (new AuxDataTileItem(stoneBrickSmooth->id - 256, stoneBrickSmooth))->setCategory(ItemCategory::Structures)->setDescriptionId("stonebricksmooth");
	Item::items[stoneSlabHalf->id] = (new StoneSlabTileItem(stoneSlabHalf->id - 256))->setCategory(ItemCategory::Structures)->setDescriptionId("stoneSlab");
	Item::items[sapling->id] = (new SaplingTileItem(sapling->id - 256))->setCategory(ItemCategory::Structures)->setDescriptionId("sapling");
	Item::items[leaves->id] = (new LeafTileItem(leaves->id - 256))->setCategory(ItemCategory::Decorations)->setDescriptionId("leaves");
	Item::items[sandStone->id] = (new AuxDataTileItem(sandStone->id - 256, sandStone))->setCategory(ItemCategory::Structures)->setDescriptionId("sandStone");
	Item::items[wood->id] = (new AuxDataTileItem(wood->id - 256, wood))->setCategory(ItemCategory::Structures)->setDescriptionId("wood");
	Item::items[doubleWoodSlabCompat->id] = (new AuxDataTileItem(doubleWoodSlabCompat->id - 256, doubleWoodSlabCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("woodSlab");
	Item::items[woodSlabCompat->id] = (new AuxDataTileItem(woodSlabCompat->id - 256, woodSlabCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("woodSlab");
	Item::items[deadBushCompat->id] = (new TileItem(deadBushCompat->id - 256))->setCategory(ItemCategory::Decorations)->setDescriptionId("deadbush");

	Item::items[quartzBlock->id] = (new AuxDataTileItem(quartzBlock->id - 256, quartzBlock))->setCategory(ItemCategory::Structures)->setDescriptionId("quartzBlock");
	Item::items[carpetCompat->id] = (new LegacyCarpetTileItem(carpetCompat->id - 256))->setCategory(ItemCategory::Structures);
	Item::items[beetrootCompat->id] = (new AuxDataTileItem(beetrootCompat->id - 256, beetrootCompat))->setCategory(ItemCategory::Decorations)->setDescriptionId("beetrootCrop");
	Item::items[cakeCompat->id] = (new TilePlanterItem(92, cakeCompat))->setMaxStackSize(1)->setIcon(13, 1)->setCategory(ItemCategory::FoodArmor)->setDescriptionId("cake");
	// These compatibility blocks use aux values for facing, rail shape, or
	// growth stage. Treat aux as block data, not item durability.
	Item::items[pumpkinCompat->id] = (new AuxDataTileItem(pumpkinCompat->id - 256, pumpkinCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("pumpkin");
	Item::items[litPumpkinCompat->id] = (new AuxDataTileItem(litPumpkinCompat->id - 256, litPumpkinCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("litpumpkin");
	Item::items[pumpkinStemCompat->id] = (new AuxDataTileItem(pumpkinStemCompat->id - 256, pumpkinStemCompat))->setCategory(ItemCategory::Decorations)->setDescriptionId("pumpkinStem");
	Item::items[railCompat->id] = (new AuxDataTileItem(railCompat->id - 256, railCompat))->setCategory(ItemCategory::Mechanisms)->setDescriptionId("rail");
	Item::items[goldenRailCompat->id] = (new AuxDataTileItem(goldenRailCompat->id - 256, goldenRailCompat))->setCategory(ItemCategory::Mechanisms)->setDescriptionId("goldenRail");
	Item::items[cobbleWallCompat->id] = (new AuxDataTileItem(cobbleWallCompat->id - 256, cobbleWallCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("cobbleWall");
	Item::items[carrotsCompat->id] = (new AuxDataTileItem(carrotsCompat->id - 256, carrotsCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("carrots");
	Item::items[potatoesCompat->id] = (new AuxDataTileItem(potatoesCompat->id - 256, potatoesCompat))->setCategory(ItemCategory::Structures)->setDescriptionId("potatoes");

    for (int i = 0; i < 256; i++) {
        if (Tile::tiles[i] != NULL) {
			if (Item::items[i] == NULL) {
				Item::items[i] = new TileItem(i - 256);
				Item::items[i]->category = Tile::tiles[i]->category;
			}
			// Check for missing category
			if (Item::items[i]->category == -1)
				LOGE("Error: Missing category for tile %d: %s\n", tiles[i]->id, tiles[i]->getDescriptionId().c_str());
        }
    }
}

/*static*/
void Tile::teardownTiles() {
	for (int i = 0; i < 256; ++i)
		if (Tile::tiles[i]) {
			delete Tile::tiles[i];
			Tile::tiles[i] = NULL;
		}
}

int Tile::transformToValidBlockId( int blockId ) {
	return transformToValidBlockId(blockId, 0, 0, 0);
}

int Tile::transformToValidBlockId( int blockId, int x, int y, int z ) {
	// Keep the PE 0.8 wooden slab IDs registered when older code paths rebuild
	// the tile table before loading saved chunks.
	if (blockId == 157 && doubleWoodSlabCompat != NULL && tiles[157] != doubleWoodSlabCompat)
		doubleWoodSlabCompat->init();
	if (blockId == 158 && woodSlabCompat != NULL && tiles[158] != woodSlabCompat)
		woodSlabCompat->init();
	if (blockId != 0 && Tile::tiles[blockId] == NULL)
		return (((x + y + z)&1) == 1)? Tile::info_updateGame1->id : Tile::info_updateGame2->id;
	return blockId;
}


Tile::Tile(int id, const Material* material)
:	id(id),
	material(material),
	tex(1),
	category(-1),
	gravity(1.0f),
	friction(0.6f),
	soundType(&Tile::SOUND_NORMAL),
	tmpBB(0,0,0,1,1,1),
	xx0(0),yy0(0),zz0(0),
	xx1(1),yy1(1),zz1(1)
{
	if (Tile::tiles[id]) {
		printf("Slot %d is already occupied by %p when adding %p\n", id, &Tile::tiles[id], this);
	}
}

Tile::Tile( int id, int tex, const Material* material )
:	id(id),
	tex(tex),
	material(material),
	category(-1),
	gravity(1.0f),
	friction(0.6f),
	soundType(&Tile::SOUND_NORMAL),
	tmpBB(0,0,0,1,1,1),
	xx0(0),yy0(0),zz0(0),
	xx1(1),yy1(1),zz1(1)
{
	if (Tile::tiles[id]) {
		printf("Slot %d is already occupied by %p when adding %p\n", id, &Tile::tiles[id], this);
	}
}

//Tile* sendTileData() {
//    Tile::sendTileData[id] = true;
//    return this;
//}

/*protected*/
Tile* Tile::setLightEmission(float f) {
    Tile::lightEmission[id] = (int) (Level::MAX_BRIGHTNESS * f);
    return this;
}

/*public static*/
bool Tile::isFaceVisible(Level* level, int x, int y, int z, int f) {
	switch (f) {
		case Facing::DOWN : y--; break;
		case Facing::UP   : y++; break;
		case Facing::NORTH: z--; break;
		case Facing::SOUTH: z++; break;
		case Facing::WEST : x--; break;
		case Facing::EAST : x++; break;
	}
	return !level->isSolidRenderTile(x, y, z);
}

/* private */
Tile* Tile::init() {
    Tile::tiles[id] = this;
	setShape(xx0, yy0, zz0, xx1, yy1, zz1); // @attn
	solid[id] = isSolidRender();
	lightBlock[id] = isSolidRender() ? 255 : 0;
	translucent[id] = !material->blocksLight();
	return this;
}

/*public virtual*/
float Tile::getDestroyProgress(Player* player) {
    if (destroySpeed < 0) return 0;
    if (!player->canDestroy(this)) return 1 / destroySpeed / 100.0f;
    return (player->getDestroySpeed(this) / destroySpeed) / 30.0f;
}

/*public virtual*/
HitResult Tile::clip(Level* level, int xt, int yt, int zt, const Vec3& A, const Vec3& B) {
    updateShape(level, xt, yt, zt);

	//Stopwatch sw;
	//sw.start();

	Vec3 sub((float)xt, (float)yt, (float)zt);
	Vec3 a = A - sub;//a.add((float)-xt, (float)-yt, (float)-zt);
    Vec3 b = B - sub;//b.add((float)-xt, (float)-yt, (float)-zt);

    Vec3 xh0, xh1, yh0, yh1, zh0, zh1;

	bool bxh0 = a.clipX(b, xx0, xh0);
    bool bxh1 = a.clipX(b, xx1, xh1);

    bool byh0 = a.clipY(b, yy0, yh0);
    bool byh1 = a.clipY(b, yy1, yh1);

    bool bzh0 = a.clipZ(b, zz0, zh0);
    bool bzh1 = a.clipZ(b, zz1, zh1);

    //if (!containsX(xh0)) xh0 = NULL;
    if (!bxh0 || !containsX(xh0)) bxh0 = false;
    if (!bxh1 || !containsX(xh1)) bxh1 = false;
    if (!byh0 || !containsY(yh0)) byh0 = false;
    if (!byh1 || !containsY(yh1)) byh1 = false;
    if (!bzh0 || !containsZ(zh0)) bzh0 = false;
    if (!bzh1 || !containsZ(zh1)) bzh1 = false;
    Vec3* closest = NULL;

    //if (xh0 != NULL && (closest == NULL || a.distanceToSqr(xh0) < a.distanceToSqr(closest))) closest = xh0;
    if (bxh0 && (closest == NULL || a.distanceToSqr(xh0) < a.distanceToSqr(*closest))) closest = &xh0;
    if (bxh1 && (closest == NULL || a.distanceToSqr(xh1) < a.distanceToSqr(*closest))) closest = &xh1;
    if (byh0 && (closest == NULL || a.distanceToSqr(yh0) < a.distanceToSqr(*closest))) closest = &yh0;
    if (byh1 && (closest == NULL || a.distanceToSqr(yh1) < a.distanceToSqr(*closest))) closest = &yh1;
    if (bzh0 && (closest == NULL || a.distanceToSqr(zh0) < a.distanceToSqr(*closest))) closest = &zh0;
    if (bzh1 && (closest == NULL || a.distanceToSqr(zh1) < a.distanceToSqr(*closest))) closest = &zh1;

    if (closest == NULL)
		return HitResult();

    int face = -1;

    if (closest == &xh0) face = 4;
    if (closest == &xh1) face = 5;
    if (closest == &yh0) face = 0;
    if (closest == &yh1) face = 1;
    if (closest == &zh0) face = 2;
    if (closest == &zh1) face = 3;

	//sw.stop();
	//sw.printEvery(5, ">>> ");

    return HitResult(xt, yt, zt, face, closest->add((float)xt, (float)yt, (float)zt));
}

/*virtual*/
void Tile::spawnResources(Level* level, int x, int y, int z, int data, float odds) {
    if (level->isClientSide) return;

    int count = getResourceCount(&level->random);
    for (int i = 0; i < count; i++) {
        if (level->random.nextFloat() > odds) continue;
        int type = getResource(data, &level->random);
        if (type <= 0) continue;
        const float s = 0.7f;
        float xo = level->random.nextFloat() * s + (1 - s) * 0.5f;
        float yo = level->random.nextFloat() * s + (1 - s) * 0.5f;
        float zo = level->random.nextFloat() * s + (1 - s) * 0.5f;
		ItemEntity* item = new ItemEntity(level, x + xo, y + yo, z + zo, ItemInstance(type, 1, getSpawnResourcesAuxValue(data)));
        item->throwTime = 10;
        level->addEntity(item);
    }
}

void Tile::spawnResources( Level* level, int x, int y, int z, int data )
{
	spawnResources(level, x, y, z, data, 1);
}

void Tile::popResource(Level* level, int x, int y, int z, const ItemInstance& itemInstance) {
	if (level->isClientSide || level->getLevelData()->getGameType() == GameType::Creative) return;

	float s = 0.7f;
	float xo = level->random.nextFloat() * s + (1 - s) * 0.5f;
	float yo = level->random.nextFloat() * s + (1 - s) * 0.5f;
	float zo = level->random.nextFloat() * s + (1 - s) * 0.5f;

	ItemEntity* item = new ItemEntity(level, x + xo, y + yo, z + zo, itemInstance);
	item->throwTime = 10;
	level->addEntity(item);
}


void Tile::destroy( Level* level, int x, int y, int z, int data )
{
}

bool Tile::isCubeShaped()
{
	return true;
}

int Tile::getRenderShape()
{
	return SHAPE_BLOCK;
}

float Tile::getBrightness( LevelSource* level, int x, int y, int z )
{
	return level->getBrightness(x, y, z);
}

bool Tile::shouldRenderFace( LevelSource* level, int x, int y, int z, int face )
{
	if (face == 0 && y == -1) return false;
	// Note: the old x/z == -1/256 guards are intentionally removed.
	// For finite worlds, out-of-bounds getTile() returns invisible_bedrock (opaque),
	// so the face will still be suppressed by the isSolidRender() check below.
	// For infinite worlds, those are valid coordinates and faces must be drawn.
	// Common
	if (face == 0 && yy0 > 0) return true;
	if (face == 1 && yy1 < 1) return true;
	if (face == 2 && zz0 > 0) return true;
	if (face == 3 && zz1 < 1) return true;
	if (face == 4 && xx0 > 0) return true;
	if (face == 5 && xx1 < 1) return true;
	Tile* t = Tile::tiles[level->getTile(x, y, z)];
	if (!t) return true;
	if (face == 1 && t->id == topSnow->id) return false;
	return !t->isSolidRender();
	//return (!level->isSolidRenderTile(x, y, z));
}

int Tile::getTexture( LevelSource* level, int x, int y, int z, int face )
{
	return getTexture(face, level->getData(x, y, z));
}

int Tile::getTexture( int face, int data )
{
	return getTexture(face);
}

int Tile::getTexture( int face )
{
	return tex;
}

void Tile::addAABBs( Level* level, int x, int y, int z, const AABB* box, std::vector<AABB>& boxes )
{
	AABB* aabb = getAABB(level, x, y, z);
	if (aabb != NULL && box->intersects(*aabb)) {
		boxes.push_back(*aabb);
	}
}

AABB* Tile::getAABB( Level* level, int x, int y, int z )
{
	tmpBB.x0 = x + xx0;
	tmpBB.y0 = y + yy0;
	tmpBB.z0 = z + zz0;
	tmpBB.x1 = x + xx1;
	tmpBB.y1 = y + yy1;
	tmpBB.z1 = z + zz1;
	return &tmpBB;
}

bool Tile::isSolidRender()
{
	return true;
}

bool Tile::mayPick( int data, bool liquid )
{
	return mayPick();
}

bool Tile::mayPick()
{
	return true;
}

int Tile::getTickDelay()
{
	return 10;
}

int Tile::getResourceCount( Random* random )
{
	return 1;
}

int Tile::getResource( int data, Random* random )
{
	return id;
}

float Tile::getExplosionResistance( Entity* source )
{
	return explosionResistance / 5.0f;
}

int Tile::getRenderLayer()
{
	return Tile::RENDERLAYER_OPAQUE;
}

bool Tile::use( Level* level, int x, int y, int z, Player* player )
{
	return false;
}

bool Tile::spawnBurnResources( Level* level, float x, float y, float z )
{
	return false;
}

int Tile::getColor( LevelSource* level, int x, int y, int z )
{
	return 0xffffff;
}

bool Tile::getSignal( LevelSource* level, int x, int y, int z )
{
	return false;
}

bool Tile::getSignal( LevelSource* level, int x, int y, int z, int dir )
{
	return false;
}

bool Tile::isSignalSource()
{
	return false;
}

bool Tile::getDirectSignal( Level* level, int x, int y, int z, int dir )
{
	return false;
}

void Tile::playerDestroy( Level* level, Player* player, int x, int y, int z, int data )
{
	//player.awardStat(Stats.blockMined[id], 1);
	spawnResources(level, x, y, z, data);
}

bool Tile::canSurvive( Level* level, int x, int y, int z )
{
	return true;
}

Tile* Tile::setDescriptionId( const std::string& id )
{
	descriptionId = TILE_DESCRIPTION_PREFIX + id;
	return this;
}

std::string Tile::getName() const
{
	return I18n::get(getDescriptionId() + ".name");
}

std::string Tile::getDescriptionId() const
{
	return descriptionId;
}

Tile* Tile::setSoundType( const SoundType& soundType )
{
	this->soundType = &soundType;
	return this;
}

Tile* Tile::setLightBlock( int i )
{
	lightBlock[id] = i;
	return this;
}

Tile* Tile::setExplodeable( float explosionResistance )
{
	this->explosionResistance = explosionResistance * 3;
	return this;
}

Tile* Tile::setDestroyTime( float destroySpeed )
{
	this->destroySpeed = destroySpeed;
	if (explosionResistance < destroySpeed * 5) explosionResistance = destroySpeed * 5;
	return this;
}

void Tile::setTicking( bool tick )
{
	shouldTick[id] = tick;
}

int Tile::getSpawnResourcesAuxValue( int data )
{
	return 0;
}

bool Tile::containsX( const Vec3& v )
{
	return v.y >= yy0 && v.y <= yy1 && v.z >= zz0 && v.z <= zz1;
}

bool Tile::containsY( const Vec3& v )
{
	return v.x >= xx0 && v.x <= xx1 && v.z >= zz0 && v.z <= zz1;
}

bool Tile::containsZ( const Vec3& v )
{
	return v.x >= xx0 && v.x <= xx1 && v.y >= yy0 && v.y <= yy1;
}

/*public*/
AABB Tile::getTileAABB(Level* level, int x, int y, int z) {
	return AABB(x + xx0, y + yy0, z + zz0, x + xx1, y + yy1, z + zz1);
}

/*public*/
void Tile::setShape(float x0, float y0, float z0, float x1, float y1, float z1) {
	this->xx0 = x0;
	this->yy0 = y0;
	this->zz0 = z0;
	this->xx1 = x1;
	this->yy1 = y1;
	this->zz1 = z1;
}

/*public*/
bool Tile::mayPlace(Level* level, int x, int y, int z, unsigned char face) {
	return mayPlace(level, x, y, z);
}

bool Tile::mayPlace( Level* level, int x, int y, int z ) {
	int t = level->getTile(x, y, z);
	return t == 0 || Tile::tiles[t]->material->isReplaceable();
}

Tile* Tile::setCategory(int category) {
	this->category = category;
	return this;
}

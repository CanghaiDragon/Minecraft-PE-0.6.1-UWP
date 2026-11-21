#include "LevelData.h"

static int getConfiguredWorldType(const LevelSettings& settings)
{
	if (settings.shouldResolveIndevInfdev() &&
		settings.getWorldType() == WorldType::Indev &&
		WorldType::isIndevInfdev(settings.getSeed()))
		return WorldType::EarlyInfdev;
	return settings.getWorldType();
}

LevelData::LevelData()
:	xSpawn(128),
	ySpawn(64),
	zSpawn(128),

	seed(0),
	lastPlayed(0),
	generatorVersion(SharedConstants::GeneratorVersion),
	time(0),
	dimension(Dimension::NORMAL),
	playerDataVersion(-1),
	storageVersion(0),
	gameType(GameType::Default),
	worldType(WorldType::Old),
	indevType(IndevType::Inland),
	biomeMapping(BiomeMapping::None),
	alphaWinterDiscoveryPending(false),
	infdevDiscoveryPending(false),
	loadedPlayerTag(NULL)
{
	//LOGI("ctor 1: %p\n", this);
	spawnMobs = (gameType == GameType::Survival);
}

LevelData::LevelData( const LevelSettings& settings, const std::string& levelName, int generatorVersion /*= -1*/ )
:	seed(settings.getSeed()),
	gameType(settings.getGameType()),
	worldType(getConfiguredWorldType(settings)),
	indevType(settings.getIndevType()),
	biomeMapping(BiomeMapping::None),
	alphaWinterDiscoveryPending(settings.getWorldType() == WorldType::Alpha &&
		WorldType::isAlphaWinter(settings.getSeed())),
	infdevDiscoveryPending(settings.shouldResolveIndevInfdev() &&
		settings.getWorldType() == WorldType::Indev &&
		WorldType::isIndevInfdev(settings.getSeed())),
	levelName(levelName),
	xSpawn((getConfiguredWorldType(settings) == WorldType::Infinite || getConfiguredWorldType(settings) == WorldType::Sky || getConfiguredWorldType(settings) == WorldType::Alpha || getConfiguredWorldType(settings) == WorldType::EarlyInfdev) ? 0 : 128),
	ySpawn(64),
	zSpawn((getConfiguredWorldType(settings) == WorldType::Infinite || getConfiguredWorldType(settings) == WorldType::Sky || getConfiguredWorldType(settings) == WorldType::Alpha || getConfiguredWorldType(settings) == WorldType::EarlyInfdev) ? 0 : 128),
	lastPlayed(0),
	time(0),
	dimension(Dimension::NORMAL),
	playerDataVersion(-1),
	loadedPlayerTag(NULL)
{
	//LOGI("ctor 2: %p\n", this);

	if (generatorVersion < 0)
		generatorVersion = SharedConstants::GeneratorVersion;

	this->generatorVersion = generatorVersion;
	spawnMobs = (gameType == GameType::Survival);
}

LevelData::LevelData( CompoundTag* tag )
:	loadedPlayerTag(NULL)
{
	//LOGI("ctor 3: %p (%p)\n", this, tag);
	getTagData(tag);
}

LevelData::LevelData( const LevelData& rhs )
:	seed(rhs.seed),
	gameType(rhs.gameType),
	worldType(rhs.worldType),
	indevType(rhs.indevType),
	biomeMapping(rhs.biomeMapping),
	levelName(rhs.levelName),
	xSpawn(rhs.xSpawn),
	ySpawn(rhs.ySpawn),
	zSpawn(rhs.zSpawn),
	lastPlayed(rhs.lastPlayed),
	time(rhs.time),
	dimension(rhs.dimension),
	storageVersion(rhs.storageVersion),
	playerDataVersion(rhs.playerDataVersion),
	generatorVersion(rhs.generatorVersion),
	spawnMobs(rhs.spawnMobs),
	alphaWinterDiscoveryPending(rhs.alphaWinterDiscoveryPending),
	infdevDiscoveryPending(rhs.infdevDiscoveryPending),
	loadedPlayerTag(NULL),
	playerData(rhs.playerData)
{
	//LOGI("c-ctor: %p (%p)\n", this, &rhs);
	setPlayerTag(rhs.loadedPlayerTag);
	//PlayerData playerData;
}

LevelData& LevelData::operator=( const LevelData& rhs )
{
	//LOGI("as-op: %p (%p)\n", this, &rhs);
	if (this != &rhs) {
		seed		= rhs.seed;
		gameType	= rhs.gameType;
		worldType	= rhs.worldType;
		indevType = rhs.indevType;
		biomeMapping = rhs.biomeMapping;
		levelName	= rhs.levelName;
		xSpawn		= rhs.xSpawn;
		ySpawn		= rhs.ySpawn;
		zSpawn		= rhs.zSpawn;
		lastPlayed	= rhs.lastPlayed;
		time		= rhs.time;
		dimension	= rhs.dimension;
		spawnMobs	= rhs.spawnMobs;
		alphaWinterDiscoveryPending = rhs.alphaWinterDiscoveryPending;
		infdevDiscoveryPending = rhs.infdevDiscoveryPending;
		playerData  = rhs.playerData;
		playerDataVersion	= rhs.playerDataVersion;
		generatorVersion	= rhs.generatorVersion;
		storageVersion		= rhs.storageVersion;
		setPlayerTag(rhs.loadedPlayerTag);
	}

	return *this;
}

LevelData::~LevelData()
{
	//LOGI("dtor: %p\n", this);
	setPlayerTag(NULL);
}

void LevelData::v1_write( RakNet::BitStream& bitStream )
{
	bitStream.Write(seed);
	bitStream.Write(xSpawn);
	bitStream.Write(ySpawn);
	bitStream.Write(zSpawn);
	bitStream.Write(time);
	bitStream.Write(sizeOnDisk);
	bitStream.Write(getEpochTimeS());
	RakNet::RakString rakName = levelName.c_str();
	bitStream.Write(rakName);
	//LOGI("WBS: %d, %d, %d, %d, %d, %d\n", seed, xSpawn, ySpawn, zSpawn, time, sizeOnDisk);
}

void LevelData::v1_read( RakNet::BitStream& bitStream, int storageVersion )
{
	this->storageVersion = storageVersion;
	bitStream.Read(seed);
	bitStream.Read(xSpawn);
	bitStream.Read(ySpawn);
	bitStream.Read(zSpawn);
	bitStream.Read(time);
	bitStream.Read(sizeOnDisk);
	bitStream.Read(lastPlayed);
	RakNet::RakString rakName;
	bitStream.Read(rakName);
	levelName = rakName.C_String();
	//LOGI("RBS: %d, %d, %d, %d, %d, %d\n", seed, xSpawn, ySpawn, zSpawn, time, sizeOnDisk);
}

// Caller's responsibility to destroy this Tag
CompoundTag* LevelData::createTag()
{
	CompoundTag* tag = new CompoundTag();
	CompoundTag* player = loadedPlayerTag? (CompoundTag*)loadedPlayerTag->copy() : NULL;
	setTagData(tag, player);
	return tag;
}

CompoundTag* LevelData::createTag( const std::vector<Player*>& players )
{
	CompoundTag* tag = new CompoundTag();

	Player* player = NULL;
	CompoundTag* playerTag = NULL;

	if (!players.empty()) player = players[0];
	if (player != NULL) {
		playerTag = new CompoundTag();
		player->saveWithoutId(playerTag);
	}
	setTagData(tag, playerTag);

	return tag;
}

void LevelData::setTagData( CompoundTag* tag, CompoundTag* playerTag )
{
	if (!tag) return;
	tag->putLong("RandomSeed", seed);
	tag->putInt("GameType", gameType);
	tag->putInt("SpawnX", xSpawn);
	tag->putInt("SpawnY", ySpawn);
	tag->putInt("SpawnZ", zSpawn);
	tag->putLong("Time", time);
	tag->putLong("SizeOnDisk", sizeOnDisk);
	tag->putLong("LastPlayed", getEpochTimeS());
	tag->putString("LevelName", levelName);
	tag->putInt("StorageVersion", storageVersion);
	tag->putInt("Platform", 2);
	tag->putInt("WorldType", worldType);
	tag->putInt("IndevType", indevType);
	tag->putInt("BiomeMapping", biomeMapping);
	tag->putBoolean("AlphaWinterDiscoveryPending", alphaWinterDiscoveryPending);
	tag->putBoolean("InfdevDiscoveryPending", infdevDiscoveryPending);

	if (playerTag != NULL) {
		tag->putCompound("Player", playerTag);
	}
}

void LevelData::getTagData( const CompoundTag* tag )
{
	if (!tag) return;
	seed = (long)tag->getLong("RandomSeed");
	gameType = tag->getInt("GameType");
	xSpawn = tag->getInt("SpawnX");
	ySpawn = tag->getInt("SpawnY");
	zSpawn = tag->getInt("SpawnZ");
	time = (long)tag->getLong("Time");
	lastPlayed = (int)tag->getLong("LastPlayed");
	sizeOnDisk = (int)tag->getLong("SizeOnDisk");
	levelName = tag->getString("LevelName");
	storageVersion = tag->getInt("StorageVersion");
	worldType = tag->getInt("WorldType"); // 0 if missing (old worlds default to Old)
	indevType = tag->contains("IndevType", Tag::TAG_Int)
		? tag->getInt("IndevType") : IndevType::Inland;
	biomeMapping = tag->contains("BiomeMapping", Tag::TAG_Int)
		? tag->getInt("BiomeMapping") : BiomeMapping::None;
	alphaWinterDiscoveryPending = tag->contains("AlphaWinterDiscoveryPending", Tag::TAG_Byte) &&
		tag->getBoolean("AlphaWinterDiscoveryPending");
	infdevDiscoveryPending = tag->contains("InfdevDiscoveryPending", Tag::TAG_Byte) &&
		tag->getBoolean("InfdevDiscoveryPending");

	spawnMobs = (gameType == GameType::Survival);

	if (tag->contains("Player", Tag::TAG_Compound)) {
		setPlayerTag(tag->getCompound("Player"));

		//dimension = loadedPlayerTag.getInt("Dimension");
	}
}

void LevelData::setPlayerTag( CompoundTag* tag )
{
	if (loadedPlayerTag) {
		loadedPlayerTag->deleteChildren();
		delete loadedPlayerTag;
		loadedPlayerTag = NULL;
	}
	if (tag)
		loadedPlayerTag = (CompoundTag*)tag->copy();
}

long LevelData::getSeed() const
{
	return seed;
}

int LevelData::getXSpawn() const
{
	return xSpawn;
}

int LevelData::getYSpawn() const
{
	return ySpawn;
}

int LevelData::getZSpawn() const
{
	return zSpawn;
}

long LevelData::getTime() const
{
	return time;
}

long LevelData::getSizeOnDisk() const
{
	return sizeOnDisk;
}

CompoundTag* LevelData::getLoadedPlayerTag()
{
	return loadedPlayerTag;
}

void LevelData::setLoadedPlayerTo( Player* p )
{
	if (playerDataVersion == 1)
		playerData.loadPlayer(p);
}

int LevelData::getDimension()
{
	return dimension;
}

void LevelData::setSeed( long seed )
{
	this->seed = seed;
}

void LevelData::setXSpawn( int xSpawn )
{
	this->xSpawn = xSpawn;
}

void LevelData::setYSpawn( int ySpawn )
{
	this->ySpawn = ySpawn;
}

void LevelData::setZSpawn( int zSpawn )
{
	this->zSpawn = zSpawn;
}

void LevelData::setTime( long time )
{
	this->time = time;
}

void LevelData::setSizeOnDisk( long sizeOnDisk )
{
	this->sizeOnDisk = sizeOnDisk;
}

void LevelData::setLoadedPlayerTag( CompoundTag* playerTag )
{
	LOGI("set-p: %p (%p <- %p)\n", this, loadedPlayerTag, playerTag);
	if (loadedPlayerTag) {
		loadedPlayerTag->deleteChildren();
		delete loadedPlayerTag;
	}
	loadedPlayerTag = playerTag;
}

void LevelData::setDimension( int dimension )
{
	this->dimension = dimension;
}

void LevelData::setSpawn( int xSpawn, int ySpawn, int zSpawn )
{
	this->xSpawn = xSpawn;
	this->ySpawn = ySpawn;
	this->zSpawn = zSpawn;
}

std::string LevelData::getLevelName()
{
	return levelName;
}

void LevelData::setLevelName( const std::string& levelName )
{
	this->levelName = levelName;
}

int LevelData::getGeneratorVersion() const
{
	return generatorVersion;
}

void LevelData::setGeneratorVersion( int version )
{
	this->generatorVersion = version;
}

long LevelData::getLastPlayed() const
{
	return lastPlayed;
}

int LevelData::getStorageVersion() const
{
	return storageVersion;
}

void LevelData::setStorageVersion( int version )
{
	storageVersion = version;
}

int LevelData::getGameType() const
{
	return gameType;
}

void LevelData::setGameType( int type )
{
	gameType = type;
}

bool LevelData::getSpawnMobs() const
{
	return spawnMobs;
}

void LevelData::setSpawnMobs( bool doSpawn )
{
	spawnMobs = doSpawn;
}

bool LevelData::takeAlphaWinterDiscoveryMessage()
{
	if (!alphaWinterDiscoveryPending) return false;
	alphaWinterDiscoveryPending = false;
	return true;
}

bool LevelData::takeInfdevDiscoveryMessage()
{
	if (!infdevDiscoveryPending) return false;
	infdevDiscoveryPending = false;
	return true;
}

int LevelData::getWorldType() const
{
	return worldType;
}

int LevelData::getIndevType() const { return indevType; }

void LevelData::setIndevType(int type) { indevType = type; }

int LevelData::getBiomeMapping() const { return biomeMapping; }

void LevelData::setBiomeMapping(int mapping) { biomeMapping = mapping; }

void LevelData::setWorldType( int type )
{
	worldType = type;
}

bool LevelData::isInfinite() const
{
	return worldType == WorldType::Infinite || worldType == WorldType::Sky ||
		worldType == WorldType::Alpha || worldType == WorldType::EarlyInfdev;
}

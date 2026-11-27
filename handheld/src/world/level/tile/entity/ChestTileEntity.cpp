#include "ChestTileEntity.h"
#include "../ChestTile.h"
#include "../../Level.h"
#include "../../../Facing.h"
#include "../../../entity/player/Player.h"
#include "../../../../nbt/NbtIo.h"
#include "../../../../platform/log.h"

#if defined(_DEBUG)
static void traceChestPersistence(const char* phase, const ChestTileEntity* chest)
{
	bool hasItems = false;
	const char* side = (chest->level != NULL)
		? (chest->level->isClientSide ? "client" : "server")
		: "unbound";

	for (int slot = 0; slot < chest->getContainerSize(); ++slot) {
		ItemInstance* item = const_cast<ChestTileEntity*>(chest)->getItem(slot);
		if (item != NULL && !item->isNull()) {
			hasItems = true;
			LOGI("[%s] side=%s chest=%p pos=%d,%d,%d slot=%d item=%d aux=%d count=%d\n",
				phase, side, (void*)chest, chest->x, chest->y, chest->z,
				slot, item->id, item->getAuxValue(), item->count);
		}
	}
	if (!hasItems)
		LOGI("[%s] side=%s chest=%p pos=%d,%d,%d items=empty\n",
			phase, side, (void*)chest, chest->x, chest->y, chest->z);
}
#endif

ChestTileEntity::ChestTileEntity()
:	super(TileEntityType::Chest),
	FillingContainer(ItemsSize, 0, ContainerType::CONTAINER, false),
	hasCheckedNeighbors(false),
	n(NULL), e(NULL), w(NULL), s(NULL),
	openness(0), oOpenness(0),
	openCount(0),
	tickInterval(0),
	remoteContainerSize(ItemsSize)
{
	//rendererId = TR_CHEST_RENDERER;
}

int ChestTileEntity::getContainerSize() const
{
	if (clientSideOnly)
		return remoteContainerSize;
	return getLargeChestPartner() != NULL ? ItemsSize * 2 : ItemsSize;
}

ChestTileEntity* ChestTileEntity::findCompatibleNeighbor() const
{
	if (clientSideOnly || level == NULL)
		return NULL;

	static const int offsets[4][2] = {
		{-1, 0}, {1, 0}, {0, -1}, {0, 1}
	};
	const int direction = level->getData(x, y, z);
	ChestTileEntity* found = NULL;

	for (int i = 0; i < 4; ++i) {
		const int dx = offsets[i][0];
		const int dz = offsets[i][1];
		const int nx = x + dx;
		const int nz = z + dz;
		if (level->getTile(nx, y, nz) != Tile::chest->id)
			continue;

		TileEntity* neighbor = level->getTileEntity(nx, y, nz);
		if (!TileEntity::isType(neighbor, TileEntityType::Chest))
			continue;
		if (level->getData(nx, y, nz) != direction)
			continue;

		// PE 0.8 pairs only chests sharing the same facing, along the axis
		// perpendicular to that facing.
		if ((dx == 0 && direction != Facing::WEST && direction != Facing::EAST) ||
			(dz == 0 && direction != Facing::NORTH && direction != Facing::SOUTH))
			continue;

		if (found != NULL)
			return NULL; // Ambiguous multi-chest layouts are not double chests.
		found = (ChestTileEntity*)neighbor;
	}

	return found;
}

ChestTileEntity* ChestTileEntity::getLargeChestPartner() const
{
	ChestTileEntity* partner = findCompatibleNeighbor();
	if (partner == NULL || partner->findCompatibleNeighbor() != this)
		return NULL;
	return partner;
}

ChestTileEntity* ChestTileEntity::getContainerForSlot(int& slot) const
{
	if (clientSideOnly) {
		if (slot < 0 || slot >= remoteContainerSize)
			return NULL;
		return const_cast<ChestTileEntity*>(this);
	}
	if (slot < 0)
		return NULL;

	ChestTileEntity* partner = getLargeChestPartner();
	if (partner == NULL)
		return slot < ItemsSize ? const_cast<ChestTileEntity*>(this) : NULL;
	if (slot >= ItemsSize * 2)
		return NULL;

	const bool thisIsFirstHalf = x < partner->x || (x == partner->x && z < partner->z);
	if (slot >= ItemsSize) {
		slot -= ItemsSize;
		return thisIsFirstHalf ? partner : const_cast<ChestTileEntity*>(this);
	}
	return thisIsFirstHalf ? const_cast<ChestTileEntity*>(this) : partner;
}

void ChestTileEntity::setRemoteContainerSize(int size)
{
	if (!clientSideOnly)
		return;

	remoteContainerSize = size >= ItemsSize * 2 ? ItemsSize * 2 : ItemsSize;
	numTotalSlots = remoteContainerSize;
	items.resize(remoteContainerSize, NULL);
}

int ChestTileEntity::getLocalContainerSize() const
{
	return ItemsSize;
}

ItemInstance* ChestTileEntity::getLocalItem(int slot)
{
	if (slot < 0 || slot >= ItemsSize)
		return NULL;
	return items[slot];
}

ItemInstance* ChestTileEntity::getItem(int slot)
{
	int localSlot = slot;
	ChestTileEntity* owner = getContainerForSlot(localSlot);
	if (owner == NULL || localSlot < 0 || localSlot >= (int)owner->items.size())
		return NULL;
	return owner->items[localSlot];
}

bool ChestTileEntity::add(ItemInstance* item)
{
	const int countBefore = item != NULL ? item->count : 0;
	if (clientSideOnly || item == NULL || item->isNull()) {
		const bool added = FillingContainer::add(item);
		if (item != NULL && item->count != countBefore)
			setChanged();
		return added;
	}

	ChestTileEntity* partner = getLargeChestPartner();
	if (partner == NULL) {
		const bool added = FillingContainer::add(item);
		if (item->count != countBefore)
			setChanged();
		return added;
	}

	const bool thisIsFirstHalf = x < partner->x || (x == partner->x && z < partner->z);
	ChestTileEntity* first = thisIsFirstHalf ? this : partner;
	ChestTileEntity* second = thisIsFirstHalf ? partner : this;
	bool added = first->FillingContainer::add(item);
	if (item->count > 0)
		added = second->FillingContainer::add(item) || added;
	if (item->count != countBefore) {
		first->setChanged();
		second->setChanged();
	}
	return added;
}

ItemInstance ChestTileEntity::removeItem(int slot, int count)
{
	if (count <= 0)
		return ItemInstance();

	int localSlot = slot;
	ChestTileEntity* owner = getContainerForSlot(localSlot);
	if (owner == NULL)
		return ItemInstance();

	ItemInstance* current = owner->items[localSlot];
	if (current == NULL || current->isNull())
		return ItemInstance();

	const int removedCount = count < current->count ? count : current->count;
	ItemInstance removed(*current);
	removed.count = removedCount;
	if (removedCount >= current->count) {
		owner->FillingContainer::clearSlot(localSlot);
		owner->setChanged();
	} else {
		current->count -= removedCount;
		owner->setChanged();
	}
	return removed;
}

void ChestTileEntity::clearSlot(int slot)
{
	int localSlot = slot;
	ChestTileEntity* owner = getContainerForSlot(localSlot);
	if (owner == NULL)
		return;

	ItemInstance* current = owner->items[localSlot];
	const bool changed = current != NULL && !current->isNull();
	owner->FillingContainer::clearSlot(localSlot);
	if (changed)
		owner->setChanged();
}

void ChestTileEntity::setItem(int slot, ItemInstance* item)
{
	int localSlot = slot;
	ChestTileEntity* owner = getContainerForSlot(localSlot);
	if (owner == NULL)
		return;

	owner->FillingContainer::setItem(localSlot, item);
	ItemInstance* stored = owner->items[localSlot];
	if (stored != NULL && stored->count > getMaxStackSize())
		stored->count = getMaxStackSize();
	owner->setChanged();
}

std::string ChestTileEntity::getName() const
{
	return "container.chest";
}

bool ChestTileEntity::shouldSave() {
	for (int i = 0; i < ItemsSize; ++i)
		if (items[i] && !items[i]->isNull()) return true;
	return false;
}

void ChestTileEntity::load( CompoundTag* base )
{
	super::load(base);

	if (!base->contains("Items")) {//, Tag::TAG_List)
	#if defined(_DEBUG)
		traceChestPersistence("CHEST_LOAD", this);
	#endif
		return;
	}

	ListTag* inventoryList = base->getList("Items");
	for (int i = 0; i < inventoryList->size(); i++) {
		Tag* ttag = inventoryList->get(i);
		if (ttag->getId() != Tag::TAG_Compound)
			continue;

		CompoundTag* tag = (CompoundTag*) ttag;
		int slot = tag->getByte("Slot") & 0xff;
		if (slot >= 0 && slot < ItemsSize) {
			if (!items[slot]) items[slot] = new ItemInstance(); //@chestodo
			items[slot]->load(tag);
		}
	}

#if defined(_DEBUG)
	traceChestPersistence("CHEST_LOAD", this);
#endif
}

bool ChestTileEntity::save( CompoundTag* base )
{
#if defined(_DEBUG)
	traceChestPersistence("CHEST_SERIALIZE", this);
#endif

	if (!super::save(base))
		return false;

	ListTag* listTag = new ListTag();

	for (int i = 0; i < ItemsSize; i++) {
		if (items[i] && !items[i]->isNull()) {
			CompoundTag* tag = new CompoundTag();
			tag->putByte("Slot", (char) i);
			items[i]->save(tag);
			listTag->add(tag);
		}
	}
	base->put("Items", listTag);
	return true;
}

int ChestTileEntity::getMaxStackSize() const
{
	return Container::LARGE_MAX_STACK_SIZE;
}

bool ChestTileEntity::stillValid( Player* player )
{
	if (level->getTileEntity(x, y, z) != this) return false;
	if (player->distanceToSqr(x + 0.5f, y + 0.5f, z + 0.5f) > 8 * 8) return false;
	return true;
}

void ChestTileEntity::clearCache()
{
	super::clearCache();
	hasCheckedNeighbors = false;
}

void ChestTileEntity::checkNeighbors()
{
	if (hasCheckedNeighbors)
		return;

	hasCheckedNeighbors = true;
	n = NULL;
	e = NULL;
	w = NULL;
	s = NULL;

	//        if (getTile() != NULL) {
	if (level->getTile(x - 1, y, z) == Tile::chest->id) {
		w = (ChestTileEntity*) level->getTileEntity(x - 1, y, z);
	}
	if (level->getTile(x + 1, y, z) == Tile::chest->id) {
		e = (ChestTileEntity*) level->getTileEntity(x + 1, y, z);
	}
	if (level->getTile(x, y, z - 1) == Tile::chest->id) {
		n = (ChestTileEntity*) level->getTileEntity(x, y, z - 1);
	}
	if (level->getTile(x, y, z + 1) == Tile::chest->id) {
		s = (ChestTileEntity*) level->getTileEntity(x, y, z + 1);
	}

	if (n != NULL) n->clearCache();
	if (s != NULL) s->clearCache();
	if (e != NULL) e->clearCache();
	if (w != NULL) w->clearCache();
	//        }
}

void ChestTileEntity::tick()
{
	super::tick();
	checkNeighbors();

	if (++tickInterval >= 4 * SharedConstants::TicksPerSecond) {
		level->tileEvent(x, y, z, ChestTile::EVENT_SET_OPEN_COUNT, openCount);
		tickInterval = 0;
	}

	oOpenness = openness;

	float speed = 0.10f;
	if (openCount > 0 && openness == 0) {
		if (n == NULL && w == NULL) {
			float xc = x + 0.5f;
			float zc = z + 0.5f;
			if (s != NULL) zc += 0.5f;
			if (e != NULL) xc += 0.5f;

			level->playSound(xc, y + 0.5f, zc, "random.chestopen", 0.5f, level->random.nextFloat() * 0.1f + 0.9f);
		}
	}
	if ((openCount == 0 && openness > 0) || (openCount > 0 && openness < 1)) {
		float oldOpen = openness;
		if (openCount > 0) openness += speed;
		else openness -= speed;
		if (openness > 1) {
			openness = 1;
		}
		float lim = 0.5f;
		if (openness < lim && oldOpen >= lim) {
			if (n == NULL && w == NULL) {
				float xc = x + 0.5f;
				float zc = z + 0.5f;
				if (s != NULL) zc += 0.5f;
				if (e != NULL) xc += 0.5f;

				level->playSound(xc, y + 0.5f, zc, "random.chestclosed", 0.5f, level->random.nextFloat() * 0.1f + 0.9f);
			}
		}
		if (openness < 0) {
			openness = 0;
		}
	}
}

void ChestTileEntity::triggerEvent( int b0, int b1 )
{
	if (b0 == ChestTile::EVENT_SET_OPEN_COUNT) {
		openCount = b1;
	}
}

void ChestTileEntity::startOpen()
{
	openCount++;
	level->tileEvent(x, y, z, ChestTile::EVENT_SET_OPEN_COUNT, openCount);
}

void ChestTileEntity::stopOpen()
{
	openCount--;
	level->tileEvent(x, y, z, ChestTile::EVENT_SET_OPEN_COUNT, openCount);
}

void ChestTileEntity::setRemoved()
{
	clearCache();
	checkNeighbors();
	super::setRemoved();
}

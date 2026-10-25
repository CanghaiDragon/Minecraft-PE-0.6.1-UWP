#include "ChestTileEntity.h"
#include "../ChestTile.h"
#include "../../Level.h"
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
	tickInterval(0),
	openCount(0),
	openness(0), oOpenness(0),
	hasCheckedNeighbors(false),
	n(NULL), s(NULL), w(NULL), e(NULL)
{
	//rendererId = TR_CHEST_RENDERER;
}

int ChestTileEntity::getContainerSize() const
{
	return ItemsSize;
}

ItemInstance* ChestTileEntity::getItem( int slot )
{
	return items[slot];
}

bool ChestTileEntity::add(ItemInstance* item)
{
	const int countBefore = item != NULL ? item->count : 0;
	const bool added = FillingContainer::add(item);
	if (item != NULL && item->count != countBefore)
		setChanged();
	return added;
}

ItemInstance ChestTileEntity::removeItem(int slot, int count)
{
	if (slot < 0 || slot >= getContainerSize() || count <= 0)
		return ItemInstance();

	ItemInstance* current = getItem(slot);
	if (current == NULL || current->isNull())
		return ItemInstance();

	const int removedCount = count < current->count ? count : current->count;
	ItemInstance removed(*current);
	removed.count = removedCount;
	if (removedCount >= current->count)
		clearSlot(slot);
	else {
		current->count -= removedCount;
		setChanged();
	}
	return removed;
}

void ChestTileEntity::clearSlot(int slot)
{
	if (slot < 0 || slot >= getContainerSize())
		return;

	ItemInstance* current = getItem(slot);
	const bool changed = current != NULL && !current->isNull();
	FillingContainer::clearSlot(slot);
	if (changed)
		setChanged();
}

void ChestTileEntity::setItem(int slot, ItemInstance* item)
{
	if (slot < 0 || slot >= getContainerSize())
		return;

	FillingContainer::setItem(slot, item);
	ItemInstance* stored = getItem(slot);
	if (stored != NULL && stored->count > getMaxStackSize())
		stored->count = getMaxStackSize();
	setChanged();
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

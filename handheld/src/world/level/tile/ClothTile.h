#ifndef NET_MINECRAFT_WORLD_LEVEL_TILE__ClothTile_H__
#define NET_MINECRAFT_WORLD_LEVEL_TILE__ClothTile_H__

//package net.minecraft.world.level.tile;

#include "../material/Material.h"

class ClothTile: public Tile
{
	typedef Tile super;
public:
    ClothTile(int id)
	:	super(id, 4 * 16, Material::cloth)
	{
	}
	ClothTile(int id, int data)
		:	super(id, 4 * 16, Material::cloth)
	{
	}

    int getTexture(int face, int data) {
		// The old color column in terrain.png does not contain a complete wool
		// palette (notably yellow and brown resolve to dirt/grass-side cells).
		// The extended 0.8 atlas stores the wool sprites in block metadata order.
		return 268 + (data & 0xf);
    }

	static int getTileDataForItemAuxValue(int auxValue) {
		return (~auxValue & 0xf);
	}
protected:
    int getSpawnResourcesAuxValue(int data) {
        return (~data & 0xf);
    }
	static int getItemAuxValueForTileData(int data) {
		return (~data & 0xf);
	}
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_TILE__ClothTile_H__*/

#ifndef NET_MINECRAFT_WORLD_ENTITY__Minecart_H__
#define NET_MINECRAFT_WORLD_ENTITY__Minecart_H__

#include "Entity.h"
#include "EntityTypes.h"
#include "player/Player.h"
#include "../level/Level.h"
#include "../level/tile/Tile.h"
#include "../item/Item.h"
#include "../../nbt/CompoundTag.h"

// PE 0.8's entity factory only creates the rideable minecart (entity 84).
// This compatibility entity follows the 0.8 rail metadata and speed rules;
// chest/furnace carts are deliberately not implemented.
class Minecart : public Entity {
public:
	Minecart(Level* level) : Entity(level), riderStrafe(0), riderForward(0) {
		setSize(0.98f, 0.7f);
		entityRendererId = ER_MINECART_RENDERER;
	}
	Minecart(Level* level, float x, float y, float z) : Minecart(level) {
		setPos(x, y, z);
		xo = x; yo = y; zo = z;
	}
	int getEntityTypeId() const { return EntityTypes::IdMinecart; }
	void setRidingInput(float strafe, float forward) { riderStrafe = strafe; riderForward = forward; }
	void positionRider(Entity* passenger) {
		if (passenger && rider == passenger) {
			// Lower the riding pose into the cart. In this port player y is measured
			// from the eye-height origin, so offset the position below the cart rim.
			passenger->setPos(x, bb.y0 + passenger->heightOffset - 0.05f, z);
		}
	}
	bool isPickable() { return !removed; }
	bool isPushable() { return !removed; }
	bool interact(Player* player) {
		if (!player) return false;
		if (rider == player) return true;
		if (rider || player->ridingAt) return false;
		player->ride(this);
		return true;
	}
	void remove() {
		if (rider) rider->ride(NULL);
		Entity::remove();
	}
	bool hurt(Entity*, int damage) {
		if (removed || damage <= 0) return false;
		if (level && !level->isClientSide && Item::minecart) spawnAtLocation(Item::minecart->id, 1);
		remove();
		return true;
	}
	void tick() {
		baseTick();
		if (removed || !level) return;

		// PE 0.8 gravity and the rail path's per-tick speed cap.
		yd -= 0.04f;
		int bx = Mth::floor(x), by = Mth::floor(y), bz = Mth::floor(z);
		int railY = by;
		int railId = level->getTile(bx, railY, bz);
		if (!isRail(railId) && isRail(level->getTile(bx, railY - 1, bz))) {
			--railY;
			railId = level->getTile(bx, railY, bz);
		}

		if (isRail(railId)) {
			int data = level->getData(bx, railY, bz);
			// Curved normal-rail shapes are metadata 6-9; only powered rails
			// reserve bit 3 for their power state.
			int shape = railId == Tile::railCompat->id ? data : (data & 7);
			float vx = 0, vz = 0;
			trackVector(shape, vx, vz);
			float speed = xd * vx + zd * vz;
			// 0.8 gives a stopped cart a 0.1 forward impulse from its rider.
			// Project the input onto the rail first so the cart starts along a flat
			// track instead of receiving a tiny sideways impulse that is discarded.
			if (rider && !rider->removed && Mth::abs(speed) < 0.1f &&
				(Mth::abs(riderForward) > 0.001f || Mth::abs(riderStrafe) > 0.001f)) {
				float yaw = rider->yRot / Mth::RADDEG;
				float forwardAlongRail = -Mth::sin(yaw) * vx + Mth::cos(yaw) * vz;
				float strafeAlongRail = Mth::cos(yaw) * vx + Mth::sin(yaw) * vz;
				float inputAlongRail = riderForward * forwardAlongRail + riderStrafe * strafeAlongRail;
				// A nearly perpendicular view has no useful projection. Keep keyboard
				// forward usable by starting in the rail's canonical positive direction.
				if (Mth::abs(inputAlongRail) < 0.25f && Mth::abs(riderForward) > 0.001f)
					inputAlongRail = riderForward;
				speed += inputAlongRail * 0.1f;
			}
			// 0.8 applies the incline impulse in world-axis direction before
			// orienting the cart, so account for which endpoint is higher.
			if (shape == 2 || shape == 5) speed -= 0.0078125f;
			else if (shape == 3 || shape == 4) speed += 0.0078125f;
			// Use the cart's existing heading when it enters a track.
			if (speed < 0) { vx = -vx; vz = -vz; speed = -speed; }
			// PE 0.8's Minecart::moveAlongTrack accelerates on every golden rail;
			// its entity code does not test the powered metadata bit here.
			if (railId == Tile::goldenRailCompat->id && speed > 0.01f)
				speed += 0.06f;
			if (speed < 0) speed = 0;
			if (speed > 0.4f) speed = 0.4f;
			xd = vx * speed;
			zd = vz * speed;
			yRot = Mth::atan2(vz, vx) * Mth::RADDEG;
			yd = 0;
			// The 0.8 rail endpoint table makes the high end of each slope
			// explicit. Follow that rise while traversing the segment.
			if (shape == 2) yd = vx * speed;
			else if (shape == 3) yd = -vx * speed;
			else if (shape == 4) yd = -vz * speed;
			else if (shape == 5) yd = vz * speed;
			xRot = Mth::atan2(yd, speed) * Mth::RADDEG;
			// Keep the bounding box and render origin on the rail surface. Assigning
			// y directly leaves the old box behind and makes the cart float.
			setPos(x, railY + 0.0625f, z);
			if (railId == Tile::goldenRailCompat->id && speed <= 0.01f) {
				if (shape == 1) {
					if (level->isSolidBlockingTile(bx - 1, railY, bz)) xd = 0.02f;
					else if (level->isSolidBlockingTile(bx + 1, railY, bz)) xd = -0.02f;
				} else if (shape == 0) {
					if (level->isSolidBlockingTile(bx, railY, bz - 1)) zd = 0.02f;
					else if (level->isSolidBlockingTile(bx, railY, bz + 1)) zd = -0.02f;
				}
			}
			float drag = 0.96f;
			xd *= drag; zd *= drag;
		} else {
			if (onGround) { xd *= 0.5f; yd *= 0.5f; zd *= 0.5f; }
		}
		move(xd, yd, zd);
		if (!onGround && !isRail(railId)) { xd *= 0.95f; yd *= 0.95f; zd *= 0.95f; }
		if (horizontalCollision) { xd *= -0.5f; zd *= -0.5f; }
		if (rider && !rider->removed) positionRider(rider);
	}
	void readAdditionalSaveData(CompoundTag*) {}
	void addAdditonalSaveData(CompoundTag*) {}

	private:
	float riderStrafe;
	float riderForward;
	static bool isRail(int id) {
		return (Tile::railCompat && id == Tile::railCompat->id) ||
			(Tile::goldenRailCompat && id == Tile::goldenRailCompat->id);
	}
	static void trackVector(int shape, float& x, float& z) {
		switch (shape) {
		case 0: case 4: case 5: x = 0; z = 1; break;
		case 1: case 2: case 3: x = 1; z = 0; break;
		// Match BaseRailTile::Rail::updateConnections endpoint pairs.
		case 7: case 9: x = 0.7071067f; z = 0.7071067f; break;
		case 6: case 8: x = 0.7071067f; z = -0.7071067f; break;
		default: x = 0; z = 1; break;
		}
	}
};

#endif

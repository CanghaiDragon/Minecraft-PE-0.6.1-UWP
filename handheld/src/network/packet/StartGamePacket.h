#ifndef NET_MINECRAFT_NETWORK_PACKET__StartGamePacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__StartGamePacket_H__

#include "../Packet.h"

class StartGamePacket : public Packet
{
public:
	long levelSeed;
	int levelGeneratorVersion;
	int gameType;

	int entityId;
	float x, y, z;
	// These fields are appended only in the project's extended protocol.  The
	// original PE packet remains byte-for-byte unchanged.
	int worldType;
	int indevType;
	bool hasExtendedWorldInfo;

	StartGamePacket()
	: levelSeed(0), levelGeneratorVersion(0), gameType(0), entityId(0),
	  x(0), y(0), z(0), worldType(0), indevType(0), hasExtendedWorldInfo(false)
	{
	}

	StartGamePacket(long seed, int levelGeneratorVersion, int gameType, int entityId, float x, float y, float z,
		int worldType = 0, int indevType = 0, bool hasExtendedWorldInfo = false)
	:	levelSeed(seed),
		levelGeneratorVersion(levelGeneratorVersion),
		gameType(gameType),
		entityId(entityId),
		x(x),
		y(y),
		z(z),
		worldType(worldType),
		indevType(indevType),
		hasExtendedWorldInfo(hasExtendedWorldInfo)
	{
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_STARTGAME));

		bitStream->Write(levelSeed);
		bitStream->Write(levelGeneratorVersion);
		bitStream->Write(gameType);
		bitStream->Write(entityId);
		bitStream->Write(x);
		bitStream->Write(y);
		bitStream->Write(z);
		if (hasExtendedWorldInfo) {
			bitStream->Write(worldType);
			bitStream->Write(indevType);
		}
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(levelSeed);
		bitStream->Read(levelGeneratorVersion);
		bitStream->Read(gameType);
		bitStream->Read(entityId);
		bitStream->Read(x);
		bitStream->Read(y);
		bitStream->Read(z);
		// Extended peers negotiate protocol 10 before this packet is sent.
		// Leave the legacy PE packet path untouched when no extension is present.
		if (bitStream->GetNumberOfUnreadBits() >= sizeof(int) * 16) {
			bitStream->Read(worldType);
			bitStream->Read(indevType);
			hasExtendedWorldInfo = true;
		}
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (StartGamePacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__StartGamePacket_H__*/

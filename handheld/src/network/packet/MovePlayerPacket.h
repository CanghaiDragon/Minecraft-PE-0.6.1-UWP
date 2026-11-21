#ifndef NET_MINECRAFT_NETWORK_PACKET__MovePlayerPacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__MovePlayerPacket_H__

#include "../Packet.h"

class MovePlayerPacket : public Packet
{
public:

	int entityId;
	float x, y, z, xRot, yRot;
	bool sneaking;
	bool hasExtendedState;

	MovePlayerPacket()
	: entityId(0), x(0), y(0), z(0), xRot(0), yRot(0),
	  sneaking(false), hasExtendedState(false)
	{
	}

	MovePlayerPacket(int entityId, float x, float y, float z, float xRot, float yRot,
		bool sneaking = false, bool hasExtendedState = false)
	:	entityId(entityId),
		x(x),
		y(y),
		z(z),
		xRot(xRot),
		yRot(yRot),
		sneaking(sneaking),
		hasExtendedState(hasExtendedState)
	{}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_MOVEPLAYER));

		bitStream->Write(entityId);
		bitStream->Write(x);
		bitStream->Write(y);
		bitStream->Write(z);
		bitStream->Write(yRot);
		bitStream->Write(xRot);
		if (hasExtendedState)
			bitStream->Write(sneaking);
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(entityId);
		bitStream->Read(x);
		bitStream->Read(y);
		bitStream->Read(z);
		bitStream->Read(yRot);
		bitStream->Read(xRot);
		if (bitStream->GetNumberOfUnreadBits() > 0) {
			bitStream->Read(sneaking);
			hasExtendedState = true;
		}
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (MovePlayerPacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__MovePlayerPacket_H__*/

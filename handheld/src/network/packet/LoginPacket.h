#ifndef NET_MINECRAFT_NETWORK_PACKET__LoginPacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__LoginPacket_H__

#include "../Packet.h"
#include <vector>

class LoginPacket : public Packet
{
public:
	RakNet::RakString clientName;
	int clientNetworkVersion;
	int clientNetworkLowestSupportedVersion;
	std::vector<unsigned char> skinPixels;
	bool hasExtendedSkin;

	LoginPacket()
	:	clientNetworkVersion(-1),
		clientNetworkLowestSupportedVersion(-1),
		hasExtendedSkin(false)
	{
	}

	LoginPacket(const RakNet::RakString& clientName, int clientVersion,
		const std::vector<unsigned char>& skinPixels = std::vector<unsigned char>(), bool hasExtendedSkin = false)
	:	clientName(clientName),
		clientNetworkVersion(clientVersion),
		clientNetworkLowestSupportedVersion(clientVersion),
		skinPixels(skinPixels),
		hasExtendedSkin(hasExtendedSkin)
	{
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_LOGIN));
		bitStream->Write(clientName);
		bitStream->Write(clientNetworkVersion);
		bitStream->Write(clientNetworkLowestSupportedVersion);
		if (hasExtendedSkin) {
			const int skinByteCount = skinPixels.size() == 64 * 64 * 4 ? (int)skinPixels.size() : 0;
			bitStream->Write(skinByteCount);
			if (skinByteCount > 0)
				bitStream->WriteAlignedBytes(&skinPixels[0], skinByteCount);
		}
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(clientName);
		// First versions didn't send the client version
		//LOGI("unread: %d\n", bitStream->GetNumberOfUnreadBits());
		if (bitStream->GetNumberOfUnreadBits() > 0) {
			bitStream->Read(clientNetworkVersion);
			bitStream->Read(clientNetworkLowestSupportedVersion);
		}
		if (bitStream->GetNumberOfUnreadBits() >= sizeof(int) * 8) {
			int skinByteCount = 0;
			bitStream->Read(skinByteCount);
			if (skinByteCount == 64 * 64 * 4) {
				skinPixels.resize(skinByteCount);
				if (!bitStream->ReadAlignedBytes(&skinPixels[0], skinByteCount))
					skinPixels.clear();
			}
			hasExtendedSkin = true;
		}
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (LoginPacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__LoginPacket_H__*/

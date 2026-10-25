#pragma once

#include <string>

// A deliberately narrow storage seam: it verifies the original MCPE
// ExternalFileLevelStorage level.dat format below UWP LocalCache.  It does
// not own a running Level and does not replace UwpCoreSession's temporary
// MemoryLevelStorageSource yet.
struct UwpLevelDatSmokeResult
{
	bool succeeded = false;
	std::wstring detail;
};

class UwpLevelDatSmoke final
{
public:
	static UwpLevelDatSmokeResult run();
};

#pragma once

#include <string>

class AppPlatform;
class Level;
class LocalPlayer;
class Minecraft;
class LevelRenderer;
class LevelSettings;
struct AppContext;

// Opaque bridge for the lifecycle-only original client translation units.
// The bridge is compiled with the original MCPE client classes and the
// UWP-safe NO_NETWORK/client-loop boundary; callers never depend on its
// private class layout or duplicate a Minecraft implementation.
using UwpOriginalClientHandle = void*;

enum class UwpOriginalMenuAction
{
	None,
	Singleplayer,
	SelectWorld,
	LeaveWorld,
	OpenSkin,
	OpenKeyMapping,
	OpenWorldEdit
};

UwpOriginalClientHandle UwpOriginalClientCreate();
bool UwpOriginalClientBindContext(UwpOriginalClientHandle handle,
	const AppContext& context);
bool UwpOriginalClientSetStoragePaths(UwpOriginalClientHandle handle,
	const std::string& externalStoragePath,
	const std::string& externalCacheStoragePath);
// Enters the original App::init -> NinecraftApp::init -> Minecraft::init
// chain after the UWP host has completed the packaged-texture preload.
bool UwpOriginalClientInitialize(UwpOriginalClientHandle handle,
	AppContext& context);
void UwpOriginalClientUpdate(UwpOriginalClientHandle handle);
// Advance one fixed world tick owned by UwpGameLoop.  Menu screens continue
// to use UwpOriginalClientUpdate(), which retains the normal Timer path.
void UwpOriginalClientFixedTick(UwpOriginalClientHandle handle);
// Draw one presented frame through Minecraft::renderFrame() without running
// another simulation tick or presenting the EGL surface.
void UwpOriginalClientRender(UwpOriginalClientHandle handle, float alpha);
// Keep the original Minecraft viewport in sync with the ANGLE surface before
// GameRenderer::render() runs.  This only updates the original client's
// resolution/configuration; it does not create a second surface or present.
void UwpOriginalClientSetSize(UwpOriginalClientHandle handle, int width, int height);
bool UwpOriginalClientShowStartMenu(UwpOriginalClientHandle handle);
bool UwpOriginalClientShowSelectWorld(UwpOriginalClientHandle handle);
// Shows the original MC error screen for a rejected legacy Infinite/Sky save.
// The screen deliberately stays inside the original client so it uses the
// native dirt background, MC font, and input/button rendering.
bool UwpOriginalClientShowLegacyWorldFormatError(UwpOriginalClientHandle handle);
// Shows the original ProgressScreen while the UWP-owned local world is being
// loaded. The UWP session remains the sole owner of the eventual Level and
// LocalPlayer; this screen only fills the no-Level render interval.
bool UwpOriginalClientShowProgress(UwpOriginalClientHandle handle);
bool UwpOriginalClientEnsureStartMenu(UwpOriginalClientHandle handle);
bool UwpOriginalClientWantsToQuit(UwpOriginalClientHandle handle);
UwpOriginalMenuAction UwpOriginalClientConsumeMenuAction(UwpOriginalClientHandle handle);
bool UwpOriginalClientSetSkinArmType(UwpOriginalClientHandle handle, bool slim);
bool UwpOriginalClientApplyImportedSkin(UwpOriginalClientHandle handle);
bool UwpOriginalClientClearImportedSkin(UwpOriginalClientHandle handle);
// Minecraft::selectLevel() is intentionally intercepted on UWP while the
// UWP session remains the sole Level/LocalPlayer owner.  The original
// SelectWorldScreen still performs the selection; MainPage consumes this
// request and starts that same saved world through UwpCoreSession.
void UwpOriginalClientRequestLevelSelection(const std::string& levelId,
	const std::string& levelName, const LevelSettings& settings);
// The original PauseScreen calls Minecraft::leaveGame().  A borrowed UWP
// world must be returned to its UwpCoreSession owner instead of being deleted
// by the desktop client path, so this request is consumed by MainPage after
// the original update returns.
void UwpOriginalClientRequestLeaveWorld();
void UwpOriginalClientRequestKeyMapping();
void UwpOriginalClientRequestWorldEdit(const std::string& worldId);
bool UwpOriginalClientConsumeLevelSelection(UwpOriginalClientHandle handle,
	std::string& levelId, std::string& levelName, long& seed,
	int& gameType, int& worldType);
bool UwpOriginalClientConsumeWorldEdit(UwpOriginalClientHandle handle,
	std::string& worldId);
Minecraft* UwpOriginalClientGetMinecraft(UwpOriginalClientHandle handle);
LevelRenderer* UwpOriginalClientGetLevelRenderer(UwpOriginalClientHandle handle);
bool UwpOriginalClientAttachWorld(UwpOriginalClientHandle handle,
	Level* world, LocalPlayer* localPlayer);
// Start the original RakNet server on the already-attached UWP world.  This
// is non-owning: UwpCoreSession remains responsible for the Level and player.
bool UwpOriginalClientHostMultiplayer(UwpOriginalClientHandle handle,
	int port);
void UwpOriginalClientDetachWorld(UwpOriginalClientHandle handle);
void UwpOriginalClientDestroy(UwpOriginalClientHandle handle);

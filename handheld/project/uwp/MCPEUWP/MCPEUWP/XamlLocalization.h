#pragma once

#include "../../../../src/locale/I18n.h"

#include <windows.h>
#include <cstring>
#include <cwchar>
#include <string>

#include <windows.ui.xaml.h>
#include <windows.ui.xaml.controls.h>
#include <windows.ui.xaml.documents.h>
#include <windows.ui.xaml.media.h>

namespace UwpXamlLocalization
{
struct TextEntry
{
	const wchar_t* english;
	const char* key;
};

inline const TextEntry* entries(size_t& count)
{
	static const TextEntry values[] = {
		{ L"Key Mapping", "uwp.keyMapping.title" },
		{ L"Select a button, then press a keyboard key", "uwp.keyMapping.instructions" },
		{ L"Keyboard bindings are saved automatically.", "uwp.keyMapping.saved" },
		{ L"Move Forward", "uwp.key.moveForward" },
		{ L"Move Left", "uwp.key.moveLeft" },
		{ L"Move Back", "uwp.key.moveBack" },
		{ L"Move Right", "uwp.key.moveRight" },
		{ L"Jump", "key.jump" },
		{ L"Sneak", "key.sneak" },
		{ L"Open Inventory", "uwp.key.inventory" },
		{ L"Toggle Run (Creative)", "uwp.key.run" },
		{ L"Toggle Spectator (Creative)", "uwp.key.spectator" },
		{ L"Drop Item", "key.drop" },
		{ L"Crafting (Survival)", "uwp.key.crafting" },
		{ L"Toggle Hide HUD", "uwp.key.hideHud" },
		{ L"Toggle Debug Function", "uwp.key.debug" },
		{ L"Take Screenshot", "uwp.key.screenshot" },
		{ L"Open Chat", "uwp.key.chat" },
		{ L"Toggle Third-person View", "uwp.key.thirdPerson" },
		{ L"Pause / Back", "uwp.key.pause" },
		{ L"Hotbar Slot 1", "uwp.key.hotbar1" },
		{ L"Hotbar Slot 2", "uwp.key.hotbar2" },
		{ L"Hotbar Slot 3", "uwp.key.hotbar3" },
		{ L"Hotbar Slot 4", "uwp.key.hotbar4" },
		{ L"Hotbar Slot 5", "uwp.key.hotbar5" },
		{ L"Hotbar Slot 6", "uwp.key.hotbar6" },
		{ L"Hotbar Slot 7", "uwp.key.hotbar7" },
		{ L"Hotbar Slot 8", "uwp.key.hotbar8" },
		{ L"Mine / Attack", "uwp.key.mineAttack" },
		{ L"Place Block / Interact", "uwp.key.placeInteract" },
		{ L"Scroll Hotbar", "uwp.key.scrollHotbar" },
		{ L"Press a key to assign it, or press Escape to cancel.", "uwp.keyMapping.pressKey" },
		{ L"Key assigned. Keyboard bindings are saved automatically.", "uwp.keyMapping.assigned" },
		{ L"Keyboard bindings restored to defaults.", "uwp.keyMapping.reset" },
		{ L"Mouse Left", "uwp.key.mouseLeft" },
		{ L"Mouse Right", "uwp.key.mouseRight" },
		{ L"Mouse Wheel", "uwp.key.mouseWheel" },
		{ L"Space", "uwp.key.space" },
		{ L"Left Shift", "uwp.key.leftShift" },
		{ L"Left Ctrl", "uwp.key.leftCtrl" },
		{ L"Escape", "uwp.key.escape" },
		{ L"Enter", "uwp.key.enter" },
		{ L"Backspace", "uwp.key.backspace" },
		{ L"Reset Defaults", "options.restoreDefaults" },
		{ L"Back", "gui.back" },
		{ L"Skin", "options.category.skin" },
		{ L"Arm Type", "options.skinArmType" },
		{ L"Classic", "options.skinArmType.classic" },
		{ L"Slim", "options.skinArmType.slim" },
		{ L"Use a 64x64 PNG skin.", "uwp.skin.description" },
		{ L"Slim arm type selected.", "uwp.skin.slimSelected" },
		{ L"Classic arm type selected.", "uwp.skin.classicSelected" },
		{ L"Skin must be a 64x64 PNG file.", "uwp.skin.invalid" },
		{ L"Custom skin imported and applied.", "uwp.skin.imported" },
		{ L"Custom skin is active.", "uwp.skin.active" },
		{ L"Using the default Steve skin.", "uwp.skin.defaultActive" },
		{ L"Custom skin is active.", "uwp.skin.active" },
		{ L"Choose a 64x64 PNG skin...", "uwp.skin.choose" },
		{ L"Skin import cancelled.", "uwp.skin.cancelled" },
		{ L"Skin import failed. Check that the file is a valid PNG.", "uwp.skin.importFailed" },
		{ L"Clearing custom skin...", "uwp.skin.clearing" },
		{ L"Custom skin cleared.", "uwp.skin.cleared" },
		{ L"Could not clear the custom skin file.", "uwp.skin.clearFailed" },
		{ L"Import Skin", "options.importSkin" },
		{ L"Clear Skin", "uwp.skin.clear" },
		{ L"Options", "options.title" },
		{ L"Game", "options.category.game" },
		{ L"Controls", "options.category.controls" },
		{ L"Graphics", "options.category.graphics" },
		{ L"Audio", "options.category.audio" },
		{ L"Additional", "options.category.additional" },
		{ L"Username", "options.username" },
		{ L"Difficulty", "options.difficulty" },
		{ L"Peaceful", "options.difficulty.peaceful" },
		{ L"Normal", "options.difficulty.normal" },
		{ L"Camera", "uwp.settings.camera" },
		{ L"First Person", "options.thirdperson.first" },
		{ L"Third Person Back", "options.thirdperson.back" },
		{ L"Third Person Front", "options.thirdperson.front" },
		{ L"Open to LAN by Default", "options.servervisible" },
		{ L"Dynamic Tile (Win10 only)", "options.dynamicTile" },
		{ L"Off", "options.dynamicTile.off" },
		{ L"Simple", "options.dynamicTile.simple" },
		{ L"Full", "options.dynamicTile.full" },
		{ L"Input Mode", "options.inputMode" },
		{ L"Keyboard & Mouse", "options.inputMode.keyboardMouse" },
		{ L"Touch", "options.inputMode.touch" },
		{ L"Sensitivity", "options.sensitivity" },
		{ L"D-Pad Size", "options.dpadSize" },
		{ L"Small", "options.dpadSize.small" },
		{ L"Large", "options.dpadSize.large" },
		{ L"Left-Handed Controls", "options.lefthanded" },
		{ L"Split Touch Controls", "options.usetouchpad" },
		{ L"Invert Mouse", "options.invertMouse" },
		{ L"Vibrate on Destroy", "options.destroyvibration" },
		{ L"Fancy", "options.graphics.fancy" },
		{ L"Fast", "options.graphics.fast" },
		{ L"Smooth Lighting", "options.ao" },
		{ L"Render Distance", "options.renderDistance" },
		{ L"Tiny", "options.renderDistance.tiny" },
		{ L"Short", "options.renderDistance.short" },
		{ L"Far", "options.renderDistance.far" },
		{ L"GUI Scale", "options.guiScale" },
		{ L"Auto", "options.guiScale.auto" },
		{ L"Medium", "options.guiScale.medium" },
		{ L"Field of View", "options.fov" },
		{ L"View Bobbing", "options.viewBobbing" },
		{ L"Hide HUD", "options.hidegui" },
		{ L"Music", "options.music" },
		{ L"Sound", "options.sound" },
		{ L"Extra World Type", "options.extraWorldType" },
		{ L"Java UI", "options.javaUi" },
		{ L"Optimized Skin", "options.skinMenu" },
		{ L"Touch Sneak", "options.touchSneak" },
		{ L"Java Beta Graphic Visual", "options.betaVisuals" },
		{ L"Debug Function", "options.debugScreen" },
		{ L"Restore Default Settings", "options.restoreDefaults" },
		{ L"Unavailable", "options.unavailable" },
		{ L"Survival", "gameMode.survival" },
		{ L"Creative", "gameMode.creative" },
		{ L"Hardcore", "gameMode.hardcore" },
		{ L"Infinite", "generator.infinite" },
		{ L"Old", "generator.old" },
		{ L"Flat", "generator.flat" },
		{ L"Sky", "generator.sky" },
		{ L"Alpha", "generator.alpha" },
		{ L"Inland", "generator.indev.inland" },
		{ L"Island", "generator.indev.island" },
		{ L"Floating", "generator.indev.floating" },
		{ L"Unlocked", "achievements.unlocked" },
		{ L"Locked", "achievements.locked" },
		{ L"Taking Inventory", "achievement.pe.0.title" },
		{ L"Benchmarking", "achievement.pe.1.title" },
		{ L"Time to Mine!", "achievement.pe.2.title" },
		{ L"Hot Topic", "achievement.pe.3.title" },
		{ L"Getting an Upgrade", "achievement.pe.4.title" },
		{ L"Acquire Hardware", "achievement.pe.5.title" },
		{ L"Bake Bread", "achievement.pe.6.title" },
		{ L"Monster Hunter", "achievement.pe.7.title" },
		{ L"Full of Diamond", "achievement.pe.8.title" },
		{ L"The End?", "achievement.pe.9.title" },
		{ L"Farming", "achievement.pe.10.title" },
		{ L"DIAMONDS!", "achievement.pe.11.title" },
		{ L"Overkill", "achievement.pe.12.title" },
		{ L"War Pig", "achievement.pe.13.title" },
		{ L"Sweet Dream", "achievement.pe.14.title" },
		{ L"Open your inventory.", "achievement.pe.0.desc" },
		{ L"Craft a workbench.", "achievement.pe.1.desc" },
		{ L"Craft a wooden pickaxe.", "achievement.pe.2.desc" },
		{ L"Craft a furnace.", "achievement.pe.3.desc" },
		{ L"Craft a stone pickaxe.", "achievement.pe.4.desc" },
		{ L"Obtain an iron ingot.", "achievement.pe.5.desc" },
		{ L"Obtain bread.", "achievement.pe.6.desc" },
		{ L"Defeat a hostile creature.", "achievement.pe.7.desc" },
		{ L"Wear a full set of diamond armor.", "achievement.pe.8.desc" },
		{ L"Activate a Nether Reactor.", "achievement.pe.9.desc" },
		{ L"Obtain wheat seeds.", "achievement.pe.10.desc" },
		{ L"Obtain a diamond.", "achievement.pe.11.desc" },
		{ L"Defeat all four hostile mob types.", "achievement.pe.12.desc" },
		{ L"Kill a zombie pigman.", "achievement.pe.13.desc" },
		{ L"Sleep in a bed.", "achievement.pe.14.desc" },
		{ L"World Edit", "uwp.worldEdit.title" },
		{ L"Folder name", "uwp.worldEdit.folderName" },
		{ L"World name", "selectWorld.enterName" },
		{ L"Last opened", "uwp.worldEdit.lastOpened" },
		{ L"Play time", "uwp.worldEdit.playTime" },
		{ L"Seed", "uwp.worldEdit.seed" },
		{ L"Game mode", "selectWorld.gameMode" },
		{ L"World type", "selectWorld.mapType" },
		{ L"Size on disk", "uwp.worldEdit.sizeOnDisk" },
		{ L"StorageVersion", "uwp.worldEdit.storageVersion" },
		{ L"PIN to start menu", "uwp.worldEdit.pin" },
		{ L"Convert to PE 0.9+", "uwp.worldEdit.convert" },
		{ L"Upgrade to Infinite", "uwp.worldEdit.upgrade" },
		{ L"View Achievements", "achievements.title" },
		{ L"Convert to new format", "uwp.worldEdit.convertFormat" },
		{ L"Delete", "selectWorld.delete" },
		{ L"Export", "uwp.worldEdit.export" },
		{ L"Save", "uwp.save" },
		{ L"Cancel", "gui.cancel" },
		{ L"Unlocked: 0 / 15", "uwp.achievements.summary" },
		{ L"Unavailable", "options.unavailable" },
		{ L"Editor shell loaded.", "uwp.worldEdit.loaded" },
		{ L"World information loaded.", "uwp.worldEdit.loadedInfo" },
		{ L"Legacy world format detected. Convert it before opening the world.", "uwp.worldEdit.legacyFormat" },
		{ L"Could not read level.dat.", "uwp.worldEdit.readFailed" },
		{ L"World deleted.", "uwp.worldEdit.deleted" },
		{ L"Could not delete the world.", "uwp.worldEdit.deleteFailed" },
		{ L"Folder name cannot be empty.", "uwp.worldEdit.folderEmpty" },
		{ L"World name cannot be empty.", "uwp.worldEdit.nameEmpty" },
		{ L"The folder name is unchanged.", "uwp.worldEdit.folderUnchanged" },
		{ L"Folder name changed.", "uwp.worldEdit.folderChanged" },
		{ L"Could not change the folder name.", "uwp.worldEdit.folderChangeFailed" },
		{ L"World name changed.", "uwp.worldEdit.nameChanged" },
		{ L"Could not change the world name.", "uwp.worldEdit.nameChangeFailed" },
		{ L"Choose a destination folder...", "uwp.worldEdit.chooseFolder" },
		{ L"Export cancelled.", "uwp.worldEdit.exportCancelled" },
		{ L"World exported as a ZIP archive.", "uwp.worldEdit.exported" },
		{ L"Could not export the world as a ZIP archive.", "uwp.worldEdit.exportFailed" },
		{ L"Delete World?", "uwp.worldEdit.deleteTitle" },
		{ L"This permanently deletes the selected world.", "uwp.worldEdit.deleteMessage" },
		{ L"Rename folder", "uwp.worldEdit.renameFolder" },
		{ L"Rename world", "uwp.worldEdit.renameWorld" },
		{ L"Opening the finite world upgrade screen...", "uwp.worldEdit.openUpgrade" },
		{ L"Opening the world format conversion screen...", "uwp.worldEdit.openConvert" },
		{ L"Updating world shortcut...", "uwp.worldEdit.updatingShortcut" },
		{ L"Pinning world shortcut...", "uwp.worldEdit.pinningShortcut" },
		{ L"World shortcut pinned.", "uwp.worldEdit.shortcutPinned" },
		{ L"Pin request cancelled.", "uwp.worldEdit.pinCancelled" },
		{ L"Could not pin world shortcut.", "uwp.worldEdit.pinFailed" },
		{ L"Could not prepare world shortcut.", "uwp.worldEdit.prepareShortcutFailed" },
		{ L"PE 0.9 / Infinite", "generator.infinite" },
		{ L"Sky / Nether Reactor", "generator.sky" },
		{ L"Early Infdev", "uwp.worldEdit.earlyInfdev" },
		{ L"Old / Finite", "generator.old" },
		{ L"Infinite Legacy (Unsupported)", "uwp.worldEdit.infiniteLegacy" },
		{ L"Sky Legacy (Unsupported)", "uwp.worldEdit.skyLegacy" },
		{ L"Alpha Legacy (Unsupported)", "uwp.worldEdit.alphaLegacy" },
		{ L"Indev / Finite", "generator.indev" },
		{ L"Early Infdev Legacy (Unsupported)", "uwp.worldEdit.earlyInfdevLegacy" },
		{ L"Not configured", "uwp.worldConvert.notConfigured" },
		{ L"Choose a ZIP file to begin.", "uwp.worldImport.chooseZip" },
		{ L"Extracting ZIP archive...", "uwp.worldImport.extracting" },
		{ L"Could not import ZIP.", "uwp.worldImport.failed" },
		{ L"Import World", "uwp.worldImport.title" },
		{ L"Select a .zip world archive to extract into minecraftWorlds.", "uwp.worldImport.description" },
		{ L"Choose a ZIP file to begin.", "uwp.worldImport.chooseZip" },
		{ L"IMPORT SUCCESS", "uwp.worldImport.success" },
		{ L"Select ZIP", "uwp.worldImport.selectZip" },
		{ L"Select a ZIP file...", "uwp.worldImport.selectZipHint" },
		{ L"Import cancelled.", "uwp.worldImport.cancelled" },
		{ L"Import rejected: unsupported ZIP file name.", "uwp.worldImport.unsupportedName" },
		{ L"Checking world format...", "uwp.worldImport.checking" },
		{ L"Import rejected: multiple level.dat files were found.", "uwp.worldImport.multipleLevels" },
		{ L"OK", "gui.done" },
		{ L"Choose Start Icon", "uwp.worldIcon.title" },
		{ L"Icon may not function on Win11, just randomly choose one", "uwp.worldIcon.description" },
		{ L"Convert to PE 0.9+", "uwp.worldConvert.title" },
		{ L"Source world", "uwp.worldConvert.source" },
		{ L"Not configured", "uwp.worldConvert.notConfigured" },
		{ L"Output ZIP file", "uwp.worldConvert.output" },
		{ L"Choose a destination ZIP file after clicking Convert", "uwp.worldConvert.destinationHint" },
		{ L"Convert at most 64 chunks near the saved player (test)", "uwp.worldConvert.chunkLimit" },
		{ L"Convert to 1.17+ void world", "uwp.worldConvert.voidWorld" },
		{ L"Lower sea level: clear y=63 water/ice and restore grass at y=62", "uwp.worldConvert.lowerSea" },
		{ L"Winter: freeze remaining y=62 water (requires lower sea level)", "uwp.worldConvert.winter" },
		{ L"Sky: fill y=0-4 air with bedrock", "uwp.worldConvert.sky" },
		{ L"Floating: replace y=0 with bedrock", "uwp.worldConvert.floatingBedrock" },
		{ L"Configure a source world to begin.", "uwp.worldConvert.ready" },
		{ L"Conversion is unavailable until this world is opened and saved in the game to upgrade it to the latest UWP storage format.", "uwp.worldConvert.needsUpgrade" },
		{ L"Conversion to PE 0.9+", "uwp.worldConvert.peTitle" },
		{ L"Conversion blocked. Open and save this world in the game first to upgrade its UWP storage format.", "uwp.worldConvert.blocked" },
		{ L"Bedrock 1.17+ void conversion is selected. Existing Sky chunks will be converted and new terrain will remain void.", "uwp.worldConvert.voidSelected" },
		{ L"Full conversion is enabled by default. The original world is not modified.", "uwp.worldConvert.fullSelected" },
		{ L"This world type is not supported by the PE 0.9 converter.", "uwp.worldConvert.unsupported" },
		{ L"Bedrock 1.17+ void conversion selected. New chunks will be pure void.", "uwp.worldConvert.voidSelectedShort" },
		{ L"PE 0.9 conversion selected.", "uwp.worldConvert.peSelected" },
		{ L"No source world is configured.", "uwp.worldConvert.noSource" },
		{ L"Choose a destination ZIP file...", "uwp.worldConvert.chooseDestination" },
		{ L"ZIP archive", "uwp.worldConvert.zipArchive" },
		{ L"Converting the world and preparing the ZIP file...", "uwp.worldConvert.converting" },
		{ L"Conversion cancelled.", "uwp.worldConvert.cancelled" },
		{ L"This world cannot be imported by Minecraft PE 0.6.1.", "uwp.worldImport.incompatible" },
		{ L"The ZIP does not contain a readable level.dat.", "uwp.worldImport.invalidWorld" },
		{ L"World Cannot Be Imported", "uwp.worldImport.rejectedTitle" },
		{ L"The selected ZIP contains multiple level.dat files. Please select one world archive at a time.", "uwp.worldImport.multipleLevelsMessage" },
		{ L"The ZIP file name contains characters that Minecraft PE 0.6.1 cannot recognize. Please rename it using ASCII characters only.", "uwp.worldImport.badNameMessage" },
		{ L"The selected ZIP does not contain a valid Minecraft PE world.", "uwp.worldImport.invalidWorldMessage" },
		{ L"cannot open selected ZIP", "uwp.worldImport.errOpen" },
		{ L"cannot seek selected ZIP", "uwp.worldImport.errSeek" },
		{ L"cannot read selected ZIP", "uwp.worldImport.errReadSelected" },
		{ L"cannot read ZIP file", "uwp.worldImport.errRead" },
		{ L"ZIP end directory not found", "uwp.worldImport.errEndDirectory" },
		{ L"ZIP central directory is invalid", "uwp.worldImport.errCentralDirectory" },
		{ L"ZIP central directory entry is invalid", "uwp.worldImport.errCentralEntry" },
		{ L"ZIP entry header is truncated", "uwp.worldImport.errEntryHeader" },
		{ L"ZIP local entry header is invalid", "uwp.worldImport.errLocalHeader" },
		{ L"ZIP entry data is truncated", "uwp.worldImport.errEntryData" },
		{ L"ZIP entry name is invalid", "uwp.worldImport.errEntryName" },
		{ L"ZIP compression method is unsupported", "uwp.worldImport.errCompression" },
		{ L"failed to decompress ZIP entry", "uwp.worldImport.errDecompress" },
		{ L"ZIP entry size does not match its header", "uwp.worldImport.errEntrySize" },
		{ L"level.dat is too small to contain a format version", "uwp.worldImport.errLevelSmall" },
		{ L"level.dat was not found in the selected ZIP", "uwp.worldImport.errLevelMissing" },
		{ L"The selected ZIP contains multiple level.dat files", "uwp.worldImport.errMultipleLevels" },
		{ L"cannot create destination folder", "uwp.worldImport.errDestination" },
		{ L"This shortcut points to a world that is no longer available.", "uwp.main.shortcutUnavailable" },
		{ L"The saved world could not be attached to the game.", "uwp.main.worldAttachFailed" },
		{ L"World editor could not be opened", "uwp.main.editorFailed" },
		{ L"Achievements", "achievements.title" },
		{ L"Language", "options.category.language" },
		{ L"Original", "options.language.mode.origin" },
		{ L"Follow System", "options.language.mode.system" },
		{ L"English", "options.language.mode.english" },
		{ L"Simplified Chinese", "options.language.mode.simplifiedChinese" },
		{ L"Minecraft PE 0.6.1 Sample text", "options.language.mode.origin.description" },
		{ L"World Edit", "uwp.worldEdit.title" },
		{ L"Rename", "uwp.worldEdit.rename" },
		{ L"Delete", "selectWorld.delete" },
		{ L"Export", "uwp.worldEdit.export" },
		{ L"Join Game", "menu.pe.joinGame" },
		{ L"Start Game", "menu.pe.startGame" }
	};
	count = sizeof(values) / sizeof(values[0]);
	return values;
}

inline std::wstring widenUtf8(const std::string& value)
{
	if (value.empty())
		return std::wstring();
	const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
	if (length <= 1)
		return std::wstring(value.begin(), value.end());
	std::wstring output(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &output[0], length);
	output.resize(static_cast<size_t>(length - 1));
	return output;
}

inline Platform::String^ get(const char* key)
{
	const std::string value = I18n::get(key);
	if (value == std::string(key) + "<")
		return nullptr;
	const std::wstring wide = widenUtf8(value);
	return ref new Platform::String(wide.c_str());
}

inline const char* findKey(Platform::String^ value)
{
	if (value == nullptr)
		return nullptr;
	const std::wstring text(value->Data());
	size_t count = 0;
	const TextEntry* table = entries(count);
	for (size_t i = 0; i < count; ++i)
		if (text == table[i].english)
			return table[i].key;
	return nullptr;
}

inline const char* taggedKey(Windows::UI::Xaml::FrameworkElement^ element)
{
	if (element == nullptr)
		return nullptr;
	Platform::String^ tag = dynamic_cast<Platform::String^>(element->Tag);
	if (tag == nullptr || tag->Length() < 9 || std::wcsncmp(tag->Data(), L"__xloc__:", 9) != 0)
		return nullptr;
	std::wstring wide(tag->Data() + 9);
	const size_t renderedSeparator = wide.find(L'|');
	if (renderedSeparator != std::wstring::npos)
		wide.resize(renderedSeparator);
	static std::string key;
	key.assign(wide.begin(), wide.end());
	return key.c_str();
}

inline Platform::String^ taggedRenderedText(Windows::UI::Xaml::FrameworkElement^ element)
{
	if (element == nullptr)
		return nullptr;
	Platform::String^ tag = dynamic_cast<Platform::String^>(element->Tag);
	if (tag == nullptr || tag->Length() < 10 || std::wcsncmp(tag->Data(), L"__xloc__:", 9) != 0)
		return nullptr;
	const wchar_t* separator = std::wcschr(tag->Data() + 9, L'|');
	return separator == nullptr ? nullptr : ref new Platform::String(separator + 1);
}

inline void writeLocalizationTag(Windows::UI::Xaml::FrameworkElement^ element,
	const char* key,
	Platform::String^ renderedText)
{
	if (element == nullptr || key == nullptr)
		return;
	std::wstring tag = L"__xloc__:";
	tag.append(key, key + std::strlen(key));
	if (renderedText != nullptr)
	{
		tag.push_back(L'|');
		tag.append(renderedText->Data());
	}
	element->Tag = ref new Platform::String(tag.c_str());
}

inline bool containsNonAscii(Platform::String^ value)
{
	if (value != nullptr)
	{
		for (const wchar_t* ch = value->Data(); *ch; ++ch)
			if (*ch > 0x7f)
				return true;
	}
	return false;
}

inline const wchar_t* fontForText(Platform::String^ value)
{
	if (containsNonAscii(value))
		return L"ms-appx:///data/fonts/unifont-18.0.01.otf#Unifont";
	return L"ms-appx:///data/fonts/minecraft.ttf#Minecraft";
}

inline void applyControlFont(Windows::UI::Xaml::FrameworkElement^ element,
	Platform::String^ value)
{
	Windows::UI::Xaml::Controls::Control^ control =
		dynamic_cast<Windows::UI::Xaml::Controls::Control^>(element);
	if (control != nullptr)
	{
		Platform::String^ fontSource = ref new Platform::String(fontForText(value));
		control->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(fontSource);
	}
}

inline void localizeValue(Windows::UI::Xaml::FrameworkElement^ element,
	Platform::String^ value,
	void(*setter)(Windows::UI::Xaml::FrameworkElement^, Platform::String^))
{
	if (element == nullptr || value == nullptr || setter == nullptr)
		return;
	const char* key = taggedKey(element);
	if (key == nullptr && element->Tag != nullptr)
		return;
	if (key == nullptr)
		key = findKey(value);
	if (key == nullptr)
		return;
	Platform::String^ localized = get(key);
	if (localized == nullptr)
		return;
	Windows::UI::Xaml::Controls::TextBlock^ block =
		dynamic_cast<Windows::UI::Xaml::Controls::TextBlock^>(element);
	Platform::String^ current = value;
	bool hasCachedRenderedText = false;
	if (block != nullptr)
	{
		Platform::String^ cached = taggedRenderedText(element);
		if (cached != nullptr)
		{
			current = cached;
			hasCachedRenderedText = true;
		}
	}
	const bool needsSet = current != localized ||
		(block != nullptr && !hasCachedRenderedText && containsNonAscii(localized));
	if (needsSet)
		setter(element, localized);
	if (block != nullptr)
	{
		if (needsSet || !hasCachedRenderedText)
			writeLocalizationTag(element, key, localized);
	}
	else if (taggedKey(element) == nullptr)
		writeLocalizationTag(element, key, nullptr);
}

inline void setText(Windows::UI::Xaml::FrameworkElement^ element, Platform::String^ value)
{
	Windows::UI::Xaml::Controls::TextBlock^ block =
		dynamic_cast<Windows::UI::Xaml::Controls::TextBlock^>(element);
	if (block == nullptr || value == nullptr)
		return;
	if (block->Name == L"StorageVersionLabelText")
		block->FontSize = containsNonAscii(value) ? 18.0 : 14.0;
	block->Text = L"";
	block->Inlines->Clear();
	block->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(
		L"ms-appx:///data/fonts/minecraft.ttf#Minecraft");

	const std::wstring text(value->Data());
	size_t start = 0;
	while (start < text.size())
	{
		const bool minecraftRun = text[start] <= 0x7f;
		size_t end = start + 1;
		while (end < text.size() && (text[end] <= 0x7f) == minecraftRun)
			++end;

		Windows::UI::Xaml::Documents::Run^ run =
			ref new Windows::UI::Xaml::Documents::Run();
		run->Text = ref new Platform::String(text.substr(start, end - start).c_str());
		const wchar_t* runFontUri = minecraftRun ?
			L"ms-appx:///data/fonts/minecraft.ttf#Minecraft" :
			L"ms-appx:///data/fonts/unifont-18.0.01.otf#Unifont";
		Platform::String^ runFontSource = ref new Platform::String(runFontUri);
		run->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily(runFontSource);
		block->Inlines->Append(run);
		start = end;
	}
}

inline void setContent(Windows::UI::Xaml::FrameworkElement^ element, Platform::String^ value)
{
	Windows::UI::Xaml::Controls::ContentControl^ control =
		dynamic_cast<Windows::UI::Xaml::Controls::ContentControl^>(element);
	if (control != nullptr)
	{
		control->Content = value;
		applyControlFont(element, value);
	}
}

inline void setHeader(Windows::UI::Xaml::FrameworkElement^ element, Platform::String^ value)
{
	Windows::UI::Xaml::Controls::TextBox^ textBox =
		dynamic_cast<Windows::UI::Xaml::Controls::TextBox^>(element);
	if (textBox != nullptr)
	{
		textBox->Header = value;
		applyControlFont(element, value);
	}
	Windows::UI::Xaml::Controls::ComboBox^ comboBox =
		dynamic_cast<Windows::UI::Xaml::Controls::ComboBox^>(element);
	if (comboBox != nullptr)
	{
		comboBox->Header = value;
		applyControlFont(element, value);
	}
	Windows::UI::Xaml::Controls::Slider^ slider =
		dynamic_cast<Windows::UI::Xaml::Controls::Slider^>(element);
	if (slider != nullptr)
	{
		slider->Header = value;
		applyControlFont(element, value);
	}
}

inline void setPlaceholder(Windows::UI::Xaml::FrameworkElement^ element, Platform::String^ value)
{
	Windows::UI::Xaml::Controls::TextBox^ textBox =
		dynamic_cast<Windows::UI::Xaml::Controls::TextBox^>(element);
	if (textBox != nullptr)
	{
		textBox->PlaceholderText = value;
		applyControlFont(element, value);
	}
}

inline void localizeComboItems(Windows::UI::Xaml::Controls::ComboBox^ comboBox)
{
	if (comboBox == nullptr)
		return;
	const char* const* keys = nullptr;
	static const char* difficulty[] = { "options.difficulty.peaceful", "options.difficulty.normal" };
	static const char* camera[] = { "options.thirdperson.first", "options.thirdperson.back", "options.thirdperson.front" };
	static const char* dynamicTile[] = { "options.dynamicTile.off", "options.dynamicTile.simple", "options.dynamicTile.full" };
	static const char* inputMode[] = { "options.inputMode.keyboardMouse", "options.inputMode.touch" };
	static const char* graphics[] = { "options.graphics.fancy", "options.graphics.fast" };
	static const char* renderDistance[] = { "options.renderDistance.tiny", "options.renderDistance.short", "options.renderDistance.normal", "options.renderDistance.far" };
	static const char* guiScale[] = { "options.guiScale.auto", "options.guiScale.small", "options.guiScale.medium", "options.guiScale.normal", "options.guiScale.large" };
	static const char* dpadSize[] = { "options.dpadSize.small", "options.dpadSize.normal", "options.dpadSize.large" };
	static const char* language[] = { "options.language.mode.origin", "options.language.mode.system", "options.language.mode.english", "options.language.mode.simplifiedChinese" };
	if (comboBox->Name == L"DifficultyComboBox") keys = difficulty;
	else if (comboBox->Name == L"CameraComboBox") keys = camera;
	else if (comboBox->Name == L"DynamicTileComboBox") keys = dynamicTile;
	else if (comboBox->Name == L"InputModeComboBox") keys = inputMode;
	else if (comboBox->Name == L"GraphicsModeComboBox") keys = graphics;
	else if (comboBox->Name == L"RenderDistanceComboBox") keys = renderDistance;
	else if (comboBox->Name == L"GuiScaleComboBox") keys = guiScale;
	else if (comboBox->Name == L"DpadSizeComboBox") keys = dpadSize;
	else if (comboBox->Name == L"LanguageComboBox") keys = language;
	if (keys == nullptr)
		return;
	for (unsigned int i = 0; i < comboBox->Items->Size; ++i)
	{
		Windows::UI::Xaml::Controls::ComboBoxItem^ item =
			dynamic_cast<Windows::UI::Xaml::Controls::ComboBoxItem^>(comboBox->Items->GetAt(i));
		if (item == nullptr || item->Content == nullptr)
			continue;
		Platform::String^ marker = dynamic_cast<Platform::String^>(item->Tag);
		if (marker == nullptr)
		{
			const std::string key(keys[i]);
			const std::wstring wideKey(key.begin(), key.end());
			item->Tag = ref new Platform::String((std::wstring(L"__xloc__:") + wideKey).c_str());
		}
		Platform::String^ value = dynamic_cast<Platform::String^>(item->Content);
		if (value != nullptr)
			localizeValue(item, value, setContent);
	}
}

inline void localizeTree(Windows::UI::Xaml::DependencyObject^ node)
{
	if (node == nullptr)
		return;
	Windows::UI::Xaml::FrameworkElement^ element =
		dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(node);
	if (element != nullptr)
	{
		Windows::UI::Xaml::Controls::TextBlock^ block =
			dynamic_cast<Windows::UI::Xaml::Controls::TextBlock^>(element);
		if (block != nullptr)
		{
			Platform::String^ text = block->Text;
			if (text == nullptr || text->Length() == 0)
			{
				Platform::String^ cached = taggedRenderedText(element);
				if (cached != nullptr)
					text = cached;
			}
			localizeValue(element, text, setText);
		}
		Windows::UI::Xaml::Controls::ContentControl^ control =
			dynamic_cast<Windows::UI::Xaml::Controls::ContentControl^>(element);
		if (control != nullptr)
		{
			Platform::String^ content = dynamic_cast<Platform::String^>(control->Content);
			if (content != nullptr)
				localizeValue(element, content, setContent);
		}
		Windows::UI::Xaml::Controls::TextBox^ textBox =
			dynamic_cast<Windows::UI::Xaml::Controls::TextBox^>(element);
		if (textBox != nullptr)
		{
			Platform::String^ header = dynamic_cast<Platform::String^>(textBox->Header);
			if (header != nullptr) localizeValue(element, header, setHeader);
			Platform::String^ placeholder = textBox->PlaceholderText;
			if (placeholder != nullptr) localizeValue(element, placeholder, setPlaceholder);
		}
		Windows::UI::Xaml::Controls::ComboBox^ comboBox =
			dynamic_cast<Windows::UI::Xaml::Controls::ComboBox^>(element);
		if (comboBox != nullptr)
		{
			Platform::String^ header = dynamic_cast<Platform::String^>(comboBox->Header);
			if (header != nullptr) localizeValue(element, header, setHeader);
			localizeComboItems(comboBox);
		}
		Windows::UI::Xaml::Controls::Slider^ slider =
			dynamic_cast<Windows::UI::Xaml::Controls::Slider^>(element);
		if (slider != nullptr)
		{
			Platform::String^ header = dynamic_cast<Platform::String^>(slider->Header);
			if (header != nullptr) localizeValue(element, header, setHeader);
		}
	}
	const int childCount = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(node);
	for (int i = 0; i < childCount; ++i)
		localizeTree(Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(node, i));
	Windows::UI::Xaml::Controls::ItemsControl^ itemsControl =
		dynamic_cast<Windows::UI::Xaml::Controls::ItemsControl^>(node);
	if (itemsControl != nullptr)
	{
		for (unsigned int i = 0; i < itemsControl->Items->Size; ++i)
		{
			Windows::UI::Xaml::DependencyObject^ item =
				dynamic_cast<Windows::UI::Xaml::DependencyObject^>(itemsControl->Items->GetAt(i));
			if (item != nullptr)
				localizeTree(item);
		}
	}
}

inline void install(Windows::UI::Xaml::FrameworkElement^ root)
{
	if (root == nullptr)
		return;
	localizeTree(root);
	Platform::WeakReference weakRoot(root);
	root->LayoutUpdated += ref new Windows::Foundation::EventHandler<Platform::Object^>(
		[weakRoot](Platform::Object^, Platform::Object^)
		{
			Windows::UI::Xaml::FrameworkElement^ target =
				weakRoot.Resolve<Windows::UI::Xaml::FrameworkElement>();
			if (target != nullptr)
				localizeTree(target);
		});
}

inline void refresh(Windows::UI::Xaml::FrameworkElement^ root)
{
	if (root != nullptr)
		localizeTree(root);
}
}

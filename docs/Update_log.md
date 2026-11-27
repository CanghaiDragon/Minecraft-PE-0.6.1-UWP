# Update Log/更新日志

This changelog covers the UWP project. It is separate from the Win32 changelog.

## 1.2.1

Bug fix:

- Water behavior is normal near world edge in finite world.

- Solving yellow wool, brown wool, lapis ore's icon error.

- Deciding whether a Indev/Alpha world is a hidden world no longer depends on seed.

Improvements:

- Support Chinese typing and gray with "/" on chatting.

- Blocks and items from PE 0.7 and 0.8 are compatible. Their crafting recipies can also be shown in server. Meanwhile, their full functions are not planed to be added.

- Ores' generation chance adjustment for Sky and Indev world. Now find enough diamonds in Indev-Floating and Sky worlds are easier.

- A few text adjustments.

## 1.2

New features:

- Achievements: a new additional feature, with 15 specific achievements, avaliable in survival mode.

- Multiple Language (Only Chinese at the moment)

- Adding "Indev" world type, with 4 new subtypes: Island, Floating, Flat and Inland.
  
  - Hidden world: Infdev 20100227.

- Screenshots (Part of Debug Function, with key F2).

- Chatting (Part of Debug Function, with key T. Only English is supported).

- Extended Multiplayer Mode (with optimized skin and more world types avaliable).

- Server avaliable. Now the game is able to join server in MCPE 0.1-0.8.

Bug fix:

- Spectator mode with touch input can fly up and down.

- Fixed cobblestone's rendering issue in Indev and Alpha world at chest page.

- Y=0 no longer have stones in Alpha world.

Improvements:

- Lakes can spawn in infinite, sky and alpha worlds.

- Sky, Indev and Alpha worlds (don't naturally spawn spruce and birch) now generates a chest with those trees' saplings.

- Less caves will spawn to improve performance on old devices.

- Bedrock is added to creative player's inventory.

- Items on hand now shows their name in Keyboard input.

## 1.1.3

New features:

- Allow v1.0 saves to be converted to new version's formate and opened.

- Old (Finite) world can be converted to infinite world.

- All world types can now be converted and exported in a 0.9+ PE supported format.

Bug fix:

- Pressing Esc when sleeping can correctly leave bed instead of entering pause interface.

Improvements:

- Adding icon and button for spectator mode (keyboard F). It is avaliable after opening Options → Additional → "Debug Function".

## 1.1.2

New features:

* Adding "Alpha" world type, with two new biomes: alpha and winter.

* Spectator mode (toggle F to enter in creative mode) can now X-ray inside blocks.

* All world types excluding finite (from origin PE) now have cave generation enabled.

Bug fix:

* Lightning under trees can now be saved and rendered correctly.

Improvements:

* Supporting low resolution on creating world page.

* Ice become translucent under Fancy + Java Beta Visual.

* Flowing water, Flowing lava, Ice and Snow (Slice) are added to creative player's inventory.

## 1.1.1

Bug fix:

* Placing a sign and write in infinite/sky world no longer causing game crash when re-enter the world.
* Snow and trees in the edge of rendering chunk can now be saved.
* Fixing the severe lightning issue under cliffs and trees in infinite/sky world. Most of those places are fine at the first place. The remainings will be fine after getting closer.
* Sky world will generate more gold ores, which is easier for building nether reactor. It only takes effect in new generated chunks.

Improvements:

* Improve the performances for low performance devices like Lumia phones.

## 1.1

The first UWP version. Features below are compared with v1.0 of Windows Universal (win32)

New features:

* Switch between original and Java UI, regardless of input mode;
* Player can jump while holding forward D-Pad after opening Touch Sneak;
* Restore the ability to swim when using original D-Pad;
* New save format for Infinite and Sky world. Significantly reduced save file sizes;
* Various optimize regard options and input;
* Adding World Edit, Import and Key Mapping XAML page;
* Redesigning skin optimization page;
* Adding Live Tiles and saves shortcut;
* Adding fullscreen support.

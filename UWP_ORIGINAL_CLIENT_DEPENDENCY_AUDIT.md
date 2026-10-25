# Original Client Dependency Audit

Date: 2026-10-09  
Scope: evidence-only preparation for the first compilable original-client
batch.  No game code is enabled by this audit.

## Observed desktop startup chain

The Win32 entry point is `handheld/src/main.cpp`, which selects
`NinecraftApp` as `MAIN_CLASS`.  In `handheld/src/main_win32.h`, the actual
startup order is:

```
AppPlatform_win32
  -> EGL/WGL context made current
  -> new NinecraftApp
  -> App::init(AppContext)
       -> NinecraftApp::init()
            -> static world registries
            -> initGLStates + Tesselator::instance.init
            -> I18n::loadLanguage
            -> Minecraft::init()
            -> ExternalFileLevelStorageSource
            -> start-menu screen
  -> NinecraftApp::setSize
  -> Win32 message loop
       -> NinecraftApp::update
            -> Multitouch::commit
            -> Minecraft::update
            -> App::swapBuffers
            -> Mouse::reset2
```

The UWP port now has the first two inputs to this chain: one UWP
`AppPlatform_uwp` plus one ANGLE-backed `AppContext`.  It has also already
executed the original `I18n::loadLanguage` call.  The UWP composition callback
remains the presentation owner; `AppContext::doRender` is intentionally false
until original presentation scheduling is migrated.

## Actual current source boundary

`MCPEUWP.vcxproj` compiles the original `world/**`, `nbt/**`, `util/**`,
`locale/I18n.cpp`, selected RakNet files, and `CreatorLevel.cpp`.  It now also
compiles the non-server original client closure: `src/client/Minecraft.cpp`,
`src/NinecraftApp.cpp`, `LevelRenderer.cpp`, `GameRenderer.cpp`, GUI/screens,
entity/model/player/input sources, renderer support, and `PerfRenderer.cpp`.
The desktop `main.cpp` entry point and `client/sound/*.cpp` remain excluded.
The per-item UWP client definitions omit `STANDALONE_SERVER` but retain
`NO_NETWORK`, `MCPE_UWP`, and `MCPE_UWP_CLIENT_LIFECYCLE_ONLY`; this makes the
object layout match the original client while keeping its init/update paths
dormant.

The current `UwpCoreSession` already uses original `CreatorLevel`, `Player`,
`Level::tick`, collision, generator code, and the static `Material`,
`MobCategory`, `Tile`, `Item`, `Biome`, and `TileEntity` registration calls.
It is therefore not a replacement candidate for those systems.  Its UWP-only
world ownership, movement adapter, and renderer bridge are transitional once
the original `Minecraft` ownership path is active.

## Why `Minecraft.cpp` is not an isolated next file

`Minecraft::init()` in `handheld/src/client/Minecraft.cpp` immediately creates
and/or invokes:

1. `Options` and `OptionsFile`, then `reloadOptions()` which reads/writes the
   options path.
2. `Textures`, including synchronous `AppPlatform::loadTexture` calls for
   terrain, font, particles, grass-color and foliage-color assets.
3. `Gui`, `Font`, `ParticleEngine`, `LevelRenderer`, `GameRenderer` and
   `PerfRenderer`.
4. `User`, game-mode/input holders, screens, level storage and potentially
   network callbacks during later `Minecraft::update()` paths.

`NinecraftApp::init()` adds the GL state setup and `Tesselator` before that
call; its `update()` additionally commits multitouch, runs `Minecraft::update`,
and resets mouse state.  Thus compiling only `Minecraft.cpp` or only
`NinecraftApp.cpp` cannot form a linkable or useful checkpoint.

## Required source groups and ordering

| Batch | Original code to add/reuse | Must already work | Completion boundary |
|---|---|---|---|
| A. Client foundation | `Options*.cpp`, `MouseHandler.cpp`, input holders, `Gui.cpp`, `Font.cpp`, base `Screen.cpp`, `Textures.cpp`, `Tesselator.cpp`, `gles.cpp` | `AppPlatform`, resource paths, ANGLE context | Compile/link a non-running original-client support library; do not construct `Minecraft`. |
| B. GLES2 compatibility | Original renderer sources plus a UWP GLES2 implementation of the compatibility functions declared in `client/renderer/gles.h` | Batch A, active ANGLE context | Draw a known original `Tesselator` batch without the custom UWP world renderer. |
| C. Original client ownership | `Minecraft.cpp`, `NinecraftApp.cpp`, game modes, local-player/input, particles, renderers, required screens | B, options/storage adapter, full texture cache | Construct and initialize the original client while UWP still owns present. |
| D. Runtime hand-off | UWP scheduler forwards platform input and calls original `NinecraftApp::update()`; UWP world slice becomes fallback-only | C | Original title screen and original world path update under UWP. |
| E. Remaining services | persistent level storage, keyboard/text dialog, audio, network, screenshots, lifecycle/reset | D | Replace the remaining platform defaults with UWP implementations. |

Each batch must retain the present UWP vertical slice as a selectable fallback
until the new boundary compiles and runs.

## Platform contracts that still block Batch C

- **Texture loading:** `Textures::loadTexture` is synchronous.  The current
  UWP cache validates only `terrain.png`; Batch A must preload the full
  original image set into the cache before `Minecraft::init()`, or provide a
  safe non-STA decoding path.  Blocking a WinRT task on the XAML STA is known
  to fail and must not be reintroduced.
- **GLES2:** `client/renderer/gles.h` uses legacy fixed-function concepts
  (`glMatrixMode`, client arrays, alpha test, fog and `GL_QUADS`).  ANGLE
  supplies GLES2, not the Win32 GLEW/WGL compatibility API.  The existing
  iOS/macOS/Linux shader paths do not select a UWP GLES2 implementation, so a
  dedicated UWP compatibility backend is required before original renderer
  sources can render correctly.
- **Options/storage:** desktop `main_win32.h` assigns `"."` to
  `externalStoragePath`; UWP must map original options and level-storage paths
  to its LocalFolder-backed storage without changing `Minecraft` world logic.
- **Input/lifecycle:** original update consumes global Keyboard/Mouse/
  Multitouch state and performs its own buffer swap.  UWP must feed those
  states from XAML events and retain `doRender = false` until a single
  presentation owner is chosen.
- **Services:** `AppPlatform` defaults are sufficient for the current locale
  test but not original screens: user text dialogs/keyboard, screenshots,
  network policy, sound, and suspend/resume must be supplied incrementally.

## Recommended next focused implementation

Batch A's first resource task is now implemented: the UWP shell asynchronously
preloads `terrain.png`, `misc/grasscolor.png`, `misc/foliagecolor.png`,
`particles.png`, and `font/default8.png` into the existing
`AppPlatform_uwp` RGBA cache.  `terrain.png` still feeds the fallback world
renderer from that same decode.  The result is independently visible through
the compact `Client texture cache: n/5 ready` diagnostic.

The first Batch A source inventory has now been applied to the UWP project.  A
static expansion check resolves 148 source files with no duplicate or missing
items.  The next task is no longer source discovery: it is a user-side x64
UWP compile/smoke run, followed by an explicit world/player ownership bridge
before enabling any original `Minecraft::init()` or `NinecraftApp::update()`
call.

## Macro/build boundary used by the UWP closure

| Definition | UWP core/storage sources | Original client closure | Effect |
|---|---|---|---|
| `STANDALONE_SERVER` | enabled | omitted | Keeps the existing `CreatorLevel`/storage vertical slice server-shaped, while giving `Minecraft`, `Gui`, `LevelRenderer`, and `GameRenderer` the real non-server member layout. |
| `NO_NETWORK` | enabled | enabled | Selects the existing `IRakNetInstance` seam and prevents network startup; it does not remove declarations needed by the client sources. |
| `MCPE_UWP` | enabled | enabled | Selects UWP platform/resource hooks and the ANGLE-compatible renderer seam. |
| `MCPE_UWP_CLIENT_LIFECYCLE_ONLY` | not required | enabled | Makes the original client translation units compile/link while returning before `NinecraftApp::init/update/teardown` and `Minecraft::init/update/tick`; it is the guard against two simultaneous owners. |

The split is per `ClCompile` item in the project rather than a global
replacement of `STANDALONE_SERVER`. Consequently the current UWP world
continues to use its proven server-side storage/tick objects, and the Win32
projects are untouched. The client closure intentionally omits the audio
translation units; `Minecraft.cpp` also gates `SoundEngine` construction and
destruction under the lifecycle macro. This is compile/link preparation, not
audio or network runtime integration.

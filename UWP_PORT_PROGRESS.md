# UWP Port Progress

## Current stage

Stage 3 — original locale bootstrap through the UWP AppPlatform.

## Completed in this round

- Retained the Stage 0 packaged byte-read seam in
  `AppPlatform_uwp::readAssetFile`.  Original callers pass paths relative to
  `data/`, matching `AppPlatform_win32` and `I18n::fillTranslations`.
- Implemented `AppPlatform_uwp::loadTexture` as a synchronous RGBA cache
  lookup that returns a newly allocated `TextureData`, preserving the original
  `Textures` ownership contract and `data/images/` path convention.  The UWP
  shell performs packaged PNG decoding asynchronously before populating it.
- Added the platform services needed by this early original-client path:
  current UWP display DPI, screen-size updates, touchscreen capability, a
  UWP build-model identifier, date text, URL activation, and soft-keyboard
  requests.
- Bound one `AppPlatform_uwp` instance to `MainPage`'s lifecycle and updates
  its dimensions from the ANGLE surface.  It is handed to the Stage 2
  `AppContext` host, but deliberately not yet to a constructed
  `NinecraftApp` or `Minecraft` instance.
- Added a Stage 1 runtime smoke test in `MainPage`.  On startup it reads the
  original `lang/en_US.lang` through `readAssetFile`, decodes the original
  `images/terrain.png` through `loadTexture`, releases both original-contract
  allocations, and displays `AppPlatform smoke: lang=... , terrain=...` in
  the existing startup/status diagnostics.
- Added `en_US.lang` to the UWP package as a linked deployment item.  This
  keeps the original asset in place and gives the packaged `data/lang/` path
  required by `readAssetFile`.
- The smoke test now confirms `lang=OK` and `terrain=OK`: `loadTexture` reads
  only from an RGBA cache filled by the existing asynchronous terrain decoder.
  This preserves the original synchronous `TextureData` contract without
  blocking the XAML STA.
- The GLES status area now supports a separate, smaller second line for
  diagnostics.  Routine state and the platform-smoke result are no longer
  concatenated into one line.
- Added `UwpOriginalClientHost`, which binds the original `AppContext` shape
  to the active ANGLE display/context/surface and `AppPlatform_uwp` instance.
  The UWP page remains presentation owner (`doRender = false`), preventing a
  future original `App::swapBuffers()` call from double-presenting.  The host
  is cleared/rebound around native-XAML navigation.
- Verified the Stage 2 binding in the UWP package: it compiles and its normal
  startup diagnostics are present at runtime.
- Added a narrow Stage 3 bootstrap that calls the original
  `I18n::loadLanguage(platform, "en_US")` after `AppContext` is bound.  It
  validates the actual original translation map via `menu.singleplayer` and
  surfaces failure before the UWP renderer starts.  This is the original
  `NinecraftApp::init()` locale call, used without entering `Minecraft::init()`.
- Completed an evidence-only original-client dependency audit in
  `UWP_ORIGINAL_CLIENT_DEPENDENCY_AUDIT.md`.  It verifies from the project
  items and call sites that the UWP target contains original world code but
  no original client source set yet, and records the required source groups,
  UWP platform contracts, and the first safe implementation boundary.

## Explicitly not included

- No `Minecraft`, `NinecraftApp`, GUI, renderer, world-generation, entity or
  game-rule changes.
- No persistent-storage, options, audio, networking, or gameplay-input
  bridge.  Their inherited `AppPlatform` defaults remain intentionally unused
  in this stage.
- The existing UWP menu and vertical-slice renderer remain unchanged; the new
  texture bridge is not called by them yet.
- No `App`, `NinecraftApp`, or `Minecraft` instance is constructed or
  initialized in Stage 3.

## Validation

- Source remains included by `MCPEUWP.vcxproj`.
- User-side Visual Studio x64 UWP compilation succeeded after the Stage 2
  XAML namespace/include separation repair, and the expected diagnostics were
  observed at runtime.
- Stage 3 has not been compiled in this session, by user request.  Its
  expected first-line diagnostic is `EGL ready | AppContext + I18n bound`.

## Current focused task

Batch A startup-image preload is implemented and user-side validation reported
`Client texture cache: 5/5 ready`.  It
asynchronously decodes `terrain.png`, `misc/grasscolor.png`,
`misc/foliagecolor.png`, `particles.png`, and `font/default8.png` into the
existing `AppPlatform_uwp` RGBA cache.  `terrain.png` continues to feed the
existing UWP fallback world renderer from the same decoded pixels.  The status
detail reports `Client texture cache: 5/5 ready` or the first failed asset.

No original client source was added and no `Minecraft` instance is created.

## Completed storage smoke task

Added a deliberately isolated original `level.dat` round-trip test.  It adds
the unmodified `ExternalFileLevelStorage.cpp` serializer to the UWP target,
writes a fixed `LevelData` into LocalCache's `__uwp_leveldat_smoke` directory,
and reads it back with the original `readLevelData` implementation.  It
compares seed, name, spawn and time, then reports `level.dat OK (write + read)`
or a specific failure.  User-side validation reported that expected result.

## Current focused task

- Replaced the vertical slice's `MemoryLevelStorageSource` with the original
  `ExternalFileLevelStorage` implementation.  The active world now uses
  `LocalFolder/games/com.mojang/minecraftWorlds/uwp-experimental-world`.
- The original storage constructor reads an existing `level.dat`, and
  `CreatorLevel` receives it through the original `LevelStorage::prepareLevel()`
  call.  If no save is present, the original `LevelData` creation path remains
  in use.
- Returning from the active world now calls original `Level::saveGame()` before
  releasing the original level and its storage object.  No UWP-only save
  serializer was added.
- The fixed `uwp-experimental-world` identifier is deliberate and temporary:
  it provides a stable reopen target before world enumeration/selection is
  connected.
- Selecting that exact folder in the existing EGL world list now invokes
  `UwpCoreSession::startSavedWorld`, which constructs `CreatorLevel` with the
  same original external storage path.  This is the first end-to-end reopen
  test; the UI retains `New World` as its display name instead of overwriting
  saved metadata with the temporary folder identifier.
- Player persistence now uses the original `Level::loadPlayer(m_player, true)`
  path.  It loads the `Player` NBT stored in `level.dat` and registers the
  player in `Level::players`, which causes original `Level::saveGame()` to
  serialize position, rotation, health, and inventory data.  Ownership now
  follows the original `Level` entity collection, avoiding a double delete
  when the session closes a world.
- `ExternalFileLevelStorageSource` is now compiled into the UWP target and is
  the owner of active-world `selectLevel()` calls.  On a background worker,
  the UWP adapter enumerates LocalFolder child directories; the original
  source then reads every candidate's `level.dat` and produces `LevelSummary`
  entries.  The EGL world list uses the resulting original ids and display
  names to open the selected world.

## Explicitly deferred

- The original Win32 `SHFileOperation` deletion path remains deliberately
  unavailable under UWP.  No delete UI is connected.  Renaming also remains
  deferred until an asynchronous UWP folder move can preserve the original
  source's collision and metadata rules.
- No new-world naming UI, options, networking, audio, or renderer work was
  included.

## Next focused task

Compile and launch the UWP package.  In Singleplayer, verify that the list
shows the level name stored in each valid original `level.dat`, not merely its
folder id.  Open a listed save, move to a distinct position, Back to save,
then reopen it and confirm its position and world time are restored.

If that passes, the next focused task is either UWP-safe delete/rename support
for the original source contract, or a distinct new-world naming/id flow.  It
must be selected explicitly; they have different platform and UX risks.

## Completed input-queue smoke seam

- Added original `platform/input/Keyboard.cpp` to the UWP target with the
  existing native-core compilation settings.
- UWP `CoreWindow::KeyDown` and `KeyUp` now translate the first desktop
  key set (letters, F1--F12, Space, Escape, Enter, Backspace, Shift, plus
  arrow-key aliases for WASD) and call original `Keyboard::feed()`.
- The temporary movement bridge now reads original `Keyboard::isKeyDown()`
  for WASD and Space.  Existing gameplay movement is therefore sourced from
  the original keyboard state table, without constructing `Minecraft` or
  replacing original `KeyboardInput`.
- Because no `Minecraft::tickInput()` consumer exists yet, each UWP fixed
  tick clears the unconsumed original event list with `Keyboard::reset()`.
  The original key-state table is intentionally retained by that method.  The
  reset is temporary and must be removed once the original client loop owns
  its normal input lifecycle.

## Next focused task

User-side x64 UWP compile and runtime verification: enter a world and verify
WASD, arrow-key aliases, and Space still move/jump.  No mouse queue, GUI
consumer, or original `Minecraft::tickInput()` has been connected in this
smoke seam.

## Completed UI-thread stall diagnostics

- Added diagnostics only; no world-generation, player, rendering, or input
  behavior was changed.
- The active world's maximum CPU times now measure original `Level::tick()`
  and `Player::tick()`, while the UWP renderer measures terrain mesh rebuild
  and render-pass CPU time.  The UWP page also records the maximum complete
  rendering callback time, including the fixed-tick advance and presentation.
- During an active world, the existing lower status-detail line updates at
  most once per second as: `max ms L:<level> P:<player> M:<mesh> R:<render>
  F:<frame>`.  It stays one line and does not alter the simulation schedule.
- Fixed the UWP/Windows `max` macro collision in the diagnostic code by using
  explicit comparisons instead of `std::max`; the user-reported syntax-error
  cascade was limited to that new instrumentation.

## Next focused task

User-side x64 UWP compile and stall capture: play until one of the 1--2 second
freezes occurs, then provide a screenshot of the two status lines immediately
after recovery.  Use the largest field among L/P/M/R/F to choose exactly one
repair task; do not move level objects to a background thread before the
blocking phase is evidenced.

## Completed UWP storage-tick stall repair

- Added a borrowed loaded-chunk enumeration method to ChunkCache.  It
  exposes resident chunks without changing ownership or generation behavior.
- Under MCPE_UWP only, ExternalFileLevelStorage::tick() now uses that
  enumeration for its 50-tick dirty-chunk scan.  It no longer calls
  Level::getChunk() for every coordinate, so storage maintenance cannot
  synchronously generate a 16x16 cache of missing chunks on the UI thread.
- The original dirty queue and savePendingUnsavedChunks(2) write budget are
  unchanged.  The Win32 storage-tick path is unchanged.

## Validation result

- The user compiled and ran the UWP build after the storage-tick repair.
- The world advanced for approximately 1000 game ticks (about 50 seconds)
  without the previous 1--2 second freezes.  Before this repair, freezes could
  occur after roughly 100 ticks.
- This confirms that the UWP storage maintenance scan that called
  Level::getChunk() for missing coordinates was the primary UI-thread stall
  observed in the vertical slice.

## Next focused task

Remove or narrow the temporary performance overlay after a final regression
run, then resume the explicitly staged original-client integration plan.  The
periodic entity-save path remains a separate concern and should only be
changed if a later measurement shows it blocking.

## Completed Stage 5 subtask: UWP GLES2 fixed-function seam

- Added an MCPE_UWP branch to the original gles.h contract.  It selects the
  packaged ANGLE GLES2 headers, reuses the existing CPU MatrixStack, and
  exposes the legacy state constants required by the original renderer.
- Extended the existing shader compatibility implementation in gles.cpp to
  the UWP GLES2 path: matrix upload, texture/alpha-test/fog state tracking,
  vertex-attribute array setup, and VBO draw helpers now share the proven
  iOS GLES2 structure.
- Added MatrixStack.cpp and gles.cpp to the UWP project with a graphics
  configuration that does not use STANDALONE_SERVER.  No original
  Textures, Font, or Tesselator source has been added yet, and the active
  UWP renderer is unchanged.

This subtask has not been compiled in this session, per the user instruction.

## Next focused task

User-side x64 UWP compile validation of the isolated GLES2 compatibility seam.
If it compiles, the next subtask is to add the original Textures source only
and exercise one packaged texture through its original load/bind contract.

## Targeted repair after first Stage 5 compile

- The first user-side compile reported a shader-symbol cascade in gles.cpp:
  glCreateShader, glShaderSource, glCompileShader, and related GLES2 entry
  points were undeclared.
- The bundled ANGLE gl2.h places those declarations behind
  GL_GLEXT_PROTOTYPES, not GL_GLES_PROTOTYPES.  The UWP branch now defines the
  correct macro before including the ANGLE headers.
- No other Stage 5 scope was changed.  This is the first targeted repair
  attempt for this compile.

## Completed Stage 5 subtask: original Textures smoke path

- Added the original Textures.cpp and its DynamicTexture dependency to the
  UWP graphics configuration.  The server-only world compilation remains
  unchanged.
- MainPage now creates the original Options and Textures objects only after
  beginFrame() has made the ANGLE context current.  It calls the original
  loadAndBindTexture("terrain.png") path against AppPlatform_uwp and checks
  the original TextureData returned by getTemporaryTextureData().
- The smoke object is destroyed immediately after the contract check, so the
  current UWP menu/world renderer does not acquire ownership of this texture.
  No Font or Tesselator source was added.

This subtask has not been compiled in this session, by user instruction.

## Next focused task

User-side x64 UWP compile and runtime check for the original Textures smoke
message.  If it reports terrain OK, the next subtask is to make one current
UWP draw call use the original Textures-created GL id without changing the
world mesh path.

## Targeted repair: make the UWP graphics contract visible to XAML translation units

- The first Textures smoke compile reported desktop GLEW symbols colliding with
  ANGLE GLES2 types (`GLintptr`, `GLsizeiptr`, and shader prototypes).  The
  conflict came from the project-level UWP XAML sources not defining
  `MCPE_UWP`; `gles.h` therefore selected its desktop GLEW fallback when
  `MainPage.xaml.cpp` included the original `Textures` contract.
- Added `MCPE_UWP` to the shared UWP `ClCompile` preprocessor definitions.
  This keeps all UWP translation units on the existing ANGLE GLES2 branch and
  prevents the desktop GLEW branch from being mixed into the same compilation
  unit.  No renderer, texture-loading, or gameplay behavior was changed.
- The user should perform the next x64 UWP compile.  Per request, no build was
  run here.  If the compiler still reports a header conflict, this is repair
  attempt 2 and the next action must remain limited to the exact remaining
  include/definition conflict.

## Targeted linker repair: original OptionsFile dependency

- The subsequent compile resolved the ANGLE/GLEW header conflict.  The only
  remaining linker failure was `OptionsFile::OptionsFile()` referenced by the
  original `Options` object used for the Textures smoke path.
- Added the original `client/OptionsFile.cpp` to the UWP target with the same
  native UWP compile definitions.  This supplies only the constructor needed
  by `Options`; it does not enable the options UI, full options loading, or any
  new persistence behavior.
- The user should perform the next x64 UWP compile.  No build was run here.

## Completed Stage 5 subtask: first original texture draw binding

- `UwpWorldRenderer` now creates and retains the original `Options` plus
  `Textures` objects after the ANGLE context is current, and loads
  `terrain.png` through `Textures::loadAndBindTexture()`.
- The existing world mesh and camera path are unchanged.  When the original
  texture is available, the current terrain draw binds its original GL texture
  ID; otherwise it keeps using the existing UWP-uploaded RGBA fallback.
- The original `Textures` owner remains alive for the renderer lifetime, so its
  GL name is not deleted at the end of the smoke callback.  `shutdown()` drops
  that owner before releasing the fallback texture.
- No original `Font`, `Tesselator`, `LevelRenderer`, `Minecraft`, or client
  loop code was added.  This is only a texture ownership/bind seam.

## Completed Stage 5 subtask: original terrain atlas coordinates

- The current mesh already obtains each face's tile index from the original
  `Tile::getTexture(LevelSource*, x, y, z, face)` implementation.  Its UV
  conversion now uses the dimensions of the retained original `Textures`
  image when that texture is bound, rather than assuming the fallback atlas
  dimensions.
- The original renderer's small `15.99f` edge inset is represented by a
  per-atlas texel inset for the original path, reducing neighbouring-tile
  bleed without changing face visibility, mesh generation, collision, or
  player movement.
- Atlas-backed color/tint selection now also recognizes the original texture
  when the fallback UWP upload is absent.

## Next focused task

User-side x64 UWP compile and runtime check.  Verify that terrain still shows
the same block textures and that grass/dirt colors remain stable.  If it
passes, the next candidate is the original `Font` texture/render contract;
the original Tesselator and LevelRenderer remain separate, larger seams.

## Current focused task: original Font contract

- Added the original `client/gui/Font.cpp` to the UWP graphics target.
- Added its required original `Tesselator.cpp` and `RenderChunk.cpp` link
  dependencies.  No original GUI screen or client loop is constructed.
- MainPage now performs a one-time `Font` smoke check after the five startup
  textures are ready.  It constructs the original `Font` with the original
  `Textures` contract, loads `font/default8.png`, and validates the original
  glyph-width calculation for `Singleplayer`.
- The smoke path is CPU-only under the existing UWP `USE_VBO` configuration;
  it does not call the original GUI draw path yet.

## Next focused task

User-side x64 UWP compile and runtime check.  Expected diagnostics are
`Original Font: OK (width=...)` and the unchanged world rendering.  If this
passes, the next task is to expose the original Tesselator as an isolated
CPU/GLES2 draw seam before attempting `LevelRenderer`.

## Next focused task

User-side x64 UWP compile and runtime check.  Enter a generated world and
verify that the existing terrain mesh still renders while the status reports
`Original Textures: terrain OK`.  If that passes, the next subtask is to route
one original texture-coordinate/atlas lookup into the current mesh without
changing chunk generation or collision.

## Targeted compile repair: complete original texture owner types

- The user-side compile reached the XAML-generated translation unit but failed
  with `can't delete an incomplete type` for `Textures` in `memory`.
- `UwpWorldRenderer` stores the original `Options` and `Textures` owners in
  `std::unique_ptr`.  C++/CX generated code instantiates the owning destructor
  from the public header, so forward declarations are insufficient there.
- The renderer header now includes the original complete `Options` and
  `Textures` definitions.  This changes only type visibility; ownership and
  rendering behavior are unchanged.
- No build was run here.  Please retry the x64 UWP compile.

## Current round: original Font smoke seam pending validation

- The original Font/Tesselator source additions described above are now the
  active focused change for the next user-side compile.
- Do not treat this as full font rendering yet: the visible UWP UI still uses
  `UwpUiRenderer`, and the original `Font::draw()` path has not been routed to
  the screen.

## Current focused task: original Tesselator CPU seam

- MainPage now runs a one-time original Tesselator check after the Font check.
- It emits one original `GL_QUADS` primitive through `vertexUV()` and calls
  the original Windows CPU extraction method `endToCPU()`.
- The result is accepted only when the original quad expansion produces six
  triangle vertices and the byte count matches the original `VERTEX` layout.
- This test does not call `Tesselator::init()`, bind a VBO, issue a draw, or
  modify the active UWP world renderer.  GLES2 submission remains a separate
  follow-up once the CPU contract is confirmed.

## Next focused task

User-side x64 UWP compile and runtime check.  Expected diagnostic:
`Original Tesselator: OK (vertices=6)`.

## Current focused task: route original Tesselator VBO submission through UWP GLES2

- Updated the original `Tesselator::draw()` GLES2 selection in
  `handheld/src/client/renderer/Tesselator.cpp` so `MCPE_UWP` uses the same
  shader-aware vertex-attribute path already used by the original iOS GLES2
  branch.
- The UWP path now supplies the original VERTEX layout to ANGLE as position
  (3 floats at byte 0), texture coordinates (2 floats at byte 12), and packed
  vertex color (4 normalized bytes at byte 20).  It also preserves the
  original `GL_QUADS` to triangle submission behavior and synchronizes the
  existing compatibility-shader uniforms.
- The desktop client-state path, current UWP world mesh, collision, input,
  storage, and GUI were not changed.  The existing CPU-only Tesselator smoke
  remains the non-invasive validation path; this change only makes the
  original VBO draw seam correct when it is invoked.
- No build was run, per the user's request.  The next action is a user-side
  x64 UWP compile and runtime check.  A successful compile should leave the
  current world visually unchanged; any future original Tesselator draw call
  should now have valid GLES2 attribute bindings instead of relying on the
  no-op fixed-function client-state macros.

## Next focused task

User-side x64 UWP compile and runtime check for the Tesselator GLES2
submission seam.  If it passes without a visual regression, the next seam can
be a deliberately isolated original draw call or the next renderer contract;
do not route the full `LevelRenderer` or `Minecraft` loop in the same round.

## Current focused task: isolated original Tesselator GLES2 draw probe

- `UwpWorldRenderer::initialize()` now initializes the existing original GLES2
  compatibility state (`glInit()`) and the original Tesselator VBO pool after
  the UWP EGL context is current.
- After the unchanged UWP terrain draw, `UwpWorldRenderer` submits one small
  magenta quad through `Tesselator::begin()` / `vertex()` / `draw()`.  The
  probe uses the original MatrixStack with a pixel-space orthographic matrix,
  so it exercises the real original VBO upload, GLES2 attribute pointers,
  shader uniform synchronization, and original quad-to-triangle expansion.
- The probe is deliberately isolated from terrain, player movement, collision,
  storage, and GUI logic.  It temporarily switches to the original
  compatibility shader, restores the depth state, and then leaves the UI
  renderer to draw normally.  The marker is diagnostic and is intended to be
  removed or replaced once this seam is validated.
- `UwpWorldRenderer.cpp` is explicitly compiled with the native UWP GLES2
  definitions rather than the dedicated-server/no-OpenGL definitions, because
  this file now calls the original renderer compatibility entry points.
- No build was run, per the user's request.  The next action is a user-side
  x64 UWP compile and runtime check.  Expected result: the existing terrain
  remains intact and a small magenta square appears near the upper-right of
  the game viewport.  If it does not appear, report whether the compile fails
  or whether only the marker is missing; do not yet route `LevelRenderer`.

## Targeted rollback: isolate the failed Tesselator draw probe

- The user-side run after the probe produced only the EGL clear color and the
  level.dat diagnostic; the game HUD and world were no longer usable.  This is
  a regression, not an expected result.
- Removed the runtime `glInit()`/VBO initialization and the visible original
  Tesselator marker from `UwpWorldRenderer`.  The UWP terrain renderer is back
  to its last known-good shader/state sequence, while the previously validated
  `Tesselator::draw()` GLES2 branch remains in the original source for later
  integration.
- Restored the project item to its previous definition.  No world, player,
  collision, input, storage, or UI code was changed by the rollback.
- No build was run.  The next user-side compile should confirm that the
  known-good terrain/HUD path is restored.  Only after that should the original
  GLES initialization be introduced as a separate, non-drawing probe with
  explicit shader/state diagnostics.

## Current focused task: original GLES2 initialization-only diagnostic

- Added a one-time `MainPage::RunOriginalGlesSmoke()` call after the existing
  Textures, Font, and CPU Tesselator smoke checks.
- The diagnostic calls the original `glInit()`, verifies that the original
  compatibility shader program was created and marked active, then immediately
  calls `shaderUnbind()`.  It does not call Tesselator drawing, MatrixStack
  operations, VBO allocation, or any original world renderer code.
- This keeps the known-good UWP terrain/UI render path as the only drawing path
  while separating shader initialization from the failed visible probe.
- Expected two-line diagnostic: `Original GLES2: OK (program=N)` or
  `Original GLES2: FAIL [shader init/link failed]`.  No build was run; the user
  should compile and test this isolated step before any further Tesselator
  integration.

## Current focused task: prevent original Tesselator VBO state leaking into UWP client arrays

- Root cause identified from the failed visible probe: original
  `Tesselator::draw()` binds a VBO, while the existing UWP terrain and UI
  renderers submit client-memory vertex pointers.  In GLES2, those pointers
  become offsets whenever `GL_ARRAY_BUFFER` remains bound, so the following
  terrain/UI draws fail and only the clear color remains visible.
- UWP `Tesselator::draw()` now unbinds `GL_ARRAY_BUFFER` after its GLES2 draw.
  The isolated probe also explicitly unbinds it before returning and keeps the
  `gles.h` macro redirects local to the probe code at the end of the source
  file, leaving the existing terrain renderer's direct GLES2 calls unchanged.
- The probe still does not replace terrain or invoke `LevelRenderer`; it only
  exercises one original colored quad after the existing terrain pass.
- No build was run.  The next user-side x64 UWP compile/runtime check should
  show the normal terrain and HUD plus the small magenta square.  If the HUD
  remains visible but the square is absent, the VBO leak is fixed and the next
  issue is limited to the original shader/matrix draw state.

## Targeted repair: explicitly enable GLES2 vertex attributes for original draws

- The UWP terrain renderer disables attributes 0/1/2 after its client-memory
  draw.  The original GLES2 Tesselator branch previously set pointers but
  assumed those arrays remained enabled from `glInit()`, so its VBO draw could
  complete without producing any pixels.
- `Tesselator::draw()` now explicitly enables the position attribute and each
  texture/color attribute it uses before `glDrawArrays()`.  The existing
  `drawArrayVT()` and `drawArrayVTC()` GLES2 helpers receive the same fix for
  the later original renderer seams.
- The VBO unbind repair remains in place, so the original draw cannot leak
  buffer state into the current UWP client-memory terrain/UI passes.
- No build was run.  Please compile and run manually; expected result is the
  normal world/HUD plus the small magenta probe square near the upper-right.

## Current focused task: isolated original Font GLES2 draw probe

- Kept the current UWP terrain mesh and `UwpUiRenderer` unchanged, while
  adding an original `Font` owner to `UwpWorldRenderer` after the original
  `Textures` owner has loaded the packaged `font/default8.png`.
- `MainPage::RunOriginalFontSmoke()` now validates that renderer-owned
  original font and reports its glyph width.  This is no longer a separate
  temporary `Textures`/`Font` object that is immediately destroyed.
- The world renderer submits a small `Original Font OK` marker through the
  original `Font::draw()` -> `Font::drawSlow()` -> `Tesselator::draw()` path,
  using the original GLES2 compatibility shader, orthographic MatrixStack,
  font atlas texture, alpha test, and VBO submission.  The probe restores the
  VBO, matrix, texture, blend, alpha, depth, and shader state before the
  existing UI pass.
- The standalone magenta Tesselator marker is disabled for this font-only
  validation round.  Its implementation remains available as a previously
  validated diagnostic, and the Font draw naturally still uses the original
  Tesselator as its required submission dependency.  No LevelRenderer,
  GameRenderer, or original UI takeover was added in this round.

## Validation and next action

- No compilation was run, per the user's instruction.  User-side x64 UWP
  compilation and runtime validation is required.

## Targeted repair: Windows `min/max` macro compatibility

- The first user-side compile of the block-batch seam reported syntax errors
  on the `std::min/std::max` expressions used to recover a face's block
  bounds.  The Windows headers expand those names as macros even in qualified
  syntax, so the lambda now uses the renderer's existing `minValue/maxValue`
  helpers instead.  No rendering behavior or selection logic changed.
- This was the first targeted repair for this compile attempt.  No build was
  run here; please repeat the same x64 UWP compile manually.
- Expected result in a game world: the existing terrain/HUD remains usable,
  the standalone magenta marker is absent, and a small pixel-font
  `Original Font OK` marker appears near the upper-middle/right of the viewport.
- If the marker is absent while the world remains normal, report that as a
  font draw/state issue.  If the world or HUD becomes blank, treat it as a
  state-leak regression and roll back this isolated probe before any further
  renderer work.

## User validation: original Font GLES2 probe accepted

- The user reports that the diagnostic information is displayed normally and
  the current UWP world remains usable after the original Font draw probe.
- This completes the Stage 5 compatibility-layer acceptance target:
  `Textures` loads the original atlas, `Font` reaches a real GLES2 draw through
  the original `Tesselator`, and the ANGLE fixed-function compatibility state
  is restored for the existing UWP UI pass.
- The visible menu/HUD is still intentionally owned by `UwpUiRenderer`; the
  original `Font` is not yet the global UI text owner.  That is a client-render
  integration task, not another Stage 5 platform-compatibility prerequisite.

## Next stage boundary

- Do not bulk-add all original rendering in one round.  The next focused task
  should be selected as a Stage 6 seam: either route one real HUD/status label
  through the original Font while retaining the custom UI, or begin one
  original Tesselator terrain face under the existing world camera.
- Full `LevelRenderer`/`GameRenderer` takeover remains deferred until one of
  those independently testable seams is stable.

## Current Stage 6 focused task: one real HUD status line through original Font

- `UwpUiRenderer` now borrows the original `Font` owned by
  `UwpWorldRenderer`.  It suppresses only the custom game status-detail line
  when that font is available; menu text, buttons, terrain, and the main HUD
  labels remain on the existing UWP renderer.
- After the normal UWP UI pass, the status-detail string is converted to the
  original ASCII-compatible MCPE text contract and drawn through
  `Font::draw()` -> `Font::drawSlow()` -> original `Tesselator::draw()` using
  the GLES2 compatibility shader.  Matrix, texture, alpha, blend, depth, and
  VBO state are restored before presentation.
- The former standalone `Original Font OK` world probe is no longer invoked;
  this round tests an actual game HUD label instead of an isolated marker.
- No `LevelRenderer`, `GameRenderer`, original menu screen, world-generation,
  entity, collision, storage, or input changes were made.

## Validation and next action

- No compilation was run, per the user's instruction.  User-side x64 UWP
  compilation and runtime validation is required.
- Expected result: the game remains usable, buttons and top-left HUD remain
  unchanged, while the bottom status-detail line uses the original pixel font.
- If this passes, the next Stage 6 seam should be selected separately: either
  another small original UI text surface or one real terrain face through the
  original Tesselator.  Do not connect the full `LevelRenderer` in the same
  round.

## Current Stage 6 focused task: one real block batch through original Tesselator

- `UwpWorldRenderer::render()` still draws the complete current UWP terrain
  mesh first, then collects all visible faces belonging to one nearby block
  from that real mesh (`appendQuad()`'s `[a,c,d,a,d,e]` expansion) and submits
  the resulting small batch through the original
  `Tesselator::begin()`/`vertexUV()`/`draw()` path.
- The selected block is recovered from actual `Level` face geometry, rather
  than a hard-coded coordinate.  Its original atlas UVs and existing
  face-colour values are reused.  The original terrain texture and shared
  GLES2 compatibility shader are used for this draw, with the existing
  projection/view matrices copied into `MatrixStack`.
- Depth writes, matrix stacks, texture/alpha/blend state, shader binding, and
  VBO binding are restored at the seam boundary.  The complete custom mesh is
  intentionally retained as the visual fallback, and no `LevelRenderer`,
  `GameRenderer`, world generation, entity, collision, storage, or input code
  was changed.
- No compilation was run, per the user's instruction.  User-side x64 UWP
  compilation and runtime validation is required.

## Validation expectation and next boundary

- Expected world behavior is unchanged: the generated terrain remains visible,
  movement and HUD remain usable, and the original Tesselator submits one
  real block batch without blanking the frame or leaking state into the UI
  pass.
- If the world becomes blank, first disable only this seam call and keep the
  existing terrain path; that isolates a shader/MatrixStack/Tesselator state
  issue without rolling back the earlier Font seam.
- If this passes, the next Stage 6 seam should be selected separately.  The
  next likely target is one original block render batch or one additional
  original HUD surface; full `LevelRenderer`/`GameRenderer` takeover remains
  deferred until these boundaries are stable.

## Current Stage 6 focused task: original Font for the live tick HUD line

- The game HUD now leaves the world name/title and Back button on the existing
  UWP UI renderer, but suppresses only the custom `Tick N` line when the
  original Font is available.
- `UwpUiRenderer::drawOriginalFontTick()` draws that live counter through the
  original `Font::draw()` -> `Tesselator::draw()` GLES2 path using the same
  logical HUD coordinates and state restoration contract as the status-detail
  seam.  If the original Font is unavailable, the prior custom tick line is
  still used automatically.
- This is an independent HUD seam after the original terrain block batch; no
  `LevelRenderer`, `GameRenderer`, world-generation, entity, collision,
  storage, or input code was changed.
- No compilation was run, per the user's instruction.  User-side x64 UWP
  compilation and runtime validation is required.

## Validation expectation and next boundary

- Expected result: the world, terrain block batch, world-name HUD, and Back
  button remain usable; only the top-left `Tick N` glyphs use the original
  bitmap font.  The text should remain in the same logical position.
- If the tick line disappears while the rest of the HUD remains visible,
  isolate this seam by restoring the custom `drawText(tickText, ...)` call;
  do not roll back the terrain or status-font seams.
- If this passes, the next renderer step should be planned against the
  original `TileRenderer` dependency boundary rather than adding more custom
  geometry.  Full `LevelRenderer`/`GameRenderer` takeover remains deferred.

## Current Stage 6 focused task: original TileRenderer single-block seam

- Added the complete original `TileRenderer.cpp` translation unit and
  `TileRenderer.h` to the UWP project.  It is compiled with the same
  `MCPE_UWP`/GLES2 compatibility definitions as the existing original
  `Tesselator`, `Font`, and `Textures` sources; no Win32 project or source was
  changed.
- Earlier Stage 6 builds used `UwpOriginalTileRendererStatics.cpp` only to
  provide storage for the original `Minecraft::useAmbientOcclusion` option.
  Stage 7 now compiles the real original `Minecraft.cpp` lifecycle translation
  unit, which already owns that static definition; the statics-only translation
  unit is therefore retained as history but excluded from the UWP project to
  avoid a duplicate-symbol link error.
- `UwpWorldRenderer::drawOriginalTerrainBlock()` now finds one nearby visible
  block from the real `Level` mesh, obtains its actual `Tile`, and invokes the
  original `TileRenderer::tesselateBlockInWorld()` into the original
  `Tesselator`.  The existing ANGLE/GLES2 shader, original terrain atlas,
  projection/view matrix stack, depth state, and texture state are reused for
  the draw.
- The complete current UWP terrain mesh remains the authoritative fallback.
  `UwpWorldRenderer::setOriginalTileRendererEnabled(false)` disables only the
  original TileRenderer seam; a non-cube tile or a false return from the
  original method automatically falls back to the previously validated
  one-block Tesselator batch.  No `Chunk`, `LevelRenderer`, `GameRenderer`,
  entity renderer, world-generation, input, storage, or GUI path was added.
- The source list deliberately stops at `TileRenderer.cpp`.  Its uncalled GUI
  tile/entity-rendering methods are not wired into this seam; if the user's
  x64 link reports those as live unresolved references, that is the explicit
  dependency boundary for the next round rather than a reason to add empty
  stubs or alter the original implementation.

### Link repair after first user build

- The first x64 link correctly exposed the uncalled original
  `TileRenderer::renderTile()` dependency on `EntityTileRenderer`.  Rather
  than add a fake renderer or pull in the entire tile-entity dispatcher for a
  path that this round never calls, Debug|x64 now enables the linker's normal
  `/OPT:REF` dead-COMDAT elimination together with the existing per-file
  function-level linking.  The live cube seam remains unchanged and all
  original TileRenderer code stays in the translation unit.
- This is the first targeted repair for this dependency boundary.  If the
  next build reports a different live symbol, stop after that diagnostic
  rather than expanding into entity-tile rendering in this round.

### Second and final repair for this dependency boundary

- The user's next x64 link still retained the two `EntityTileRenderer`
  symbols, so `/OPT:REF` alone was insufficient for this project configuration.
- Added the original dependency closure required by those symbols:
  `EntityTileRenderer.cpp`, `TileEntityRenderer.cpp`,
  `TileEntityRenderDispatcher.cpp`, `ChestRenderer.cpp`, `SignRenderer.cpp`,
  and the original model geometry sources `ModelPart.cpp`, `Cube.cpp`, and
  `Polygon.cpp`.  These are source-level original MCPE files; no empty UWP
  implementations were introduced.
- This is the second repair allowed for this boundary.  If the next user build
  still reports a linker failure from this chain, stop and report it rather
  than adding another renderer subsystem in the TileRenderer round.

## Validation and next action

- No Visual Studio compilation was run, per the user's standing instruction;
  the user should perform the x64 UWP build and launch the world.  Expected
  behavior is unchanged terrain and controls, with one real cube submitted
  through the original TileRenderer/Tesselator path.  A blank world or a
  link failure should be reported with the first diagnostic line before any
  second repair attempt.
- If this passes, the next focused task is to decide whether to expand the
  original TileRenderer seam to a controlled set of real blocks or begin the
  `Chunk` dependency closure.  `LevelRenderer` and `GameRenderer` remain out
  of scope for this round.

### User validation result

- The user rebuilt the x64 UWP target after the dependency-closure repair and
  reports that the game now runs normally in-world.  This confirms the current
  TileRenderer seam and its original entity-tile link closure are buildable and
  do not disturb the existing world-rendering fallback at runtime.
- No additional TileRenderer dependencies are being added in this round.

## Current Stage 6 focused task: original Chunk/RenderChunk window

- Added the original `Chunk.cpp`/`Chunk.h` and `RenderList.cpp`/`RenderList.h`
  to the UWP project.  `Chunk::rebuildImpl()` remains the source of block
  geometry: it constructs the original `Region`, calls original
  `TileRenderer::tesselateInWorld()`, and commits the original
  `Tesselator::end(true, vbo)` result into three per-chunk VBO names.
- `UwpWorldRenderer` now owns a bounded 3x3 X/Z by 8-layer window of original
  `Chunk` objects (72 chunks).  The window recenters on the player's chunk,
  allocates and releases the original `RenderChunk` VBO records, and rebuilds
  at most four dirty chunks per render call.  The existing UWP terrain mesh
  remains visible during a partial rebuild and is still the runtime fallback.
- The camera now uses the real player X/Z coordinates instead of clamping to
  the first 48x48 fallback slice.  The original `RenderList` receives the
  camera offset while its model-view orientation is kept translation-free, so
  world-space Chunk vertices are not translated twice when crossing boundaries.
- `UwpWorldRenderer` is now a `LevelListener`.  `tileChanged()`,
  `setTilesDirty()`, and `allChanged()` mark overlapping original chunks dirty
  (including a one-block border for neighbor-face visibility), so a changed
  block invalidates the relevant cached geometry instead of leaving stale VBOs.
- `RenderList::renderChunks()` now has a UWP GLES2/ANGLE branch that binds the
  shared shader's position/UV/color attributes before submitting each original
  VBO.  The desktop Win32 branch is unchanged.  This is a backend adaptation,
  not a replacement chunk mesh implementation.
- No `LevelRenderer`, `GameRenderer`, full frustum sorting, or Win32 behavior
  was changed.  The original Textures/Font/Tesselator resource contract and
  the previously validated TileRenderer seam remain intact.  :codex-annotation{index="1"}

### Static validation and manual validation required

- The first user-side build exposed an incomplete-type error from the public
  `std::unique_ptr<Chunk>` member (`C2027`/`C2338` in generated memory code).
  Fixed this without including the macro-heavy original renderer header in
  every UWP consumer: `UwpChunkDeleter` is declared beside the forward
  declaration and defined after `Chunk.h` in `UwpWorldRenderer.cpp`.  Chunk
  ownership and destruction remain RAII-managed.
- No Visual Studio compilation or runtime launch was performed in this round,
  following the standing preference that the user compiles locally.  The UWP
  project XML parses successfully and all newly referenced source/header files
  exist.
- The user should perform an x64 UWP build and enter a world.  Verify: several
  adjacent chunks are visible; moving across an X/Z chunk boundary keeps their
  world coordinates stable; a block change causes the affected chunk (and its
  border neighbor) to rebuild; disabling `setOriginalChunkRendererEnabled`
  returns to the prior fallback path.  If the build fails, report the first
  compile/link diagnostic before expanding this dependency closure.

### Next boundary

- If the bounded Chunk window passes, the next stage is controlled visibility
  and scheduling integration around the original `LevelRenderer` dependency
  boundary.  Do not add `LevelRenderer`, `GameRenderer`, or the complete
  Minecraft main loop in this round.

## Stage 6 follow-up: chunk-window and shutdown stability repair

The user's runtime report exposed two independent lifecycle/cache issues.  The
following targeted fixes were applied without changing the Win32 client:

- `MainPage.xaml.cpp`: the game-back path now calls
  `m_worldRenderer.setLevel(nullptr)` before `m_core.stop()`.  `stop()` destroys
  the native core and its `Level`; detaching the `LevelListener` afterwards was
  a use-after-free and matches the debugger stop in STL iterator adoption seen
  while leaving a world.
- `UwpWorldRenderer.cpp`: when the player crosses an X/Z chunk boundary, the
  existing 72 original `Chunk` objects and their VBO names are repositioned
  with `Chunk::setPos()` instead of being destroyed.  Their previous VBO data
  remains drawable until each dirty slot reaches the bounded rebuild cursor,
  so the overlapping old window prevents an all-world blank frame.  Initial
  creation and level changes still use the existing destroy/recreate path.
- `UwpWorldRenderer.cpp`: original chunk submission no longer rejects a chunk
  solely because `empty[0]` is true.  A repositioned dirty chunk may have
  valid previous VBO data; submission is now gated by visibility and the actual
  `RenderChunk::vertexCount`.
- `Chunk.cpp`: under `MCPE_UWP`, an empty rebuild explicitly resets the
  layer's VBO id, vertex count, and world position.  This prevents
  removed/empty blocks from leaving stale geometry in the cache after a
  rebuild, without changing the Win32 build's behavior.
- `UwpWorldRenderer.cpp`: the fallback mesh is now drawn only while the
  original Chunk window is still rebuilding (or when the original path is
  disabled/unavailable).  Once the original window is ready, the original
  TileRenderer VBOs are the sole terrain producer.  This removes depth seams
  caused by overlaying two independently tessellated copies of the same block,
  which was the likely source of the reported side-face strips.

No Visual Studio compilation or runtime launch was performed, following the
user's instruction to compile locally.  The project files and source edits were
checked statically.  Manual validation should cover:

1. Enter a world, cross both an X and a Z 16-block boundary, and confirm that
   the overlapping terrain stays visible while new chunks fill in.
2. Remove or alter a block at a chunk edge and confirm the changed block and
   neighboring face update after the rebuild, with no old VBO ghost remaining.
3. Use the in-game Back button repeatedly and confirm the world exits without
   an STL iterator/access-violation break.  Also close the app while a world is
   active to exercise renderer destruction while the core is still alive.
4. Toggle the existing original-chunk renderer switch off and confirm the
   prior `UwpWorldRenderer` fallback remains unchanged.

If a side-face discrepancy persists after this repair, the next diagnostic
should compare an original-only frame against the fallback-only frame; do not
add `LevelRenderer` until that comparison is complete.

## Stage 6 follow-up: depth-occlusion correction

The user clarified that the remaining side problem was geometric occlusion:
faces hidden behind nearer blocks were still visible.  Static inspection found
the direct cause in `UwpWorldRenderer::drawOriginalChunks()`: the original
Chunk VBO pass always used `glDepthMask(GL_FALSE)`.  Depth testing was enabled,
but no submitted face could update the depth buffer, so later hidden faces
could not be rejected within the batch.

- `UwpWorldRenderer.h/.cpp`: the Chunk and diagnostic single-block draw paths
  now receive a `writeDepth` flag.  During a partial window rebuild it remains
  false so stale original VBOs cannot occlude the fallback mesh; once the
  original window is ready it is true, restoring normal nearer-face occlusion.
- The depth mask is restored to `GL_TRUE` after either pass, preserving the
  existing renderer state boundary.

No Visual Studio compilation or runtime launch was performed.  The project XML
and whitespace/static checks pass.  The next manual run should specifically
look at a terrain step or block wall from an oblique angle: a farther left/back
face must disappear behind the nearer right/front face.  If it remains visible,
the next check should isolate whether the symptom is from TileRenderer's face
visibility test at chunk borders or from the camera/projection depth path.

## Stage 6 follow-up: unified terrain backend and LevelRenderer boundary

The requested Stage 6 remainder was inspected and split at the actual client
ownership boundary instead of introducing a second chunk scheduler.

### Unified runtime backend (implemented)

- `UwpWorldRenderer.h/.cpp` now expose one `UwpRenderBackend` state with
  `Auto`, `OriginalChunk`, and `UwpFallback` values. `Auto` selects the
  original Chunk/TileRenderer VBO path only after the original terrain texture
  and the current Chunk window are ready; otherwise it uses the existing UWP
  terrain mesh. Explicit `OriginalChunk` keeps the same safe fallback while
  the requested path is unavailable, so a transient rebuild cannot become a
  permanent blank frame.
- The existing `setOriginalTileRendererEnabled()` and
  `setOriginalChunkRendererEnabled()` methods remain source-compatible and are
  now independent gates of the unified decision. Disabling either gate makes
  the effective frame backend `UwpFallback` without destroying cached original
  VBOs.
- The normal world frame no longer submits the old single-block TileRenderer
  diagnostic together with a full terrain backend. The diagnostic function is
  retained for isolated probing, while Chunk owns the original TileRenderer
  terrain batch. This enforces one terrain producer and one depth-writing
  policy per frame.
- `MainPage.xaml.cpp` adds F5/F6/F7 debug controls for Auto, OriginalChunk, and
  UwpFallback respectively. These controls only change the renderer mode; they
  do not modify Level data or the Win32 target.

### LevelRenderer dependency finding (not force-integrated)

The original `LevelRenderer.cpp` was inspected rather than copied into a new
UWP scheduler. Its terrain lifecycle functions are not a self-contained
module: construction and `allChanged()` require a real `Minecraft` owner for
`Options`, texture ownership, view-distance/fancy-graphics settings and the
camera target; `render()`/`renderChunks()` also read
`Minecraft::cameraTargetPlayer`. The same translation unit directly includes
and links client-wide services (`GameRenderer`, `ParticleEngine`, `SoundEngine`,
`Culler`, `EntityRenderDispatcher`, tile-entity dispatch, `PerfTimer`, and
model code). The current UWP world owner is `UwpMinecraftCore`, not
`Minecraft`, and the full `Minecraft`/`GameRenderer` path is explicitly
outside this Stage 6 round.

Therefore no fake `Minecraft` object, empty service implementations, or copied
LevelRenderer algorithm was added. The existing original Chunk/RenderList
path remains active and the new backend switch is ready for a later
LevelRenderer bridge once a real original-client owner is available. This is
the required stop point under the task rule against large refactors.

### Static validation and manual validation

- No Visual Studio compilation or runtime launch was performed, following the
  standing instruction that the user compiles locally.
- `git diff --check` was run; only pre-existing line-ending normalization
  warnings were reported. The UWP project XML remains unchanged and valid.
- User-side x64 UWP validation should press F5, F6, and F7 in a running world,
  then cross an X/Z chunk boundary and change a block. Verify that Auto and
  OriginalChunk show the original Chunk path once ready, UwpFallback shows the
  prior UWP mesh, and switching back to Auto does not leave stale VBOs or a
  blank world. Back/enter-world should still detach the Level before core
  shutdown.

### Next recommendation

The next safe step is not `GameRenderer` yet. First introduce a real
UWP-compatible original-client owner (or a narrowly scoped original
`LevelRenderer` constructor that receives actual `Level`, `Player`, `Options`,
and `Textures` objects) and compile only its terrain lifecycle methods. If
that owner cannot be supplied without bringing in full `Minecraft`, defer
LevelRenderer runtime takeover and continue using this explicit backend switch
as the stable platform seam.

## Stage 6 follow-up: fallback backend coverage after backend unification

The user reported that the original Chunk backend was mostly usable, but the
explicit UWP fallback backend rendered a completely empty frame. The cause was
the coordinate ownership boundary: the fallback mesh was still generated in a
fixed world slice around the old origin, while the unified renderer now uses the
real `Player` X/Z coordinates for its camera. After moving away from that slice,
the camera could legitimately see no fallback triangles even though the Level
was valid.

`handheld/src/platform/uwp/UwpWorldRenderer.h/.cpp` now keeps the fallback mesh
center and rebuilds its bounded 48x48 slice around the current player whenever
the fallback can be visible and the player has moved more than 20 blocks from
the previous center. The same coverage is available while the original Chunk
window is being rebuilt, so Auto mode no longer exposes the old fixed-origin
blank frame during a transition. An empty first scan is retried as well, since
the original Level chunk source may finish generating data after the renderer's
first frame. The fallback still uses the existing real `Level` data and the
existing UWP texture/colour path; no second chunk cache or new world-generation
path was introduced.

No Visual Studio compilation or runtime launch was performed, following the
standing instruction that the user compiles locally. `git diff --check` passed
(only pre-existing line-ending normalization warnings were reported), and the
source reference check confirms the new two-argument mesh builder has no stale
call sites.

### Manual validation for this fix

1. Build x64 UWP and enter a world. Press F7 to select `UwpFallback`; terrain
   should remain visible at the current player position, including after moving
   across at least one chunk boundary.
2. Press F5 for `Auto`, cross a chunk boundary, and confirm there is no
   permanent empty frame while the original window is rebuilt.
3. Press F6 for `OriginalChunk`, change a block, then return to F7 and verify
   that the fallback mesh is rebuilt around the current position.
4. Leave and re-enter the world to confirm that the recenter state is reset and
   no stale mesh is reused.

If F7 is still blank after this change, the next diagnostic should display the
player X/Z, `m_vertices.size()`, and `m_terrainTexture` in the existing compact
two-line status overlay; do not add another rendering implementation before
checking those three values.

## Stage 7: lifecycle-only original client host (completed boundary for this round)

### Dependency conclusion

The Win32 startup chain was rechecked in `main_win32.h` and the original
translation units:

`AppPlatform_win32` → current EGL/WGL context → `new NinecraftApp` →
`App::init(AppContext)` → `NinecraftApp::init()` → static registries, GL/Tesselator,
I18n, `Minecraft::init()`, storage source and the original update loop.

`NinecraftApp::init()` is not an isolatable UWP entry point: after its registry
and GL setup it immediately calls `Minecraft::init()`, whose client branch
constructs synchronous `Textures`, `Font`, `LevelRenderer`, `GameRenderer`,
`ParticleEngine`, `PerfRenderer`, GUI/screens and sound state. Its later update
path owns ticks, input, rendering and buffer presentation. Entering that chain
would violate this round's explicit boundary and create a second owner beside
`UwpCoreSession`/`UwpWorldRenderer`.

### Implemented host seam

- Added `UwpOriginalClientBridge.h/.cpp`, an opaque bridge that constructs and
  destroys the real original `NinecraftApp` object. The bridge compiles the
  original `NinecraftApp.cpp` and `client/Minecraft.cpp` with
  `STANDALONE_SERVER + NO_NETWORK + MCPE_UWP_CLIENT_LIFECYCLE_ONLY`; this keeps
  the original object layout and constructors/destructors while excluding the
  not-yet-adapted renderer, audio, storage and network startup graph.
- `App.h` now has a narrow `bindContextWithoutInit()` platform seam. For this
  lifecycle boundary the bridge sees the same EGL-bearing `AppContext` layout
  as UWP (the `NO_EGL` shortcut is disabled only for the lifecycle macro), so
  the actual ANGLE display/context/surface and `AppPlatform_uwp` are attached to
  the original object with `doRender=false`.
- `UwpOriginalClientHost` now owns that object and context, validates the
  platform binding, and destroys the object before EGL shutdown. MainPage binds
  it on load/navigation and clears it during page shutdown; no original update,
  input, swap, world or renderer call is routed through it.
- The lifecycle-only macro makes the original virtual `init`, `update`,
  `selectLevel`, `setLevel` and back handlers linkable no-op boundaries. This is
  not a replacement client implementation: the default Win32 source remains
  unchanged, and the real methods remain selected for the normal build.
- `NinecraftApp` now tracks ownership of static registries. A constructed but
  not initialized lifecycle object cannot tear down registries owned by the
  active `UwpCoreSession` during page destruction.

### Actual initialization boundary

The following operations execute in the UWP host after the user's next build:

1. `new NinecraftApp()` and the original `Minecraft` base constructor.
2. `AppContext` copy containing the real ANGLE handles and
   `AppPlatform_uwp`, with `doRender=false`.
3. Existing original `I18n::loadLanguage(platform(), "en_US")` call.
4. Opaque destructor path for `NinecraftApp`/`Minecraft` before EGL teardown.

The following are intentionally **not** executed in this round:

- `App::init(AppContext&)`
- `NinecraftApp::init()`
- `Minecraft::init()`
- `Minecraft::update()` / original input queue
- `GameRenderer`, `LevelRenderer`, original GUI/screens, audio or network

No Visual Studio compilation or runtime launch was performed, following the
standing instruction that the user compiles locally. Static validation passed:
the UWP project XML parses, all new bridge/project references are present, and
`git diff --check` reports only the repository's existing line-ending warnings.

### Manual validation required

After an x64 UWP build, verify the existing two-line startup diagnostic still
reaches the menu, then enter/leave a world and navigate away from MainPage. The
important checks are that the original host binding does not change terrain
ownership, no duplicate tick/input path appears, and page/EGL shutdown produces
no crash. The displayed status should include `NinecraftApp host + AppContext +
I18n bound`.

### Next recommendation

If this lifecycle boundary is stable, the next single task should be a client
foundation batch for the original `Options`/texture cache and a narrowly scoped
`Minecraft::init()` checkpoint. Do not enable `GameRenderer`, `LevelRenderer`,
or `NinecraftApp::update()` until that checkpoint has a real storage and
resource contract and an explicit single owner for input, world ticks and
presentation.

### Stage 7 compile-boundary correction

The first local build reported C2601/C1075 errors in `NinecraftApp.cpp` and
`Minecraft.cpp`. The common cause was the lifecycle-only `#if/#else` wrapper:
the function closing brace had been placed inside the excluded `#else` branch,
so the compiler treated subsequent methods as nested local definitions. The
wrappers for `NinecraftApp::init`, `teardown`, and `update`, `handleBack`, plus
`Minecraft::update` and `init`, now close the conditional block before the
function brace. No client behavior or Win32 source path changed. Compilation
and runtime remain intentionally unverified here; rebuild the UWP project
locally before the next lifecycle test.

The subsequent local error list exposed two more complete-client-only callbacks:
`Minecraft::optionUpdated(bool)` referenced `LevelRenderer`, while
`Minecraft::optionUpdated(int)` referenced `Multitouch` and `gui`. Both callbacks
now use the same lifecycle-only no-op boundary (with unused arguments consumed)
and retain their original bodies for every non-UWP-lifecycle build.

The next local link reported a duplicate definition of
`Minecraft::useAmbientOcclusion`: the Stage 6 statics-only seam and the newly
linked original `Minecraft.cpp` both provided it. The UWP project now excludes
the obsolete statics-only translation unit, leaving the original client
translation unit as the single owner. The source file remains on disk for
historical reference and is not part of the build.

The next link pass exposed the remaining complete-client graph reachable from
the lifecycle translation unit: options reload, multiplayer discovery,
`_levelGenerated`, game-mode selection, and the original tick dispatcher.
These methods are now lifecycle-only no-ops with their arguments consumed;
their original implementations remain unchanged for non-UWP-lifecycle builds.
This keeps the host at construction/context/destruction only and prevents
network, command-server, GUI, and game-mode dependencies from entering this
stage.

The final remaining link error came from the original `Minecraft` destructor's
unconditional `_commandServer` delete. The lifecycle object never creates that
server, so its destructor call is now excluded only for
`MCPE_UWP_CLIENT_LIFECYCLE_ONLY`; all normal builds retain the original cleanup.

### Stage 7 dependency continuation: original storage roots

The next bounded dependency is the original client's storage-path contract,
without entering `Minecraft::init()`. `UwpStoragePaths` now exposes UTF-8
conversions for `ApplicationData::Current->LocalFolder` and `LocalCacheFolder`.
`UwpOriginalClientBridge` assigns those roots to the real original
`NinecraftApp::externalStoragePath` and `externalCacheStoragePath` fields during
the existing context bind. This gives a future original storage source and
`OptionsFile` the correct UWP roots while keeping the current UWP world/session
owner unchanged. No options read/write, world load, or original tick/render call
is made in this round.

### Stage 7 dependency continuation: original Options implementation

The original `client/Options.cpp` is now part of the UWP lifecycle build with
the same `MCPE_UWP_CLIENT_LIFECYCLE_ONLY` boundary. Its default-value,
file-backed `update()`/`save()`, parsing, and option-state logic are reused
unchanged. Only `syncAmbientOcclusion()` skips the not-yet-owned
`LevelRenderer::allChanged()` notification, and the Win32 skin import/clear
callbacks are lifecycle-only no-ops; non-UWP builds retain their original
implementations. This prepares a real original `Options` object for the future
init checkpoint without invoking it or changing current UWP settings UI.

The first local compile exposed that `STANDALONE_SERVER` also disables all GL
typedefs in `gles.h`; this was inappropriate for `Options.cpp`, which includes
the original `Textures.h` even though its renderer callbacks are gated. The
UWP project entry for `Options.cpp` now keeps `MCPE_UWP_CLIENT_LIFECYCLE_ONLY`
but omits `STANDALONE_SERVER`, so it sees the same ANGLE GLES2 type contract as
the original client renderer while still excluding the not-yet-owned callbacks.

### Stage 7 dependency continuation: `OptionStrings`

The next local link reported only unresolved `OptionStrings::*` static string
definitions from the newly linked original `Options.cpp`. The original
`client/OptionStrings.cpp` translation unit is now included in the UWP project;
it supplies those definitions without changing option logic or Win32 behavior.
No compile or runtime test was performed here; rebuild the UWP project locally
to verify the link closure.

### Main client ownership handoff audit (current main goal)

The user-side x64 UWP build now succeeds after adding `OptionStrings.cpp`.
This round performed a source-level stability and ownership audit without
running another build or changing the active game loop.

The current owners are still unambiguous:

- `UwpCoreSession` owns the live `CreatorLevel`, its loaded `Player`, world
  time, save/close path, and the only active simulation tick (`Level::tick()`
  followed by the temporary player movement adapter).
- `UwpGameLoop` is the frame scheduler. `MainPage::OnGameTick()` calls only
  `m_core.tick()`, and `MainPage::OnGameRender()` calls only
  `UwpWorldRenderer::render()` plus the UWP UI renderer.
- `UwpOriginalClientHost` owns a constructed original `NinecraftApp` object,
  its bound `AppContext`, UWP platform, storage roots, and locale binding, but
  it does not call the original update loop, world selection, input consumer,
  or buffer swap.
- The active terrain producer is selected by `UwpWorldRenderer`'s
  `Auto`/`OriginalChunk`/`UwpFallback` backend. The original client does not
  submit terrain yet.

The actual original functions that have run in UWP are the `Minecraft` base
constructor/destructor path, `I18n::loadLanguage()`, original resource
contracts (`Textures`, `Font`, `Tesselator`, `TileRenderer`, `Chunk`, and
`RenderList`) through their validated seams, and the original world/storage
methods used by `UwpCoreSession`. `NinecraftApp::init()`,
`Minecraft::init()`, `NinecraftApp::update()`, `Minecraft::update()`,
`Minecraft::tickInput()`, `LevelRenderer`, and `GameRenderer` have not been
executed by the UWP host.

The blocking call graph is concrete rather than a missing single symbol:

`NinecraftApp::init()` → static registries → GL/Tesselator → I18n →
`Minecraft::init()` → `Textures`/GUI resources → `LevelRenderer(this)` →
`GameRenderer(this)` → `ParticleEngine`/`Font`/`PerfRenderer` → `User` and
option reload. `NinecraftApp::update()` then commits multitouch, calls
`Minecraft::update()`, presents through `App::swapBuffers()`, and resets mouse
state. `Minecraft::update()` owns timer advancement, input, level ticks,
lighting, renderer, sound, particles and GUI.

Therefore the next handoff cannot be made by enabling one more source file or
by calling `Minecraft::init()` while the current lifecycle-only build remains
`STANDALONE_SERVER`. The client build must first provide the non-server
`Minecraft` object layout and the real `LevelRenderer`/`GameRenderer` source
closure, then transfer world/player ownership from `UwpCoreSession`, input
consumption from the temporary keyboard bridge, tick scheduling from
`UwpGameLoop`, and presentation ownership from `MainPage` in one controlled
handoff. Enabling only part of that graph would either fail to link or create
two owners of the same world/tick/render state, so no such partial switch was
made in this round.

### Verification boundary and next action

The current source/project checks pass, and the user has confirmed the latest
UWP compilation succeeds. Runtime stability for this exact post-`OptionStrings`
build still needs one manual smoke run: start the app, enter/reopen a world,
leave it, and navigate away from `MainPage` while confirming the existing
two-line diagnostics and no duplicate ticks or shutdown crash.

The next implementation should be one controlled ownership-handoff batch:
compile the non-server original client dependency closure with UWP-safe
presentation disabled, add a real bridge for `Minecraft::setLevel()` and
player ownership, and only then route one frame through `NinecraftApp::update()`.
Until that batch is complete, the current UWP loop and renderer remain the
intentional owners and the fallback backend must stay enabled.

### Main client dependency closure: expanded compile/link basis

This round extends the previous non-server, lifecycle-only client closure in
the UWP project.  The original source is still used directly; no replacement
GUI, entity, input, or renderer implementations were added.  The project now
includes the original client GUI component and screen trees (with the already
explicit `ScreenChooser.cpp` excluded from the wildcard), non-geometry entity
models, entity renderers, local/remote player and player-input sources, the
original `IConfigListener.cpp` and `MouseHandler.cpp`, the platform input
implementations (`Controller.cpp`, `Mouse.cpp`, `Multitouch.cpp`), and the
original `util/PerfRenderer.cpp` required by the later `Minecraft::init()`
graphics path.  All of these entries use `WIN32;NO_NETWORK;MCPE_UWP;
MCPE_UWP_CLIENT_LIFECYCLE_ONLY`; the existing explicit geometry, texture,
renderer, and `ScreenChooser` entries are excluded from the wildcards so each
translation unit appears once.  `client/sound/*.cpp` remains intentionally
excluded: this round prepares the audio symbols for a later controlled
handoff but does not initialize or add an audio runtime.

The macro boundary is deliberate.  The core UWP world/storage sources keep
`STANDALONE_SERVER;NO_NETWORK`, while the original client closure omits
`STANDALONE_SERVER` so `Minecraft`, `LevelRenderer`, `GameRenderer`, `Gui`,
input, and renderer declarations have the non-server layout.  The lifecycle
macro still returns before `NinecraftApp::init/update/teardown` and before
`Minecraft::init/update/tick`, so this source expansion cannot create a second
world, input consumer, tick owner, or renderer owner.  Sound construction and
destruction remain gated in `Minecraft.cpp` for the same reason.

Static validation after the expansion: the UWP project XML parses, wildcard
expansion resolves 148 source files with zero duplicate entries and zero
missing files, and `git diff --check` reports only the repository's existing
LF/CRLF normalization warnings.  No Visual Studio build or runtime launch was
performed here; the user must compile the x64 UWP configuration and run the
existing smoke path.  A successful build establishes **compiled/linked** for
this closure; it does not mean the original client is initialized or running.

Current ownership is unchanged: `UwpCoreSession` owns the live world/player,
`UwpGameLoop` owns tick/input scheduling, and `UwpWorldRenderer` owns terrain
submission.  `UwpOriginalClientHost` owns only the dormant original
`NinecraftApp`/`Minecraft` object and `AppContext`.  The next controlled step,
after the user's build and smoke test, is to add an explicit world/player
transfer contract and only then consider one guarded `Minecraft::init()` or
one frame of `NinecraftApp::update()`; do not enable both loops together.

### Current boundary after dependency-closure change

| State | Result |
|---|---|
| Source included | Original non-server `Minecraft`/`NinecraftApp`, `LevelRenderer`, `GameRenderer` and listed direct dependencies are in the UWP project. |
| Compiled | Not yet verified in this round; user-side build is required. |
| Linked | Not yet verified in this round; no new linker result is claimed. |
| Initialized | No: lifecycle macro still stops before `NinecraftApp::init()` and `Minecraft::init()`. |
| Running | No: UWP still owns world ticks, input consumption, terrain rendering, and presentation. |

If the next user build reports errors, repair only the exact non-server client
closure issue first. Do not call `Minecraft::init()` or route
`NinecraftApp::update()` until this configuration compiles and the ownership
handoff has an explicit world/player transfer path.

### Targeted UWP compile repair: Win32-only input/window APIs

The first user build of the expanded client closure reported eight UWP
compile errors for `GetAsyncKeyState`, `ShowCursor`, `GetClientRect`, and
`ClientToScreen`/related cursor APIs.  These calls belong to the Win32 desktop
window path, not to the UWP CoreWindow/XAML input path.  The original sources
now keep those branches under `!MCPE_UWP`: adventure-setting Ctrl shortcuts
and multiplayer Tab suppression are omitted while `NO_NETWORK` is active;
`MouseHandler` no longer includes or calls Win32 cursor capture APIs on UWP;
and the desktop-only skin import code in `Options`/`LocalPlayer` is excluded
from the UWP client translation units.  Win32 builds retain their original
branches unchanged.

No replacement input implementation was added and no original update loop was
enabled.  Static source review confirms every reported Win32 API occurrence is
inside a desktop-only conditional.  The user should rebuild x64 UWP and report
the next actual compiler/linker boundary; this turn did not run a build.

### Targeted client-closure linker repair: original SoundEngine and no-network pause path

The next user-side link reported calls from the newly included original client
sources to `SoundEngine::enable`, `SoundEngine::play`, and
`SoundEngine::playUI`, plus one `ServerSideNetworkHandler::allowIncomingConnections`
call from `PauseScreen`.  The UWP project now includes the original
`client/sound/SoundEngine.cpp` and `client/sound/Sound.cpp` translation units,
so those symbols and the original embedded sound descriptors are provided by
the source tree rather than by replacement functions.  `SoundEngine.h` keeps
the Win32/OpenAL `SoundSystemAL` selection unchanged for desktop builds, but
selects the existing original `SoundSystem` base for `MCPE_UWP`; that base is
the repository's intentional unavailable-audio implementation and performs no
OpenAL initialization or playback.  This closes the client link dependency
without enabling audio runtime work.

`PauseScreen::buttonClicked` now keeps the original server-visibility toggle
for non-UWP builds and excludes only that network mutation under `MCPE_UWP`.
The UWP project already defines `NO_NETWORK`, so this is a platform boundary,
not a new network implementation or a fabricated handler symbol.  The rest of
the original pause-screen source remains compiled.

No Visual Studio build or app launch was performed in this round, per the
working agreement that the user performs compilation.  Static checks completed
after the edit: the project XML remains well-formed, the new source paths exist,
the UWP sound selection cannot include `SoundSystemAL`, and `git diff --check`
has no new whitespace errors.  The user should rebuild x64 UWP; this result is
source/project-ready but not yet a claim of compiled, linked, initialized, or
running status.

### Controlled original Minecraft main-loop entry

The UWP host now has an explicit, guarded entry into the original client
initialization and update chain. `UwpOriginalClientBridge` exposes the real
`App::init(AppContext&)` dispatch and `NinecraftApp::update()` without exposing
the original class layout to XAML. `UwpOriginalClientHost` tracks bound,
initialized, and loop-enabled states and resets all of them during clear.

`MainPage` waits until all five synchronous startup textures have succeeded,
then invokes the original `NinecraftApp::init()` and `Minecraft::init()` once.
While no UWP world is active, `UwpGameLoop` routes its tick callback to the
original `NinecraftApp::update()`; UWP's CompositionTarget remains the sole
presenter because the bound `AppContext` has `doRender=false`. Before saved or
new-world loading, the original loop is disabled. `UwpCoreSession` remains
the sole owner of the UWP world/player, tick, input bridge, and terrain
renderer. Returning from a world re-enables only the original menu/client
loop, so the two clients never update the same world.

The UWP-only `MCPE_UWP_ORIGINAL_CLIENT_LOOP` definition re-enables the original
`NinecraftApp`/`Minecraft` methods while preserving the lifecycle guard for the
other closure units. UWP sound construction is deferred until after
`Options::initDefaultValues()` and uses the existing no-op `SoundSystem`; no
OpenAL, audio, network, GUI replacement, or Win32 behavior was added. A UWP
options path is supplied through the existing `Options` implementation.

Global registry ownership is now explicit in `UwpCoreSession`: if the original
client initialized Material/Tile/Item/Biome registries first, the UWP session
borrows them and does not tear them down. This prevents duplicate teardown
when the page stops a world before destroying the original client host.

Static validation after this edit: the project XML parses; the bridge, host,
MainPage routing, macro definitions, and registry ownership checks are present;
`git diff --check` reports only existing repository LF/CRLF normalization
warnings. No Visual Studio build or runtime launch was performed, per the
working agreement. Therefore this round is source-ready but not yet claimed
compiled, linked, initialized, or running. The user's next build/run should
confirm that the startup diagnostic reaches `Original Minecraft init OK` and
that the menu remains responsive, then enter/leave a world to verify the
ownership switch and shutdown path.

Current ownership after this change:

| State | World/player | Tick | Rendering/presentation |
|---|---|---|---|
| Original loop enabled, no UWP world | No world transfer; original client is menu/client state only | `NinecraftApp::update()` → `Minecraft::update()` | Original client may issue its GL calls; UWP UI remains presenter |
| UWP world active | `UwpCoreSession` | `UwpGameLoop` → `UwpCoreSession::tick()` | `UwpWorldRenderer` plus UWP UI |

Still missing for full Minecraft world takeover: a safe transfer of the real
`Level` and `LocalPlayer` into `Minecraft::setLevel()`, input ownership through
`Minecraft::tickInput()`, and a decision to let original `GameRenderer` /
`LevelRenderer` own world rendering. Those are intentionally not started by
this controlled menu-loop entry.

### Targeted no-network client-loop linker boundary

The first user-side link after enabling the controlled loop reported unresolved
symbols from the desktop server/multiplayer branches: `ClientSideNetworkHandler`,
`CThread`, `ServerLevel`, and `CommandServer`. These paths are not used by the
current UWP menu-only loop and would unnecessarily expand the network/server
closure. Under the existing UWP `NO_NETWORK` boundary, `locateMultiplayer()`
now remains dormant, command-server ticking/destruction is excluded, and the
UWP `selectLevel()`, `setLevel()`, and `_levelGenerated()` paths return without
starting the desktop server/threaded Level handoff. Win32 and non-UWP builds
retain their original implementations.

This is an explicit ownership boundary, not a replacement implementation: the
UWP world continues to belong to `UwpCoreSession`, while the original client
loop remains limited to menu/client state. No build or runtime launch was
performed here; the user should rebuild x64 UWP and report the next exact
compiler/linker result. If this link succeeds, the next required dependency
work is the deliberate `Level`/`LocalPlayer` transfer, not re-enabling these
desktop server paths prematurely.

### UWP presentation isolation for the controlled original loop

The first successful runtime launch after enabling the original client loop
showed the menu briefly and then left only the blue XAML surface and startup
diagnostic. The original `Minecraft::update()` was entering `GameRenderer::render()`
even though `MainPage`/`UwpUiRenderer` remained the intended UWP presentation
owner. `GameRenderer` clears and reconfigures the shared GLES surface, so it
could erase or invalidate the subsequent UWP menu draw.

`Minecraft.cpp` now keeps the original timer, tick, lighting and sound update
path active, but conditionally skips the original `GameRenderer::render()` only
when both `MCPE_UWP` and `MCPE_UWP_ORIGINAL_CLIENT_LOOP` are defined. The Win32
path and the existing UWP world path are unchanged. This preserves a single
presentation owner while the original client loop is still menu/client-state
only.

No build or runtime launch was performed after this source change. The next
user-side x64 UWP build should verify that the main menu remains visible after
the original-client initialization diagnostic and that entering a world still
switches to the existing UWP world renderer.

### Second pass: restore the UWP UI GLES state contract

The first presentation isolation did not change the user's blank-screen
behavior. The remaining shared-context risk was stale GLES state left by the
original client initialization/compatibility layer: an enabled scissor test,
disabled color channels, a nonzero array-buffer binding, or a non-default
active texture unit can make the UWP UI shader submit successfully while
producing no visible menu geometry.

`UwpUiRenderer::render()` now re-establishes its own state before drawing:
viewport, scissor/stencil/cull/depth state, depth and color write masks,
texture unit 0, and null array/element buffers. This is scoped to the UWP UI
pass and does not alter the original renderer or the world-renderer backend.
The original client update remains tick-only at this point; its
`GameRenderer::render()` is still isolated in the controlled UWP loop.

No build or runtime launch was performed after this second pass. Rebuild and
run the app again; if the menu is still absent, the next diagnostic should be
the UWP UI shader/program status immediately before `m_ui.render()`, rather
than further changing Minecraft ownership or world logic.

### Controlled original Minecraft world-tick handoff

The UWP host now has a non-owning world handoff for the original client tick.
`UwpCoreSession` remains the sole owner of `Level`, `LocalPlayer`,
`LevelStorage`, and the external storage source.  When the original client has
completed `Minecraft::init()`, new worlds use the genuine `LocalPlayer`
constructor and are attached through `Minecraft::attachUwpWorld()`.  Minecraft
borrows those pointers, binds the level's existing no-network RakNet instance,
initializes its original `GameMode`/input holder, and gives the `LocalPlayer`
the original `KeyboardInput` path.  `Minecraft::detachUwpWorld()` clears those
borrowed pointers without deleting them; the UWP session then performs the
normal save and destruction.

The active game tick call chain is now:

`UwpGameLoop` → `NinecraftApp::update()` → `Minecraft::update()` →
`Minecraft::tick()`.

`UwpCoreSession::tick()` returns immediately for an original-client-backed
world, so the level and player are not updated twice.  `MainPage` still owns
the frame scheduler, UWP terrain rendering, UI, and EGL presentation.  Its
keyboard bridge feeds the original `Keyboard` queue; while the handoff is
active the fallback movement adapter is disabled and `Minecraft::tickInput()`
is the only movement consumer.  The original client does not call
`swapBuffers()` on this UWP path.

World back-navigation, page shutdown, and host destruction explicitly disable
the original world tick, detach Minecraft, detach the UWP renderer, and only
then stop/save the UWP session.  This ordering prevents dangling level
listeners and double deletion.  The existing `OriginalChunk`, `UwpFallback`,
and `Auto` render backends remain unchanged.

Source-level validation completed: bridge/host/core/MainPage ownership paths
and project macro layout were checked, and the UWP core translation unit now
uses the same non-server client class layout as `Minecraft` when it constructs
`LocalPlayer`.  No Visual Studio build or runtime launch was performed here;
the user must rebuild x64 UWP and manually verify entering a world, movement,
jump, block updates, save/exit/re-entry, and clean page/application shutdown.
Until that run succeeds, the original world tick is source-wired but not
claimed as runtime-verified.  Full original GameRenderer/LevelRenderer
ownership and complete original input/UI behavior remain intentionally
deferred.

### UWP original-tick movement direction correction

The user has now verified that the original Minecraft tick is active in-world:
movement, auto-jump, and the former input stalls are behaving as expected.
The remaining defect was an exact sign reversal on both movement axes (W/S and
A/D) in the UWP presentation.  `KeyboardInput::tick()` now applies a
UWP-only motion-vector conversion while preserving the original key bindings
(`W=forward`, `A=left`, `S=back`, `D=right`).  The desktop/Win32 signs are
unchanged, and the UWP fallback adapter is untouched because it is not used
while the original world tick owns input.

Source changes: `src/client/player/input/KeyboardInput.cpp` and the
UWP-only creative forward check in `src/client/player/LocalPlayer.cpp`.  A
Visual Studio x64 UWP rebuild and a manual in-world check of W/S/A/D, jump,
auto-jump, and (if creative mode is exercised) forward sprint are still
required; no build or runtime launch was performed by this change.

### Original LevelRenderer terrain handoff

This pass wires the original `LevelRenderer` into the existing non-owning
Minecraft world handoff. `Minecraft::attachUwpWorld()` now binds the borrowed
`Level` to the real `LevelRenderer`; detach clears that binding before
`UwpCoreSession` saves and destroys the world. The renderer's native
`setLevel()`, `allChanged()`, dirty listeners, `updateDirtyChunks()`,
`Chunk::rebuild()`, sorting, and `render()`/`RenderList` submission therefore
operate on the same Level and LocalPlayer already used by the UWP tick path.
No second Level or player is created.

`UwpWorldRenderer` now receives the original LevelRenderer through the existing
client bridge. When `OriginalChunk` or `Auto` is enabled and the native pair is
ready, the frame callback submits original LevelRenderer batches through the
shared GLES2 compatibility shader. The old bounded UWP Chunk window is not
scheduled in that mode, and the fallback mesh is only redrawn if native
submission has not produced a batch yet. `UwpFallback` still uses the existing
UWP mesh path. `MainPage` remains the sole camera-matrix/EGL presenter and the
original path does not call `swapBuffers()`.

For UWP only, the original LevelRenderer VBO pool is capped at a compact
64-block render distance to avoid allocating the Win32-sized 1024-block pool;
the Win32 cap and behavior are unchanged. A bridge accessor exposes the real
LevelRenderer without leaking the original client layout into XAML code.

Static validation after this edit: touched source paths are present and
`git diff --check` reports no new whitespace errors. Visual Studio compilation
and runtime validation were not available in this environment. The next x64
UWP run must verify native terrain across adjacent chunks, movement re-sorting,
block-edit invalidation, switching to `UwpFallback`, and save/exit/re-entry
without stale VBO or Level pointers. If the first native frame is blank, keep
`Auto` enabled while checking the fallback transition; the native path falls
back until `updateDirtyChunks()` produces a non-empty RenderList.

Current ownership after this pass:

| State | World/player | Tick | Chunk scheduling/submission | Camera/EGL |
|---|---|---|---|---|
| Native backend ready | `UwpCoreSession` owns; Minecraft/LevelRenderer borrow | Original `Minecraft::tick()` | Original `LevelRenderer` + `Chunk` + `RenderList` | `MainPage`/`UwpWorldRenderer` and EGL |
| Native batch not ready or `UwpFallback` | `UwpCoreSession` owns | Original `Minecraft::tick()` | Existing bounded UWP fallback mesh | `MainPage`/UWP EGL |

Full `GameRenderer` ownership, sky/entities/particles, and the complete
original render loop remain intentionally deferred.

### Native terrain layer and UWP frame-time correction

The native UWP bridge now submits the original terrain layers in the same
order and state model as `GameRenderer`: opaque layer 0, alpha-tested layer 1
(leaves and crossed vegetation), then blended layer 2 (liquids).  The GLES2
compatibility shader receives the corresponding alpha-test, blend, cull, depth
write, and depth-mask state for each pass.  `LevelRenderer::hasRenderableGeometry`
now accepts a layer so an alpha-only or liquid-only chunk set is not mistaken
for an empty world.

The handoff is protected during the initial rebuild window:
`UwpWorldRenderer` draws the existing UWP mesh as a base while
`LevelRenderer::updateDirtyChunks()` still reports pending chunks, then
submits any already compiled native RenderLists over it.  Once the bounded
rebuild queue is empty, the original path becomes the sole terrain producer.
This prevents the first few completed VBOs from replacing the complete
fallback scene and leaving persistent holes while neighbouring chunks are
still being generated.

The native ring also now marks repositioned UWP chunks visible immediately.
The desktop path normally gets this state from `GameRenderer::cull()`; that
full frustum pass is intentionally not part of the UWP bridge yet.  Without
the UWP-specific visibility seed, `Chunk::setPos()` reset every moved chunk to
`visible=false`, so a valid VBO could be counted while `RenderList` submitted
nothing, producing the intermittent all-void frame.

This visibility fix is also important for material validation: before it, the
native path could report completed geometry while the UWP bridge was actually
showing its simplified fallback mesh.  That fallback intentionally omits the
special top-snow geometry and does not reproduce the native leaf alpha
material, so testing those materials was not testing the original TileRenderer
path at all.

While the native queue is pending, the fallback mesh is now allowed to
recenter around a moving player.  This keeps the temporary coverage valid when
the player crosses the 48-block fallback boundary instead of exposing the sky
while the native ring is rebuilding.

The UWP-only native chunk window is capped at 64 blocks (5 x 8 x 5 chunks,
200 chunk objects and 600 layer VBOs).  The previous 256-block cap caused
thousands of VBO submissions per frame after the original layer passes were
enabled and was the primary sustained-frame-time regression.  Win32 and other
platform caps are unchanged.

Files changed in this correction: `src/client/renderer/LevelRenderer.h`,
`src/client/renderer/LevelRenderer.cpp`, and
`src/platform/uwp/UwpWorldRenderer.cpp`.  Static checks completed with no new
whitespace errors; a Visual Studio x64 UWP build and runtime FPS/material
check are still required.  The manual check should compare steady FPS before
and after moving through snow/leaf terrain, confirm that leaves are
alpha-tested rather than solid slabs, confirm water remains translucent, and
then exercise block edits and save/exit/re-entry.  If 64 blocks is still too
expensive on the target GPU, the same platform cap can be lowered without
changing the original chunk scheduling algorithm or the Win32 build.

The follow-up frame-path correction keeps already compiled native batches
visible while `LevelRenderer::updateDirtyChunks()` is still draining.  The
bounded UWP mesh is drawn first during that transition, and the original
`RenderList` batches are then overlaid; this prevents a player crossing the
chunk ring from seeing a complete void while also allowing the native
TileRenderer's top-snow and alpha-tested foliage layers to appear before the
whole ring has finished rebuilding.  If the renderer discovers a new dirty
ring during its sort pass, the fallback is filled in during that same frame.
After the queue is empty, only the original native path is submitted again.

Fallback mesh rebuilds also retain the last non-empty mesh until a replacement
contains real geometry.  This covers the short interval in which the borrowed
Level reports no blocks while its chunk window is being attached, so a
recenter or world re-entry cannot clear the frame to an untextured void.

The terrain frame now also reasserts `GL_CCW` front faces and `GL_BACK` culling
at the fallback/native boundary, so a UI or transparent pass cannot leave a
different cull mode behind for the next terrain submission.

The bounded fallback box emitter now includes the original thin `topSnow` box
using its actual tile shape bounds and face-visibility checks.  Other special
shapes remain native-only; this keeps the fallback transition from replacing
every snow cap with a missing top surface.

The fallback GLES2 fragment shader now discards fully transparent atlas texels,
so leaf/foliage cutouts do not become opaque colored rectangles while the
native alpha-tested layer is rebuilding.

The native `Tesselator` path was also checked for the face assembly issue:
`Tesselator::vertex()` already expands each original GL_QUADS face into six
triangle vertices before the UWP VBO upload, so no second GLES2 conversion is
performed.  This avoids corrupting the original TileRenderer winding while
keeping the existing Win32 behavior unchanged.

Static validation after this correction: `git diff --check` reports no new
whitespace errors and the UWP project XML remains well formed.  Visual Studio
x64 compilation and runtime validation still need to be performed manually.
The next run should check that horizontal grass/top-snow faces and leaf alpha
are correct, that no void appears while moving across a chunk boundary, and
that the fallback switch and block-edit rebuild remain safe.

The follow-up Visual Studio error list exposed an include-order regression in
the UWP bridge: `render()` queried `LevelRenderer` methods before the original
header was included, leaving only a forward declaration visible.  The bridge
now includes the complete `LevelRenderer` class at the top of
`UwpWorldRenderer.cpp` through `MCPE_LEVELRENDERER_NO_GLES_INCLUDE`; this
preserves the direct GLES2 fallback path while still allowing the later native
renderer block to include the normal GLES compatibility macros.  The duplicate
late `LevelRenderer.h` include was removed.  `git diff --check` and UWP project
XML validation pass; Visual Studio compilation and runtime validation remain
manual because the shell environment has no MSBuild installation.

### GameRenderer handoff checkpoint

A local Git checkpoint was created before this handoff (`3d8b0d2`,
`checkpoint: stabilize UWP original client rendering`).

The next convergence step now routes the borrowed in-world client through the
original `Minecraft` → `GameRenderer` → `LevelRenderer` path.  The fixed-step
UWP callback still calls `Minecraft::update()` for simulation, while the
variable-rate frame callback calls the new original `Minecraft::renderFrame()`
entry so every presented frame receives a real `GameRenderer::render()` pass.
`MainPage` updates the original Minecraft viewport size from the current ANGLE
surface before those calls, so `GameRenderer` owns its original projection,
camera transform, clear/state setup, and calls to `LevelRenderer::cull()`,
`updateDirtyChunks()`, and the three terrain layers.  `MainPage` continues to
own only the frame clock, UWP HUD overlay, and final EGL present; it no longer
submits `UwpWorldRenderer` terrain when the original world handoff is active.
The old UWP terrain renderer remains available for the non-attached/fallback
diagnostic path and is not expanded.

This source change has passed `git diff --check` and UWP project XML parsing.
Visual Studio x64 compilation and runtime validation are still required.  The
first manual run should verify that entering a world produces terrain through
the original GameRenderer, that the UWP HUD remains visible without clearing
the world, and that movement, chunk rebuilds, save/exit/re-entry, and page
shutdown remain stable.  If the original pass is blank, inspect the first
GameRenderer/LevelRenderer call and ANGLE shader state before re-enabling the
fallback; do not add another renderer.

### Black-frame correction

The first GameRenderer handoff produced a solid black world frame because the
UWP HUD/font pass intentionally calls `shaderUnbind()` after each overlay
frame.  The original `GameRenderer::render()` assumes its fixed-function
compatibility program is still bound at frame entry; on GLES2/ANGLE that
assumption is false after the UWP overlay.  The UWP entry now calls
`shaderBind()` before original mouse/camera/matrix work, restoring the shared
program before `GameRenderer` reaches `LevelRenderer` and its RenderLists.
No UWP terrain algorithm was added or re-enabled by this correction.  The
change is source-checked only in this environment; a Visual Studio x64 build
and manual world-entry check are still required.

### Original GameRenderer texture-cache correction

The first in-world GameRenderer screenshot confirmed that geometry and the
Minecraft -> GameRenderer -> LevelRenderer call chain were active, but it also
provided a direct cache-miss symptom: `Gui::renderToolBar()` requested
`gui/gui.png` and `gui/gui_blocks.png`, `ItemInHandRenderer` requested
`mob/char.png`, and `LevelRenderer::renderClouds()` requested
`environment/clouds.png`. `AppPlatform_uwp::loadTexture()` only served decoded
entries from its synchronous cache. Before this correction the cache contained
only terrain plus the four Minecraft::init() images, so those later requests
returned `InvalidId` and left the previous terrain atlas bound. That explains
the terrain atlas in the hotbar, the wrong first-person hand, and the wrong
cloud texture without changing any original renderer algorithm.

`MainPage` now preloads the original client render set (GUI atlas/items/icons,
touch/menu atlases, clouds, sun/moon, player skin, sign and the existing init
images) before allowing Minecraft initialization. The readiness counter is now
data-driven (21 textures including terrain) rather than hard-coded to five.
Win32 source paths
are untouched; the original GameRenderer and LevelRenderer ownership remains
unchanged. The screenshot's near-black sky is not treated as a texture-cache
failure: `GameRenderer::setupClearColor()` derives it from the current world
time and brightness, so it must be rechecked in daylight after the texture
fix.

### Input-axis and translucent submission correction

The current report showed the original in-game client was active but W/A/S/D
were reversed.  The cause was isolated to the UWP-only branch in
`handheld/src/client/player/input/KeyboardInput.cpp`: it inverted `xa` and
`ya` after `Minecraft::tickInput()` had already delivered the original key
events.  The camera basis is established by `GameRenderer`; compensating for
it in `KeyboardInput` inverted the actual player motion.  That branch now uses
the original `KeyboardInput::tick()` signs.  Win32 was already using those
signs and remains unchanged.

The remaining water/transparent-layer issue had a separate, code-visible
cause.  `LevelRenderer::renderChunks()` sorted the shared chunk list
near-to-far for every layer, while `GameRenderer` disables depth writes for
layer 2 before drawing water/lava.  On UWP, the layer-2 submission range now
iterates far-to-near; opaque and alpha-tested layers retain the original order,
and Win32 is untouched.  This is only a submission-order correction; the
original TileRenderer geometry and LevelRenderer scheduling remain in use.

Static checks after this change: source patch applied cleanly.  Visual Studio
x64 compilation and runtime verification are still required manually because
this environment has no MSBuild/Visual Studio toolchain.  The next run should
verify W/A/S/D independently, then inspect overlapping water surfaces and
leaf/block boundaries in daylight.  If leaf errors remain after the axis and
water-order fix, capture a daylight frame with a nearby leaf canopy and water
edge before changing alpha-test or face-culling state.

### Chunk-ring stale VBO correction

The latest screenshots show a repeatable distant-ring occlusion failure: after
moving far enough to recenter the fixed chunk window, geometry from a previous
ring position can remain visible through nearer terrain until the replacement
chunk finishes rebuilding.  The UWP path stores absolute world coordinates in
each VBO, so reusing a `Chunk` object must invalidate its old draw metadata at
the same moment that `setPos()` resets the chunk.  `Chunk::reset()` now clears
every UWP `RenderChunk::vertexCount` and restores the current VBO slot before
the chunk is queued for rebuilding.  The VBO objects are retained and reused;
no original `TileRenderer`/`LevelRenderer` algorithm was replaced.

This is source-checked only in the current environment (`git diff --check` and
project XML validation); Visual Studio x64 compilation and runtime verification
remain manual.  The next run should cross a chunk boundary repeatedly and
confirm that old distant terrain no longer appears through a foreground wall.
If the symptom persists, capture a frame immediately after a ring recenter so
the next investigation can distinguish stale VBO submission from frustum or
culling state.

The same correction also hardens the original `GameRenderer::renderLevel()`
entry on UWP: depth testing and depth writes are explicitly restored after the
frame clear, before opaque terrain.  This keeps HUD, hand, and transparent
passes from leaking a disabled depth state into the next world frame.  The
original Minecraft -> GameRenderer -> LevelRenderer call chain is unchanged.

### Main-menu font source correction

The main menu is not built from XAML controls.  `MainPage` loads
`data/images/font/default8.png` into `UwpUiRenderer::m_fontTexture` and also
asynchronously builds a DirectWrite atlas from `data/fonts/minecraft.ttf`.
Before this correction, `drawText()` and `measureText()` unconditionally chose
the TTF atlas whenever `m_ttfReady` was true, so button labels such as
`Singleplayer` and `Options` were rendered from the TTF file rather than the
original MCPE bitmap font image.  The button backgrounds and title were already
image assets, but their labels were not.

`UwpUiRenderer` now prefers the uploaded `default8.png` atlas for the main-menu
and world-select screens whenever that texture is ready.  The TTF path remains
the fallback during asynchronous bitmap loading and remains the active path for
in-game overlay text, so this change is limited to the original menu-font
seam.  No Win32 code or world/GameRenderer path was changed.

Static checks after this change: `git diff --check` and UWP project XML parsing
are expected to pass; Visual Studio x64 compilation and the visual comparison
still require a manual run.  Verify that menu button labels have the expected
pixelated MCPE glyphs, that hover/selection still aligns, and that entering a
world leaves the existing HUD and original Minecraft -> GameRenderer ->
LevelRenderer chain unchanged.

### Original keyboard/mouse main-menu handoff

The UWP shell now exposes the original desktop `StartMenuScreen` through the
existing `Minecraft -> GameRenderer -> Screen` path after the original client
initializes.  `Button::renderBg()` therefore uses the original `gui/gui.png`
atlas and `Button::renderFace()` uses `Minecraft::font`, instead of drawing a
second UWP button/font implementation on top of it.  The original menu title,
background, and button atlases are synchronously cached before client
initialization; `gui/title.png` was added to that preload set because the
original start screen requests it lazily during its first render.

The UWP bridge feeds pointer press/move/release events into the original
`Mouse` queue and feeds the original menu Up/Down/Enter key codes into the
`Keyboard` queue.  The menu entry for Singleplayer is the only intentional
handoff: the original screen handles the click, while the UWP shell consumes a
small action signal and opens its existing asynchronous world list/session
flow.  Other menu buttons remain on their original `StartMenuScreen` paths.
When the original menu is shown, desktop keyboard/mouse mode is selected so
`Screen::keyPressed()` can run its original tab-navigation logic.  The custom
UWP menu is not composited while this screen is active, so there is only one
menu renderer and one font source in that state.  Returning from the UWP world
list creates a fresh original start screen; leaving a world stops the original
menu loop before the custom world list is displayed.

If the original Options screen returns through `ScreenChooser` to a plain
`StartMenuScreen`, the UWP bridge detects that specific screen on the next
client tick and wraps it with the UWP-aware original start screen again.  This
keeps the original Options screen intact while preserving the Singleplayer
handoff and avoids silently falling into the disabled UWP `selectLevel()` path.

Static validation for this handoff passes: `git diff --check` and UWP project
XML parsing are clean.  This environment has no Visual Studio/MSBuild, so an
x64 UWP build and runtime verification are still required manually.  The
manual check should confirm that the main menu shows the same pixel font and
button atlas as the in-world original screens, that mouse hover/click and
Up/Down/Enter navigation work, and that Singleplayer still reaches the UWP
world list without two update loops running.  The original Win32 source path
was not modified.

### Original menu textured-state correction

The first runtime check after the keyboard/mouse menu handoff exposed a
UWP-only state mismatch: the original menu's geometry was present, but the
background and GUI atlases appeared as flat gray/white rectangles and all
labels were invisible. The existing in-world original font remained visible.
The cause is in the GLES2 compatibility layer: `glBindTexture` changes the
bound texture but does not enable the fixed-function `GL_TEXTURE_2D` state;
on UWP that state controls the shader's `u_useTexture` uniform. The menu
screen enters after an untextured frame clear/fill, whereas the in-world HUD
already explicitly enables texture before its original-font pass.

The original code path is preserved. `Font::drawSlow()`, both original
`GuiComponent::blit()` overloads, `Screen::renderDirtBackground()`, and the
direct title quad now restore `GL_TEXTURE_2D` immediately before their
textured draws. `GameRenderer::render()` also establishes the textured,
blended, depth-disabled GUI state at the no-Level menu boundary. These are
small UWP-compatible fixed-function state repairs; no custom menu renderer,
TTF fallback, or Win32 behavior was changed.

Static validation after the correction: `git diff --check` is clean apart
from the repository's existing CRLF normalization warnings. The UWP project
XML remains parseable. This environment cannot invoke Visual Studio or run
the packaged executable, so an x64 rebuild and manual check are still
required: confirm the dirt background, title, pixel-font labels, hover state,
and Singleplayer handoff, then enter a world and confirm the existing original
tick/render path remains unchanged.

### Original SelectWorld screen and opaque RGBA texture upload

The original keyboard/mouse `SelectWorldScreen` is now the screen shown after
the original StartMenu Singleplayer button. Its real list, scrolling, button
input, and level-selection code run unchanged. `Minecraft::selectLevel()` is
intercepted only in the UWP original-client build: it publishes the selected
level id/name to the bridge, while `UwpCoreSession` remains the sole creator,
owner, saver, and destroyer of the actual `Level` and `LocalPlayer`. MainPage
consumes that request and reuses the existing asynchronous saved-world attach
path; no second world or tick loop is introduced. The custom UWP world list
remains available as a fallback path.

`gui/default_world.png` was added to the synchronous original-client texture
preload set so the real world carousel does not depend on a missing lazy
asset. The menu dirt background stripe artifact had a separate concrete cause:
UWP's decoder always caches RGBA8 pixels, but `Textures::assignTexture()` chose
`GL_RGB` for fully opaque images, making ANGLE read four-byte rows with a
three-byte stride. UWP uploads now use `GL_RGBA` for this RGBA8 contract;
other platforms retain the original format-selection logic.

The original-menu bridge also recreates the original StartMenuScreen if Escape
clears the screen directly while no world is attached, preventing a clear-color
blank menu frame without adding another renderer.

Static validation after this change: `git diff --check` and UWP project XML
parsing pass (with the repository's existing line-ending warnings). Visual
Studio x64 build and runtime verification still require a manual run. Verify
that the menu background is tiled dirt rather than stripes, Singleplayer opens
the original carousel, saved-world selection enters the existing original-tick
world path, and Back returns to the original StartMenu.

### SelectWorld UI-thread crash fix

The first manual attempt to enter the original world-selection screen crashed
during `SelectWorldScreen::init()`.  The concrete unsafe path was
`ExternalFileLevelStorageSource::getLevelList()` →
`UwpStoragePaths::enumerateWorldFolderNamesBlocking()` → WinRT
`GetFoldersAsync().get()` on the XAML STA.  UWP directory enumeration now uses
the synchronous Win32 `FindFirstFileW`/`FindNextFileW` API for the app-local
worlds directory.  The original `SelectWorldScreen`, `LevelStorageSource`, and
summary parsing remain unchanged; only the platform directory adapter avoids
blocking the UI dispatcher.  A manual x64 run is still required to confirm the
original list opens, displays saved worlds, and returns to the menu without an
exception.

### Legacy UWP menu/list removed from the normal client flow

The UWP shell no longer selects or renders its former `UwpUiRenderer` main-menu
or world-card screens.  `MainPage` enters the original `StartMenuScreen`, routes
Singleplayer to the original `SelectWorldScreen`, and returns to that original
menu after a world is closed.  The UWP renderer is now limited to the in-world
diagnostic/HUD seam; its old enum and drawing helpers remain only as inert
source compatibility for the current generated UWP project and are forced to
the `Game` screen if an old caller tries to select them.  The old XAML
`WorldsPage` is no longer navigated or used by the application.

The obsolete UWP-side `UwpCoreSession::listSavedWorlds()` adapter and the
fixed `startNewWorld("uwp-experimental-world")` entry point were removed.  World
enumeration is now exclusively the original `ExternalFileLevelStorageSource`
path used by `SelectWorldScreen`.

The storage-root mismatch was corrected at the same time.  `UwpCoreSession`
now passes the application-data roots (`LocalFolder` and `LocalCacheFolder`) to
the original `ExternalFileLevelStorageSource`; that original class appends
`games/com.mojang/minecraftWorlds` exactly once.  Consequently both the
original client host and the UWP session use:

`ApplicationData::Current->LocalFolder/games/com.mojang/minecraftWorlds`

for persistent worlds.  The cache-root counterpart is used only for the
original temporary-storage contract.  No folders were deleted or migrated;
worlds created by an older build under a duplicated experimental path may need
to be copied once into the canonical folder before they can appear in the
original list.

Static validation in this environment: `git diff --check` and UWP project XML
parsing pass.  Visual Studio x64 compilation and runtime verification are not
available here.  The required manual check is: launch the app, verify that no
legacy UWP menu/list appears, enter the original SelectWorld screen, confirm
that a world under the canonical folder is listed by its `level.dat` name,
open it, press Back to save, then reopen it after restarting the app.

### Canonical world-root compile-condition correction

The supplied save was valid: its `level.dat` starts with storage version 3,
an NBT payload, and contains `LevelName=world`; the four `chunks.r.*.dat`
files are also in the expected folder.  The failure was in the UWP project
configuration, not in the save layout.  `ExternalFileLevelStorageSource.cpp`
was being compiled with `STANDALONE_SERVER`, so its original constructor
deliberately left `basePath` at the `LocalState` root.  The UWP enumerator found
`LocalState/games/com.mojang/minecraftWorlds/world`, but summary parsing then
looked for `LocalState/world/level.dat`.

That translation unit is now compiled as the client storage source
(`WIN32;NO_NETWORK;MCPE_UWP`, without `STANDALONE_SERVER`), restoring the
original suffix construction.  The resulting `basePath` is the same canonical
directory used by `UwpStoragePaths` and `UwpCoreSession`.  Minimal diagnostics
now log the root, enumerated directory count, summary id/name, and any skipped
`level.dat`, which makes a package-path mismatch immediately visible.

Static validation after this correction: `git diff --check` and UWP project XML
parsing pass.  This environment has no Visual Studio/MSBuild or packaged UWP
runtime, so the x64 rebuild and manual SelectWorld verification remain
required.  Rebuild before testing; an old binary still contains the erroneous
`STANDALONE_SERVER` storage layout.

### Local world selection no longer enters the server-progress path

After the storage-root fix, the original SelectWorld list displayed the save,
but selecting it still followed the desktop sequence
`Minecraft::selectLevel()` → `hostMultiplayer()` → `ProgressScreen`.  That
sequence is for a network-hosted desktop world; on UWP the selection is already
published by `Minecraft::selectLevel()` to the bridge and MainPage starts the
local `UwpCoreSession::startSavedWorld()` load.  The old progress screen waited
for a server and eventually showed `Could not connect to server`.

Both keyboard/mouse and touch SelectWorld sources now keep the original
Win32 path unchanged, while the UWP branch skips `hostMultiplayer()` and
`ProgressScreen` after publishing the selection.  MainPage remains the only
owner of the asynchronous local load and then attaches the resulting Level and
LocalPlayer to the original client.  No networking behavior was enabled or
disabled globally.

Static validation: `git diff --check` and UWP project XML parsing pass.  A
Visual Studio x64 rebuild and runtime verification are still required here.
Manual checks: select `world`, confirm it enters the existing save without the
server-progress screen, move briefly, press Back to save, and reopen it from
the original SelectWorld screen.

### Original Options interaction and bitmap-font alpha correction

The current UWP original-client menu path keeps the original `OptionsScreen`
and `OptionsGroup` implementation; no second settings screen was introduced.
Two UWP-only presentation/input faults were corrected.  First, a sidebar
category was selected only after `Screen::mouseReleased()` confirmed the same
button object and coordinates.  A UWP pointer release can arrive with a
different logical coordinate after composition scaling or sidebar scrolling,
so the category row is now resolved from the original GUI-space sidebar
coordinate on the left-button press, while the existing `Button` objects and
keyboard tab navigation remain active.  This makes Game, Controls, Graphics,
Audio, Additional, and Skin reachable without changing the Win32 path.

Second, GUI fills/gradients can leave the GLES2 compatibility state with
alpha-test disabled.  The original `default8.png` font atlas has zero-alpha
background pixels; drawing it without the original alpha-test/blend contract
produces black rectangles behind glyphs.  UWP font draws now restore
`GL_TEXTURE_2D`, `GL_ALPHA_TEST` (`GL_GREATER, 0.1`), and standard alpha
blending, and the menu GUI setup establishes the same state before a screen is
rendered.  This changes no font asset or Win32 rendering code.

Changed files: `handheld/src/client/gui/Font.cpp`,
`handheld/src/client/gui/screens/OptionsScreen.cpp`, and
`handheld/src/client/renderer/GameRenderer.cpp`.

Static validation: `git diff --check` passes.  Visual Studio x64 build and
runtime are not available in this environment.  Manual verification required:
rebuild UWP, open Options, click every sidebar category (including after
scrolling), confirm glyphs have no black rectangles in the menu/options/world
screens, and verify Back still returns to the original menu.

The first post-fix run showed the categories flashing briefly and returning to
Game.  The cause was the expected per-frame `Minecraft::setSize()` call used
to follow the ANGLE surface: `OptionsScreen::setupPositions()` was resetting
the selected category to zero on every refresh.  It now reapplies the stored
`selectedCategory`, so layout updates no longer change the active pane.

### UWP main-menu skin entry

The skin editor is no longer exposed as a sixth first-level `OptionsScreen`
sidebar category.  The existing `SKIN_MENU` toggle remains in the Additional
pane, but now controls visibility of the main-menu entry instead.  The
original skin pane remains compiled and is opened directly from a new UWP-only
button on the lower-right of the original desktop
start menu.  The keyboard/mouse button uses the two vertically stacked square
skin states drawn on the right edge of
`handheld/data/images/gui/gui.png` (normal at `(237,67,19,18)`, hover at
`y=87` with a two-pixel source gap).  The touch controls continue to use their
existing `touchgui.png` artwork.  `ImageDef` now supports vertical second-state
strips with an optional source stride while preserving the existing horizontal
behavior for all other image buttons.

The project content copies of `gui.png` and `touchgui.png` were synchronized
with the edited source assets so a normal UWP build packages the new icon.
Win32 start-menu
code and the original Options skin-pane implementation were not changed.  The
button opens `OptionsScreen` and selects its existing skin pane; no duplicate
skin editor or new rendering path was introduced.

The keyboard/mouse skin icon is positioned relative to the original
`StartMenuScreen` quit button: it is four GUI pixels to the right and vertically
centered, matching the requested lower-row relationship rather than being
anchored to the screen corner.

The skin button's hit/render square is now two GUI pixels smaller than the
original quit button's 22-pixel height (`20x20`) and remains vertically
centered.  The updated `gui.png` contains complete `20x20` normal/hover
rectangles at `(236,66)` and `(236,86)`; the source rectangle now includes the
black frame on all four sides instead of cropping it away.

Static validation: `git diff --check` passes.  This environment does not have
the Visual Studio/MSBuild UWP toolchain or a packaged runtime, so rebuild and
manual verification are still required: confirm the icon appears in the
lower-right of the main menu, its hover state uses the lower texture, clicking
it opens the original skin controls, and the Options sidebar contains only
Game, Controls, Graphics, Audio, and Additional.

### UWP mouse bridge for the original in-world client

The UWP `AnglePanel` pointer bridge now forwards real mouse input to the
original `Mouse` device while the borrowed world is active.  Pointer movement
is queued as relative `ACTION_MOVE` deltas for the original
`Minecraft::tickInput()` → `MouseTurnInput` → `GameRenderer` camera path.
Left and right button press/release events are forwarded as
`ACTION_LEFT`/`ACTION_RIGHT`; the existing original `MouseBuildInput` therefore
owns block breaking and placement/interact without a second UWP action path.
Touch pointers remain excluded from this in-world mouse bridge.  Pointer
capture is released on button release so the original menu and pause screens
can continue to process normal screen-space clicks.

The UWP world handoff now also invokes the original `Minecraft::grabMouse()`
state when no original screen transition performed it.  This is required by
the existing `GameRenderer` contract: it polls `MouseTurnInput` only while
`mouseGrabbed` is true in keyboard/mouse mode.  The UWP `MouseHandler::grab()`
implementation remains platform-neutral and does not add a second cursor or
EGL presenter.

Because UWP simulation ticks are fixed-step while presentation can run at a
higher display rate, the UWP render callback calls the existing
`Mouse::reset2()` after the original frame render.  The UWP branch skips the
otherwise update-owned reset in `NinecraftApp::update()`, so a pointer delta
arriving between a frame and the next fixed tick is not discarded.  Together
these changes consume each relative delta once per presented frame instead of
reapplying it or losing it at the tick boundary.

Changed files: `handheld/project/uwp/MCPEUWP/MCPEUWP/MainPage.xaml.cpp`,
`handheld/src/client/Minecraft.cpp`, and
`handheld/src/NinecraftApp.cpp`.

Static validation: `git diff --check` passes.  Visual Studio x64 build and
runtime are not available in this environment.  Manual verification required:
enter a saved world in keyboard/mouse mode, move the mouse in all four
directions and confirm the original crosshair/camera turns, hold/release left
click to break a block, right click to place/use, and verify the original
Esc/PauseScreen path exits the world.  Also verify touch mode remains on its existing
touch path and that no button remains stuck after focus/capture changes.

### Original in-world cursor capture and pause ownership

The keyboard/mouse world path now follows the original client's
`Minecraft::mouseGrabbed`/`screen` state.  While the original world view is
active, `MainPage` sets the UWP `CoreWindow::PointerCursor` to `nullptr`; the
pointer is therefore not visible while the original `GameRenderer` crosshair
remains fixed at the viewport center.  Pointer movement arrives through the
UWP `MouseDevice::MouseMoved` relative-delta event and is forwarded to the
original `Mouse` queue; no absolute pointer warp or second camera path is
used.  The screen-space pointer origin is discarded
whenever cursor ownership changes, preventing a pause/menu-to-world camera
jump.

Pressing Esc is now exclusively handled by the original path:
`CoreWindow::KeyDown` → `Keyboard::feed(KEY_ESCAPE)` →
`Minecraft::tickInput()` → `Minecraft::pauseGame(false)` → original
`PauseScreen`.  When that screen is active, the cursor is restored and the UWP
diagnostic HUD is suppressed so the original pause controls own the frame and
mouse input; any outstanding XAML pointer capture is released at that
transition.  Continuing calls the original `setScreen(NULL)`/`grabMouse()`;
the cursor is hidden again on the next frame.  The former lower-left UWP Game
Back button is no longer drawn or hit-tested; its enum remains only for source
compatibility with older callers.  Win32 pause and rendering code is unchanged.

Changed files: `handheld/project/uwp/MCPEUWP/MCPEUWP/MainPage.xaml.cpp`,
`handheld/src/client/Minecraft.cpp`,
`handheld/src/platform/uwp/UwpOriginalClientBridge.cpp`,
`handheld/src/platform/uwp/UwpOriginalClientBridge.h`,
`handheld/src/platform/uwp/UwpUiRenderer.cpp`,
`handheld/src/platform/uwp/UwpUiRenderer.h`, and `UWP_PORT_PROGRESS.md`.

The original pause quit path also now has an explicit ownership handoff:
`PauseScreen::buttonClicked()` still calls the original `Minecraft::leaveGame`,
but the UWP borrowed-world branch only detaches the original pointers and
publishes a `LeaveWorld` bridge action.  MainPage then clears renderer
references, lets `UwpCoreSession::stop()` save/destroy the sole owned world,
and returns to the original start menu.  This prevents the desktop delete path
from freeing a UWP-owned `Level` twice.

Static validation: `git diff --check` reports no whitespace errors; the UWP
project XML remains parseable; the `MainPage.xaml.cpp` brace count is balanced;
no runtime build is available in this environment.  Manual verification
required after rebuilding: enter a world, confirm the OS cursor disappears and
the crosshair stays centered while turning; press Esc and confirm the original
PauseScreen appears with a visible cursor; click Continue and confirm the
world resumes with the cursor hidden; verify no lower-left temporary Back
button is present; use the original pause screen to leave the world and reopen
the save without a stuck mouse button or duplicate world/tick owner.

### PauseScreen local-world network guard

The original pause screen no longer treats every `RakNetInstance` reporting
`isServer()` as a hosted server. UWP local worlds may retain the RakNet
instance while intentionally leaving `Minecraft::netCallback` null. The
server-visibility control and `updateServerVisibilityText()` now require a
real callback before casting to `ServerSideNetworkHandler`; local worlds use
the normal quit-to-title path instead of dereferencing a null callback.

Static validation: `PauseScreen.cpp` braces are balanced and the null guard is
present at both the button construction and visibility-update call sites.

### UWP relative mouse input and local pause actions

The UWP mouse bridge now subscribes to `MouseDevice::MouseMoved` and forwards
its relative `MouseDelta` values directly to the original `Mouse` queue while
the world is grabbed. This avoids both the client-area edge limit and the
large synthetic pitch deltas caused by warping an absolute pointer. Absolute
`PointerMoved` input remains reserved for the original menu and pause screens.

`PauseScreen` now distinguishes a real hosted/network client from the UWP
local-world RakNet placeholder. The original “Quit and copy map” action is no
longer shown for that local-world state; the button remains available for the
original non-server client path where its semantics apply.

Static validation: `MainPage.xaml.cpp` and `PauseScreen.cpp` braces are
balanced, the UWP project XML remains parseable, and no invalid
`PointerPoint::Pointer` access remains. Runtime verification still requires an
x64 UWP rebuild: turn continuously past 360 degrees, test vertical ±90 degree
clamping, press Esc, and confirm the local pause screen has no copy-map action.

### UWP audio backend restored through original SoundEngine

The UWP client no longer selects the repository's empty `SoundSystem` base.
The original `SoundEngine` and its embedded PCM descriptors remain unchanged;
only the UWP platform selection now uses `SoundSystemUwp`. This backend creates
the system XAudio2 engine and mastering voice, submits the original PCM
`SoundDesc` buffers as short-lived source voices, applies the original volume
and pitch values, and reclaims completed voices. Disabling sound stops active
voices, matching the existing `Minecraft::audioEngineOff()` boundary.

The desktop OpenAL path was deliberately not reused: the repository contains
desktop OpenAL binaries but no UWP-compatible OpenAL package, and the UWP
project previously selected a no-op base specifically to avoid loading that
runtime. The UWP project now links the Windows SDK `xaudio2.lib` and compiles
`platform/audio/SoundSystemUwp.cpp`; Win32/OpenAL selection is unchanged.

This restores PCM sound effects through the original call chain, but does not
yet add music streaming or X3DAudio positional panning. Static validation is
complete (project XML parses, the new source/header paths exist, and
`git diff --check` reports no new whitespace errors). A Visual Studio x64 UWP
rebuild and runtime test are still required to confirm `XAudio2Create`, step,
button, block and damage sounds, option volume, pause/leave, and device-loss
behavior.

### Original RakNet multiplayer path restored (controlled UWP handoff)

The UWP client project now includes the original `ClientSideNetworkHandler`,
`ServerSideNetworkHandler`, `RakNetInstance`, `CommandServer`, `ServerPlayer`,
`NetEventCallback`, and the original RakNet transport/serialization closure.
`Minecraft.cpp` and `NinecraftApp.cpp` are no longer compiled with
`NO_NETWORK`, and the package declares the inbound LAN capabilities required
by RakNet host mode. `ws2_32.lib` is linked for the original Win32 socket
transport; Win32 sources and their build definitions are unchanged.

The local UWP world is still owned by `UwpCoreSession`. After the original
client borrows that Level/LocalPlayer, the bridge starts the original
`Minecraft::hostMultiplayer(19132)` path and explicitly delivers the original
`ServerSideNetworkHandler::levelGenerated()` callback for that same Level.
No second world or player is created. `PauseScreen` can therefore show and
toggle the original server visibility button. Borrowed-world teardown now
disconnects RakNet and destroys the network callback before the UWP owner
destroys the Level, preventing a stale listener.

The original client join path is also compiled and its normal
`ClientSideNetworkHandler -> MultiPlayerLevel -> Minecraft::setLevel()` path
is no longer rejected by the UWP lifecycle guard. MainPage switches input
ownership to the original client once a remote level has completed generation;
the UWP local-world owner remains inactive during that path.

Static validation: the UWP project and app manifest parse successfully, the
RakNet source glob excludes only the two non-Win32 helper files, and the
network callback/ownership paths have balanced source structure. A Visual
Studio x64 UWP build and runtime verification are still required: enter a
local world, confirm PauseScreen shows the visibility button, toggle it,
connect a second MCPE client on the LAN, test JoinGame discovery/login/chunk
transfer, then leave and re-enter the world to confirm the callback and socket
are released cleanly. If the UWP linker rejects a RakNet source that is not
permitted in AppContainer, keep the original local-world path and report that
specific source/API rather than adding a replacement transport.

Build follow-up: a complete x64 Debug rebuild reached compilation and linking
of the network closure and produced
`handheld/project/uwp/x64/Debug/MCPEUWP/MCPEUWP.exe`.  The executable imports
`WS2_32.dll` and contains the original network error/UI strings, confirming
that this is not a file-list-only change.  A later incremental/rebuild attempt
was stopped by `C1902` PDB/PCH mismatch errors in the existing VS toolchain;
the generated executable from the successful link is retained.  This is an
intermediate-file/toolset cache issue, not a source or linker error from the
RakNet units.  Runtime LAN discovery, joining, and the pause visibility toggle
still require manual launch on the target UWP machine.

Build diagnostic follow-up: `UwpOriginalClientBridge.cpp` now includes the
original `network/RakNetInstance.h` definition before calling
`IRakNetInstance::isServer()`.  This removes the reported C2027 incomplete-type
error; the remaining local rebuild failure is the existing C1902 PDB/PCH
toolchain-cache mismatch (with `CompileXaml/MSB4181` as its secondary symptom).
The x64 project also uses `/FS` for its Debug/Release compiler databases so
parallel original-client translation units do not mask source diagnostics.

Build diagnostic follow-up (x64 UWP clean rebuild): the RakNet POSIX-name
diagnostics are now suppressed through the UWP target's
`_CRT_NONSTDC_NO_DEPRECATE` definition, and the old `#pragma warning` values
were corrected to warning 4996.  `gettimeofday.cpp` uses the UWP CRT's real
`_get_timezone`/`_get_daylight` accessors instead of the desktop-only
`_tzset`, while the desktop implementation remains unchanged.  The original
`CThread.cpp` is now part of the UWP dependency closure; its UWP destructor
closes the worker handle because `TerminateThread` is not exposed in UWP.

The x64 Debug compiler uses `/Z7` and does not request a link-time PDB because
the installed VS toolchain reports an `MSPDB140.DLL` version mismatch for its
PDB manager.  With packaging disabled for diagnosis (`AppxPackage=false;
AppxBundle=false`), the project now compiles and links successfully and emits
`handheld/project/uwp/MCPEUWP/MCPEUWP/x64/Debug/MCPEUWP/MCPEUWP.exe`.
A normal package build reaches the same successful link, then stops at the
machine's APPX0501 COM-registration error; this is an SDK/tool installation
problem after compilation, not a remaining C++ or linker error.  The generated
executable is available for local launch validation.

Verification: a clean x64 Debug `Rebuild` with packaging disabled completed
successfully after recompiling the full client, renderer, world, audio and
RakNet closure.  The remaining output is warning-only (encoding and legacy
type-conversion warnings).

Deployment/runtime diagnostic: the subsequent x64 build also compiled and
linked successfully, but packaging still stopped at `APPX0501` while resolving
the system Appx Packaging COM class.  The staged `AppX` executable is therefore
older than the newly linked intermediate executable.  Launching either that
stale staged copy or the nested intermediate `MCPEUWP.exe` without completing
UWP deployment can fail during `App::InitializeComponent()`/`MainPage` XAML
resource loading with `Platform::COMException` `0x802B000A`.  This is a package
identity/resource deployment failure, not evidence of a new C++ startup fault;
the current build must be deployed through Visual Studio or an up-to-date
signed AppX package before runtime validation.

## Startup and local-world loading transition surfaces (2026-10-21)

The UWP host no longer exposes the ANGLE clear color while the original client
is waiting for its first menu screen.  `UwpUiRenderer::renderLoadingBackground`
draws only the packaged MCPE dirt texture (with a dark-brown fallback before
the texture upload completes); it is a transition surface and does not own a
menu or world renderer.  The package visual background is also an opaque white
value matching the iOS Mojang splash artwork, so the OS-to-Page handoff does
not expose a transparent-splash system blue.

Saved-world loading now keeps the original client frame loop alive and shows
the original `ProgressScreen`.  `UwpCoreSession` remains the sole owner of the
asynchronously created Level and LocalPlayer, and the screen is removed by the
existing `attachUwpWorld()` handoff once those objects are ready.  The menu
fallback is suppressed while `m_worldCreationPending` is true, preventing the
progress screen from being replaced by the start menu during the load.

Verification: x64 Debug compile/link with packaging disabled succeeded after
these changes.  Runtime verification still requires deploying the new AppX
through Visual Studio; the machine-level APPX0501 packaging issue is unchanged.

## UWP identity assets (2026-10-21)

The UWP project now uses the MCPE 0.10 Windows Phone artwork for its application
identity assets instead of the placeholder gray squares.  The Square150,
Square44, StoreLogo, Wide310x150, LockScreen, and Square71 variants were rebuilt
from the reference package and `Package.appxmanifest` now declares the Square71
tile.  The splash asset uses the transparent iOS `mojang_logo.png`, centered on
the UWP scale-200 splash canvas.  A clean x64 Debug rebuild with packaging
disabled compiled and linked successfully after these changes.  Runtime icon,
tile, and splash verification still requires deploying the package through
Visual Studio; the existing machine-level APPX0501 packaging error remains
 separate from this asset change.

## Original-screen text input bridge (2026-10-21)

The UWP page previously forwarded `CoreWindow::KeyDown` and `KeyUp`, which was
enough for movement and shortcut keys but did not produce character input for
the original client text boxes.  The Win32 client receives a separate
`WM_CHAR`/Unicode-character path; without its UWP equivalent,
`Keyboard::_inputText` stayed empty and `Screen::updateEvents()` could never
deliver text to `TextBox::charPressed()`.

`MainPage` now registers and unregisters `CoreWindow::CharacterReceived` with
the same lifetime as the existing keyboard handlers.  Printable ASCII
characters are passed through `Keyboard::feedText()`, then consumed by the
original `Minecraft::tickInput()` -> `Screen::updateEvents()` -> `TextBox`
chain.  This keeps the original username, new-world name, and seed controls
as the sole text-input owner; no parallel UWP text widget or duplicate
character path was added.  The bridge intentionally follows the current
byte-oriented original `TextBox` contract, so non-ASCII/IME composition is
not part of this fix.

Verification: the x64 Debug UWP project compiled and linked successfully with
packaging disabled (`AppxPackage=false`, `AppxBundle=false`) and reported zero
build errors.  Deploy the resulting package through Visual Studio, then
manually verify focus, letters, digits, spaces, Backspace, and Enter in the
username, world-name, and seed fields.  The machine's existing `APPX0501`
packaging problem is unchanged and must be resolved by deployment before
runtime validation.

## World-creation settings and UWP world deletion (2026-10-21)

The original `SimpleChooseLevelScreen` was already parsing the world name,
seed, game mode, and world type.  The UWP interception in
`Minecraft::selectLevel()` discarded the `LevelSettings` object, while
`UwpCoreSession` constructed every new world with a fixed seed and
`Survival/Infinite`.  The selection bridge now carries all three settings
values to `MainPage`, which distinguishes a new-world request from a saved
world (`LevelSettings::None()`).  New worlds are created through the existing
`CreatorLevel` path using the exact original seed, Creative/Survival mode, and
Old/Infinite/Sky world type; saved worlds still load their `level.dat` values.
The desktop `hostMultiplayer()`/server-progress path is skipped for UWP after
a new-world selection so it cannot overwrite or consume the request.

The original `DeleteWorldScreen` already called
`LevelStorageSource::deleteLevel()`, but the UWP implementation only logged
that deletion was unsupported.  It now recursively removes the selected
world directory inside the app-local `minecraftWorlds` root using the
UWP-permitted Win32 file APIs, clears read-only attributes, and logs success or
the failing path/error.  No alternate world-list or deletion manager was
added; the original confirmation screen and storage source remain in control.

Verification: the x64 Debug UWP build with packaging disabled completed with
zero errors (warnings are the existing encoding/conversion warnings).  Manual
deployment is still required to verify: create two worlds with different
seeds/modes/types and compare terrain/`level.dat`, reopen a saved world, then
delete one from the original confirmation screen and confirm it disappears
after returning to Select World.  The existing machine-level AppX packaging
issue remains separate from this source fix.

## UWP mouse wheel and creative sprint input (2026-10-21)

`SwapChainPanel` did not previously forward `PointerWheelChanged` into the
original `MouseAction` queue. UWP now converts each mouse-wheel notch to the
same signed `ACTION_WHEEL` event used by Win32, so the original screen lists,
options panes, crafting panes, and in-world hotbar selection all receive the
event through their existing handlers.

Creative sprint had two input-side gaps: Control was not mapped into the
original keyboard state table, and the UWP-only sprint test treated the
forward axis as negative even though `KeyboardInput` maps W to positive
`input->ya`. Both paths now use the original key state and forward-axis
semantics, restoring Ctrl-held and double-tap-W sprint without changing the
movement vector itself.

Verification: x64 Debug UWP compilation with packaging disabled completed with
zero errors. Runtime verification still requires deploying the package and
checking wheel scrolling on Select World/Options and creative sprint with
both double-tap W and W+Ctrl.

## Chunk population and shutdown-save ordering (2026-10-21)

The UWP `ChunkCache::postProcess()` was already using the current Win32-safe
ordering: the generator runs first, `clearUpdateMap()` clears the transient
update bits and `unsaved`, then `terrainPopulated` is set and
`markUnsaved()` requeues the completed terrain.  The regression was in the
UWP ownership path instead: `UwpMinecraftCore::saveGame()` previously called
only `Level::saveGame()`, which writes level/player data but does not flush the
resident chunk source before `Level` and `LevelStorage` are destroyed.

`ChunkCache::saveAll()` now drains the player-centered active population area
(a five-by-five chunk neighbourhood) whose base terrain is complete but whose
`terrainPopulated` flag is still false, including resident chunks discovered
while a feature crosses a chunk boundary.  The renderer may keep a much larger
resident ring; incomplete outer base-terrain chunks are deliberately not
serialized because the legacy format restores a loaded chunk as populated and
would otherwise suppress its later tree/snow/plant pass.  Each completed chunk
is marked unsaved after `clearUpdateMap()`, then the existing storage
`saveAll(true)` and pending-unsaved queue are allowed to write it.  The UWP
save chain is now:

`UwpCoreSession::stop()` / suspension save →
`UwpMinecraftCore::saveGame()` →
`ChunkSource::saveAll(true)` (population drain + dirty chunks) →
`Level::saveGame()` (level/player metadata) →
destroy `Level` and `LevelStorage`.

The normal leave-world path remains the original client’s screen/tick path;
UwpCoreSession is still the sole owner and performs the final flush after the
borrowed pointers are detached.  Application suspension now uses the same
non-destructive flush without leaving the world.  Diagnostic lines record
`postProcess begin/end` with `terrainPopulated`/`unsaved`, the initial and
remaining population counts, and the `saveAll` candidate count, so the
expected sequence is visible as `postProcess end ... unsaved=1` followed by
`ChunkCache saveAll`.  The storage writer also logs each actual
`ExternalFileLevelStorage save chunk: x,z` operation with its final population
and dirty flags.

Verification: x64 Debug UWP compilation/link succeeded with packaging
disabled, zero errors (only the existing encoding/conversion and APPX1503
warnings).  Runtime validation still needs a deployed package: create or
enter a world, move into a newly generated chunk, leave immediately, reload
it several times, and confirm trees/leaves/snow remain.  Also repeat through
application suspension and inspect the diagnostic sequence for
`remaining=0` in the player-centered population area before the chunk save.

### UWP incomplete-chunk queue protection (2026-10-21)

The first shutdown-save implementation exposed a second timing path.  The
UWP `ExternalFileLevelStorage::tick()` queue was still accepting any dirty
resident chunk, including a neighbouring chunk touched by a tree or snow
feature before that chunk's own `postProcess()` pass.  In addition,
`savePendingUnsavedChunks(-1)` could serialize an already queued incomplete
chunk during shutdown.  Both paths could write base terrain, after which the
legacy loader restored it as `terrainPopulated=true` and permanently skipped
the missing feature pass.

The UWP queue now accepts only `terrainPopulated` chunks.  A queued incomplete
chunk is deferred and kept in the queue until its population pass completes;
the Win32 queue path is unchanged.  The shutdown candidate filter and the
player-centred population drain therefore share one invariant: no incomplete
base terrain is serialized.  A diagnostic line reports the number of deferred
entries so a correct exit sequence is visible as population completion,
`unsaved=1`, and then a populated chunk save.

The previous full-render-ring drain was also narrowed when no player is yet
registered: it uses the origin five-by-five window rather than synchronously
processing every renderer-resident chunk.  This avoids the early-load/leave
UI stall while retaining the active player's five-by-five shutdown guarantee.

The x64 UWP Debug build still completes with zero errors and only the existing
encoding/conversion and packaging warnings.  Runtime verification is required:
leave immediately after entering a newly generated snowy area, restart and
re-enter it, and check that exit returns without a long stall and that snow,
leaves, and trees persist.  If it still fails, capture the lines around
`finishPostProcess`, `deferred`, and `ExternalFileLevelStorage save chunk`.

### Local movement authority and optional LAN hosting (2026-10-22)

The UWP local-world handoff no longer calls `hostMultiplayer()` automatically.
The borrowed `Level` and `LocalPlayer` therefore remain offline and the
original `LocalPlayer::tick()` does not emit movement packets while the user is
playing alone. This removes a client/server feedback path that could apply a
delayed `ServerSideNetworkHandler::handle(MovePlayerPacket)` interpolation to
the same local player and feel like a pull in the opposite direction during
continuous movement.

The original pause-screen server control remains available as `Open to LAN`.
It lazily starts the existing RakNet server and binds the already-held Level;
no second world or player is created. The UWP server handler still broadcasts
the host player's movement to remote clients, but does not feed that packet
back through the host player's three-tick interpolation path. The Win32 server
path keeps its original behavior.

Verification: x64 UWP Debug compilation/link succeeded with packaging disabled,
zero errors. Runtime verification still needs deployment: hold W/A/S/D for
several seconds in an offline local world, then repeat after explicitly using
`Open to LAN`; confirm there is no reverse pull, and verify a remote client
still receives host movement when LAN hosting is enabled.

### Original chunk-window scheduling restored (2026-10-22)

The active UWP render path now follows the Win32 `LevelRenderer` chunk-window
rules again.  The UWP-only 64-block distance cap was removed; the original
`Options::viewDistance`/`DIST_TABLE` values now determine the resident ring,
with the same 1024-block ceiling and VBO pool capacity as the desktop client.
The camera-forward ring bias was removed, so `resortChunks()` recenters only
from the player's world position.  Visibility remains owned by the normal
`GameRenderer::cull()` pass instead of being forced during ring relocation.

To keep simulation and rendering aligned at large view distances, the original
`Minecraft::prepareLevel()` preload path is now also used for the
externally-owned UWP Level before it is attached to the client.  This restores
the Win32 startup sequence (chunk reads/generation, initial lighting cleanup,
storage preload release, and population preparation) instead of making the
first visible render generate the world on demand.

After startup, the original
`LevelRenderer::resortChunks()` entry now performs a small UWP-only residency
prefetch around the player.  It requests the missing 3x3 neighbourhood through
the real `Level::getChunk()`/`ChunkCache` path before the next original tick;
this is not a second generator or a replacement scheduler.  The purpose is to
prevent `Level::hasChunksAt()` from temporarily treating a newly visible
collision chunk as absent, which was the direct trigger for the movement
pull-back observed with `far` view distance.

The UWP-specific GLES/VBO invalidation, water-layer ordering, and chunk
population/save sequencing remain in place because they are platform safety
and persistence fixes rather than alternate chunk-loading logic.  x64 UWP
Debug compilation/link succeeds with zero errors (existing warnings only).
Runtime verification still needs deployment: move across several chunk
boundaries in straight and turning directions, confirm distant terrain fills
without a moving void ring, then leave and reload to verify the save path is
unchanged.

### Resident-edge population flush correction (2026-10-22)

The previous shutdown fix still limited `ChunkCache::finishPostProcess()` to
the player's five-by-five chunk neighbourhood.  That left renderer-resident
chunks at the view-distance edge in the base-terrain state, so a forced exit
could omit their delayed trees, leaves, flowers, or snow.  Because the legacy
chunk format restores every serialized chunk as `terrainPopulated`, the missing
features could not be regenerated on the next load.

The save-time population drain now enumerates every chunk already resident in
the unique `ChunkCache` as a stable save-time snapshot.  It does not request
unrelated coordinates while collecting, and a feature that touches another
already-resident chunk is still covered by that snapshot.  Chunks created only
as an out-of-ring support side effect are given one bounded follow-up pass when
they were dirtied by a feature, which preserves cross-boundary leaves and snow
without allowing the save to grow without bound.  Any further incomplete base
chunk remains excluded by the old-format protection.
The existing ordering remains unchanged: generator `postProcess()` first,
`clearUpdateMap()`, then `terrainPopulated = true` and `markUnsaved()`, followed
by the populated-only candidate filter and the pending-unsaved queue.  Thus no
base-only edge chunk is serialized.

The x64 UWP Debug build succeeds with zero errors (37 existing warnings,
including encoding/conversion, linker incremental, and APPX manifest warnings).
Runtime verification still needs deployment: use `far` view distance, travel
until a fresh ring of chunks is visible, exit immediately, reload, and inspect
the distant ring for snow and tree/leaf blocks.  Also repeat with `tiny` as a
control and check the `finishPostProcess` log for `remaining=0` and a finite
`support` count before the `ExternalFileLevelStorage save chunk` lines.  A
nonzero `newIncomplete` count identifies a newly generated outer support ring
that was intentionally not serialized as base-only terrain.

### Far-view scheduling and generated-chunk persistence correction (2026-10-22)

The UWP-only synchronous 3x3 `Level::getChunk()` prefetch in
`LevelRenderer::resortChunks()` has now been removed.  At `far` distance that
prefetch ran inside render-ring relocation, made camera movement wait on world
generation, and could re-enter collision/loading timing that felt like the
player was being pulled backward.  `resortChunks()` now uses the same ring
repositioning schedule as Win32; the real `LevelRenderer` remains responsible
for the render ring and `Chunk` rebuilds.

The save path now drains resident population to a bounded fixed point (up to
eight support rounds), rather than only one neighboring pass.  In addition,
an explicitly forced save includes a populated chunk that was generated during
the current session even if its `unsaved` bit has already moved into the delayed
storage queue.  `ExternalFileLevelStorage::save()` marks that generated chunk
as persisted only after `RegionFile::writeChunk()` succeeds.  Region files and
the entities file are explicitly flushed after the full save.

The x64 UWP Debug build succeeds with zero errors (37 existing warnings).
Runtime verification still requires deployment: test a newly generated world
at `far` distance, travel across at least one fresh outer ring, exit normally,
reload, and inspect edge snow/tree/leaf blocks.  Existing worlds that were
already serialized with missing features cannot be repaired by the old chunk
format's `terrainPopulated` flag; use a newly generated area for this check.

### Startup and save transition surfaces (2026-10-22)

The short interval between ANGLE initialization and the original start menu no
longer exposes the platform clear colour.  The UWP shell now loads the bundled
`app/ios/icons/mojang_logo.png` into the packaged GUI assets and presents a
white Mojang-logo surface with a `Loading...` caption until the original
Minecraft client has initialized and created its first Screen.  If client
initialization is deferred, the existing dirt transition remains the fallback.

Saving keeps the same single MainPage/ANGLE presentation owner.  Before the
synchronous `UwpMinecraftCore::saveGame()`/world-stop path, MainPage paints a
dirt background with `Saving world...` and presents one frame; the save then
drains and flushes the existing original chunk/storage path.  No new world
renderer, client loop, or second EGL swap owner was introduced.  The new logo
is included by the existing `data\\images\\**\\*` deployment rule.

The x64 UWP Debug build succeeds with zero errors (three existing warnings in
this build, including source encoding, linker incremental, and APPX manifest
warnings).  Runtime verification remains: cold-start visibility through the
original main menu, normal world exit, suspension save, and reload after a
fresh far-view chunk ring.

### Startup logo geometry and dark dirt tint alignment (2026-10-22)

The custom pre-menu startup surface now uses the same logical logo-canvas
scale, with the Mojang logo raised to match the native cold-start splash.  The transparent
900x600 Mojang asset is scaled as a whole, preserving the native margin around
the visible wordmark; `Loading...` remains below it.

The UWP tiled dirt transition now uses the original `Screen::renderDirtBackground`
`0x404040` tint (0.25 per RGB channel).  This applies to both the ordinary
loading fallback and `Saving world...`, removing the brighter save-screen
variant while leaving the original Minecraft menu/progress rendering path
unchanged.

### Original item/entity/mob simulation enabled (2026-10-22)

The UWP-owned `CreatorLevel` no longer forces `Level::isClientSide = true`.
Local worlds now keep the original single-player authoritative Level semantics
(`false`, which is also the `Level` constructor default).  This re-enables the
existing original paths without a second UWP simulation layer:

- `Level::tick()` can run `MobSpawner::tick()` when `spawnMobs` and difficulty
  allow it;
- server-side item drops, projectile entities, falling tiles and block/entity
  callbacks are no longer suppressed by the client-side guards;
- `Level::tickEntities()` updates the single entity list, while
  `LevelRenderer::renderEntities()` and `EntityRenderDispatcher` submit the
  original entity/item/mob renderers;
- `ExternalFileLevelStorage::loadEntities()` / `saveEntities()` continue to
  use the existing `entities.dat` path for persistent entities.

The local UWP world still has no remote `NetEventCallback`, so
`LocalPlayer::tick()` does not send online movement packets.  No new entity
manager or custom mob-spawn algorithm was added.  The x64 UWP Debug build
succeeds with zero errors; runtime verification remains: create a survival
world, wait through day/night for passive/hostile spawns, break a block in
survival to produce a dropped item, pick it up, and reload the world to check
entity persistence.

### Original mob textures and creative inventory initialization (2026-10-22)

The original entity renderers were already active, and the packaged
`data/images/mob/*.png` files were present, but the UWP
`AppPlatform_uwp::loadTexture()` contract is deliberately cache-only and does
not perform a later synchronous file read.  Only `mob/char.png` had been
included in the startup decode list.  When a cow, pig, sheep, or other
renderer requested its skin, the cache miss left the previously bound terrain
texture active, producing block-textured entities.  The startup list now
includes the original chicken, cow, creeper, pig, pig-zombie, sheep,
sheep-fur, skeleton, spider, and zombie textures.  This changes no renderer or
texture coordinates; it only makes the original assets available through the
existing UWP adapter.

Creative worlds also constructed `LocalPlayer` with a hard-coded survival
inventory flag.  The player is now created from the loaded `LevelData` game
type, so the existing original `Inventory::setupDefault()` path populates the
complete creative catalogue and links the normal creative hotbar.  Survival
worlds continue to initialize with an empty inventory.  No separate UWP
inventory or entity renderer was added.

The x64 UWP Debug build succeeds with zero errors (10 existing warnings).
Runtime verification still requires deployment: spawn or load several mob types
and confirm their individual skins, then create a new Creative world and open
the original inventory/selection screen to verify the full catalogue and
hotbar.  If a particular mob remains textured with terrain, record its entity
type and the startup diagnostic `Client texture cache: ...` result so the
missing asset can be traced without adding a fallback renderer.

### Single-owner fixed-step world tick (2026-10-22)

The UWP world loop now uses the existing original client through
`NinecraftApp::updateFixedStep()` and `Minecraft::updateUwpFixedStep()`.  The
UWP scheduler is the only fixed 20 Hz cadence for an active world; the original
`Minecraft::update()` path, with its independent wall-clock `Timer`, remains
for menus and non-world screens.  This removes the previous double-scheduling
of `Minecraft::tick()` that could produce process-wide, startup-dependent
movement correction/pullback, especially with a far chunk view.

No renderer, world generator, or UWP fallback simulation was added.  The x64
UWP Debug build succeeds with zero errors; runtime verification still requires
multiple cold launches, far-view continuous movement, and switching between
several saves in one process.

### Local CreatorLevel daylight and mob tick parity (2026-10-22)

The UWP-owned local `CreatorLevel` uses a deliberately lightweight tick rather
than the network packet-emitting base `Level::tick()`.  That local override had
also omitted two original gameplay responsibilities: `updateSkyDarken()` and
`MobSpawner::tick()`.  The override now restores both while retaining its
offline storage/tick path.  This makes the cached block-light sky-darkness
follow the advancing world time and enables the original hostile/friendly mob
spawn code for survival worlds.  No custom lighting or spawning algorithm was
added.

The x64 UWP Debug build succeeds with zero errors; runtime verification remains
to observe a saved world through sunrise/sunset and to wait through a night for
hostile spawns.

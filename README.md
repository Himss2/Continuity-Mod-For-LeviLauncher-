# Continuity Bedrock — LeviLauncher RE POC

Native Bedrock reimplementation experiment inspired by Java Continuity. This repository is prepared for **LeviLauncher Android 1.5.24**, `preloader-android 0.2.2`, Android NDK `28.2.13676358`, and Minecraft Bedrock **1.26.52.3 arm64**.

This is deliberately a **diagnostic-first POC**, not a finished release. By default it installs hooks and probes the bundled atlas but leaves `enableLegacyCtm=false`, so it should not alter world rendering until the renderer path is confirmed on-device.

## What is already implemented

- verified signatures from the supplied `libminecraftpe.so 1.26.52.3`
- legacy `BlockTessellator::_getTexture` hook
- `BlockTessellatorCache::getBlock` 8-neighbor query using validated `this + 0x678`
- exact Continuity 256-mask -> 47-tile CTM lookup
- bundled Bedrock resource pack with 47 generated debug tiles
- lazy atlas resolution through `BlockGraphics::getTextureUVCoordinateSet`
- diagnostic hook for `BlockTessellatorPipeline::useNewTessellation`
- `.properties` parser scaffold and bundled `glass.properties`
- fail-safe behavior when signatures do not match

See `docs/RE_1.26.52.3.md` and `docs/ARCHITECTURE.md`.

## First test

Build/import with defaults. Enter a world containing normal glass, move around enough to rebuild chunks, then close the game and inspect logcat. Useful messages:

```text
Atlas probe ready: glass + 47 CTM tiles resolved
minecraft:glass reached legacy BlockTessellator::_getTexture path
Renderer stats: getTexture=..., glassLegacy=..., pipelineChecks=..., newTrue=..., newFalse=...
```

If `glassLegacy > 0`, set this in the mod config for the second test:

```json
"enableLegacyCtm": true
```

The bundled textures are intentionally obvious debug placeholders; visual correctness is not the goal of the first renderer test.

If `glassLegacy == 0`, do **not** force the legacy path. The next task is the new `ClientBlockPipeline` hook around material/face extraction.

## Build with Termux / Linux

You can use CMake directly with the Android NDK. The packaged target is `levi_package`. Example shape:

```bash
cmake -S . -B build-config -G Ninja
cmake --build build-config --target levi_generate_config

cmake -S . -B build-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_shared \
  -DLEVI_PACKAGE_CONFIG_DIR="$PWD/build-config/generated-config"

cmake --build build-arm64 --target levi_package
```

PowerShell users can run:

```powershell
./scripts/package.ps1
```

## Verify the exact binary before testing

The analyzed binary itself is intentionally **not** included in this repository.

```bash
python scripts/verify_signatures.py /path/to/libminecraftpe.so
```

For the analyzed file all four primary patterns must report one exact match at their documented RVAs.

## Repository layout

```text
src/engine/     RE bridges, CTM lookup, atlas cache, hooks
src/mod/        Levi lifecycle + typed config
src/util/       .properties parser scaffold
resources/
  continuity/   rule metadata
  minecraft_resource_packs/continuity_bedrock/  bundled Bedrock RP
scripts/        package + signature verification
docs/           RE and architecture notes
```

## Important limitations

- only `1.26.52.3` arm64 is targeted right now
- generic Java `.properties` compatibility is not wired into hot-path matching yet
- the experimental CTM path currently recognizes vanilla glass by its atlas UV and connects canonical neighbor block pointers
- `ctm_compact`, overlays, emissive, custom layers and glass-pane geometry are not implemented
- new `ClientBlockPipeline` mutation is intentionally not guessed; runtime diagnostics decide the next hook

## License

LGPL-3.0. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

# Continuity Bedrock — LeviLauncher RE POC

Native Bedrock reimplementation experiment inspired by Java Continuity. This repository targets **LeviLauncher Android 1.5.24**, `preloader-android 0.2.2`, Android NDK `28.2.13676358`, and Minecraft Bedrock **1.26.52.3 arm64**.

This is a **diagnostic-first POC**, not a finished release.

## Connected glass

**Connected glass is deliberately excluded from this mod.**

BedrockTools now contains its own Connected Glass module for glass blocks and panes. Continuity Bedrock therefore does not bundle glass CTM textures, a glass rule, glass-pane culling, or any glass-specific UV replacement logic. This keeps both mods able to coexist without two modules trying to own the same rendering behavior.

The generic Continuity CTM lookup code remains as renderer groundwork for future **non-glass** resource-pack rules.

## Current state

Implemented RE groundwork:

- verified signatures from the supplied `libminecraftpe.so 1.26.52.3`
- legacy `BlockTessellator::_getTexture` diagnostic hook
- diagnostic hook for `BlockTessellatorPipeline::useNewTessellation`
- known `BlockTessellatorCache::getBlock` and atlas resolver addresses retained for later generic rule work
- Continuity 256-mask -> 47-tile algorithm retained as generic engine code
- fail-safe behavior when signatures do not match
- no active texture replacement in the current build

The old connected-glass POC and bundled glass resource pack were removed.

## Diagnostic test

Build/import with defaults, enter a world, move around enough to rebuild chunks, then close the game and inspect logcat.

Useful output:

```text
RE 1.26.52.3: _getTexture=..., cacheGetBlock=..., atlasUv=..., useNew=...
Renderer diagnostics installed; no texture replacement is active.
Renderer stats: getTexture=..., pipelineChecks=..., newTrue=..., newFalse=...
```

This build is intended only to verify that the renderer hooks remain stable before the first non-glass Continuity method is implemented.

## Next renderer target

The next POC should use a non-glass rule such as Continuity's horizontal bookshelf method. That lets us validate rule parsing, atlas lookup, neighbor orientation and UV replacement without overlapping BedrockTools.

## Build with Termux / Linux

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

PowerShell:

```powershell
./scripts/package.ps1
```

## Verify the exact binary

```bash
python scripts/verify_signatures.py /path/to/libminecraftpe.so
```

## Important limitations

- only `1.26.52.3` arm64 is targeted right now
- generic Java `.properties` compatibility is not yet wired into runtime matching
- no texture replacement is active in the current diagnostic build
- connected glass is delegated to BedrockTools
- `ctm_compact`, overlays, emissive and custom layers are not implemented
- new `ClientBlockPipeline` mutation will be implemented only after its runtime path is validated

## License

LGPL-3.0. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

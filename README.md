# Continuity Bedrock — LeviLauncher

Native Bedrock reimplementation experiment inspired by Java Continuity. Current target: **Minecraft Bedrock 1.26.52.3 arm64**, LeviLauncher Android 1.5.24, preloader-android 0.2.2, Android NDK 28.2.13676358.

## Current milestone: generic horizontal rule engine

The renderer hook no longer contains bookshelf-specific matching. Runtime behavior now follows this pipeline:

```text
resources/continuity/rules/*.properties
        ↓
properties scanner/compiler
        ↓
compiled RuleDefinition
        ↓
block-type candidate selection
        ↓
source atlas match
        ↓
method processor
        ↓
neighbor lookup
        ↓
replacement TextureUVCoordinateSet
```

The first supported processor is:

- `method=horizontal`
- alias `method=bookshelf`
- `connect=block`
- `faces=all|sides|down|up|north|south|west|east`
- `orient=none`
- exactly 4 replacement tiles
- `matchBlocks` and/or `matchTiles`

The bundled `bookshelf.properties` is now only a test rule for that generic engine.

## Connected glass policy

**Connected glass is intentionally excluded.**

BedrockTools already owns connected rendering for glass blocks and panes. The rule compiler rejects known glass/pane targets so this mod does not compete with BedrockTools over the same renderer path.

The generic 47-tile CTM lookup remains in the source because it will be used for future non-glass `method=ctm` support.

## Performance

The expensive `useNewTessellation` diagnostic hook was removed after runtime testing showed the legacy BlockTessellator path is overwhelmingly dominant on the tested 1.26.52.3 build.

The active hot path now:

1. calls vanilla `BlockTessellator::_getTexture`;
2. uses a thread-local cache for the current `BlockType` candidate rules;
3. returns immediately when no rule can match;
4. only queries neighbors after a rule, face, block and source tile all match.

Atlas entries are resolved lazily once per rule and cached.

## Test rule

`resources/continuity/rules/bookshelf.properties`:

```properties
matchBlocks=minecraft:bookshelf
matchTiles=bookshelf
method=horizontal
tiles=continuity_bookshelf_0-3
faces=sides
connect=block
innerSeams=false
orient=none
```

Continuity horizontal mapping:

```text
no neighbours  -> tile 3
left only      -> tile 2
right only     -> tile 0
left + right   -> tile 1
```

## Build

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

## RE anchors for 1.26.52.3

- `BlockTessellator::_getTexture`: RVA `0xA67583C`
- `BlockTessellatorCache::getBlock`: RVA `0xA65CAD4`
- `BlockGraphics::getTextureUVCoordinateSet`: RVA `0xA66EB14`

Use:

```bash
python scripts/verify_signatures.py /path/to/libminecraftpe.so
```

before testing another binary.

## Next

The next processors should be implemented on top of the same compiled-rule path rather than adding block-specific hooks:

- vertical
- fixed
- top
- random
- repeat
- generic 47-tile CTM for non-glass targets
- multipass
- later: compact CTM, overlay, emissive

## License

LGPL-3.0. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

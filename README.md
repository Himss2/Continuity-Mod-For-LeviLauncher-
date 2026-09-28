# Continuity Bedrock — LeviLauncher

Native Bedrock reimplementation experiment inspired by Java Continuity.

Current target:
- Minecraft Bedrock **1.26.52.3 arm64**
- LeviLauncher Android 1.5.24
- preloader-android 0.2.2
- Android NDK 28.2.13676358

## Current milestone: multi-method compiled rule engine

The legacy renderer path was validated on-device and the renderer hook no longer contains block-specific CTM logic.

Runtime flow:

```text
resources/continuity/rules/*.properties
        ↓
properties scanner/compiler
        ↓
compiled RuleDefinition
        ↓
BlockType candidate cache
        ↓
matchBlocks / matchTiles / faces
        ↓
method processor
        ↓
cached neighbour lookup when required
        ↓
replacement TextureUVCoordinateSet
```

Supported methods in **0.3.0-re-poc**:

- `fixed`
- `horizontal` / `bookshelf`
- `vertical`
- `horizontal+vertical` / `h+v`
- `vertical+horizontal` / `v+h`
- `random`
- `repeat`
- `ctm` / `glass` algorithmically, but glass targets are rejected

Current connecting-method support is `connect=block`. Current orientation support is `orient=none`.

### Method details

`fixed`
- exactly 1 tile

`horizontal`
- exactly 4 tiles
- Continuity mapping `{3,2,0,1}`

`vertical`
- exactly 4 tiles
- same Continuity mapping `{3,2,0,1}` applied to local down/up

`horizontal+vertical` and `vertical+horizontal`
- exactly 7 tiles
- exact secondary connection lookup tables ported from Continuity

`random`
- one or more tiles
- deterministic position/face hash compatible with Continuity
- supports `weights`, `randomLoops=0..9`, `symmetry=none|opposite|all`, and `linked=true|false`
- weights are normalized once during rule compilation, not in the render hot path

`repeat`
- requires positive `width` and `height`
- tile count must equal `width*height`
- face projection follows Continuity/OptiFine formulas
- supports `symmetry=none|opposite|all`
- `orient=none` only for now

`ctm`
- exactly 47 tiles
- exact Continuity 256-mask → 47-tile lookup
- 4 cardinal + conditional diagonal neighbour checks
- non-glass targets only

## Performance

The `useNewTessellation` diagnostic hook was removed after runtime validation showed the legacy BlockTessellator path dominates on the tested build.

The active hook:
1. calls vanilla `BlockTessellator::_getTexture`;
2. caches candidate rules per `BlockType` per tessellation thread;
3. returns immediately when no rule can match;
4. resolves atlas entries once per rule;
5. performs neighbour queries only after block, face and source texture match.

## Bundled vanilla-style validation blocks

Besides bookshelf, the bundled resource pack now includes Continuity's default vanilla-style horizontal CTM assets for:

- `minecraft:cut_sandstone`
- `minecraft:chiseled_sandstone`
- `minecraft:cut_red_sandstone`
- `minecraft:chiseled_red_sandstone`

These rules use `matchBlocks` + `faces=sides` + `connect=block`. They intentionally do not depend on Bedrock's vanilla atlas key naming, which can differ from Java/OptiFine naming.

## Connected glass policy

**Connected glass and glass panes are intentionally excluded.**

BedrockTools already owns Connected Glass. The compiler rejects known glass/pane targets even though the generic `ctm` processor exists, preventing both mods from competing over the same rendering path.

## Bundled validation rule

The bundled `bookshelf.properties` remains as a non-glass validation rule. Its four CTM tiles now use Continuity's default bookshelf assets, preserving the vanilla bookshelf visual while still demonstrating horizontal connection behavior:

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

## Not implemented yet

- `top`: requires correct block-axis/state handling plus connection parity
- `connect=state`
- `connect=tile`
- block-state predicates
- `orient=state_axis`
- `orient=texture`
- `<skip>` / `<default>`
- multipass
- compact CTM geometry splitting
- overlays
- emissive extra-quad/material path

These are intentionally deferred rather than approximated incorrectly.

## RE anchors — Minecraft 1.26.52.3

- `BlockTessellator::_getTexture`: RVA `0xA67583C`
- `BlockTessellatorCache::getBlock`: RVA `0xA65CAD4`
- `BlockGraphics::getTextureUVCoordinateSet`: RVA `0xA66EB14`

Validate another binary with:

```bash
python scripts/verify_signatures.py /path/to/libminecraftpe.so
```

## License

LGPL-3.0. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Test branch: plank overlay POC

Branch `test/plank-overlay-poc` adds the first Bedrock **extra-quad style** experiment.

The base block face is rendered normally. On side faces of a full cube, an adjacent `*_planks` block can add four irregular strips onto the target face. The extra strips reuse the adjacent plank's live vanilla `TextureUVCoordinateSet`; no custom plank PNG is bundled, so the test follows the currently loaded terrain atlas.

This is intentionally narrower than full Continuity overlay parity:
- side faces only;
- full-cube targets only;
- plank source blocks only;
- no glass;
- no slab/stair overlay geometry;
- corners may overlap until the standard 17-state overlay resolver is ported.

The branch is a renderer-path validation before brick and generic overlay rules are implemented.

# Continuity Bedrock — LeviLauncher

Native Bedrock reimplementation experiment inspired by Java Continuity.

Current target:
- Minecraft Bedrock **1.26.52.3 arm64**
- LeviLauncher Android 1.5.24
- preloader-android commit `7ca94daedfa30d6d4c265fc9b591768e8dc1f5af` (the exact preloader bundled by LeviLauncher 1.5.24)
- Android NDK 28.2.13676358

### LeviLauncher 1.5.24 Mod Menu ABI fix

Version **0.5.1-re-poc** pins the native SDK headers to preloader commit `7ca94daedfa30d6d4c265fc9b591768e8dc1f5af`, which is the exact submodule revision bundled by LeviLauncher 1.5.24.

The previous build used preloader tag `0.2.2` (commit `99c0ae753a4623b8482dde6f4d59cf442283375d`). Its `pl::modmenu::ModuleInfo` layout is older and does not contain the `onKeybind` callback field present in the 1.5.24 runtime. Passing that older C++ struct ABI into the newer runtime caused the startup SIGSEGV inside `pl::modmenu::registerModule`.

## Levi Mod Menu runtime toggle

Version **0.5.0-re-poc** registers a native Mod Menu module named **Continuity Connected Textures** through `pl::modmenu::ModuleBuilder`.

The module toggle:

- enables/disables the compiled Continuity rule engine without unloading the native mod;
- persists `enableRuleEngine` back to the typed config;
- keeps the lightweight renderer hook installed and fast-returns vanilla UVs while disabled;
- does not control Connected Glass, which remains owned by BedrockTools.

Version **0.5.2-re-poc** intentionally disables the experimental automatic `RenderChunkCoordinator::setAllDirty` refresh path after an on-device crash showed that the coordinator object traversal is not safe enough for this Minecraft build. Existing meshes therefore refresh on the next natural chunk rebuild while the Mod Menu toggle itself remains runtime-safe.

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

### Bedrock sandstone end-cap correction

Bedrock's rendering makes the left outer edge of Continuity's default cut/chiseled sandstone tiles much less visible than the right edge. Version **0.4.1-re-poc** applies a resource-only correction to tiles `0` and `3` for cut/chiseled sandstone and their red variants so the retained left end-cap is visually balanced with the right end-cap. The connection algorithm, direction mapping and internal-edge tiles are unchanged.

## Cross-block continuity

Version **0.4.0-re-poc** adds the first true cross-BlockType connection path:

- `method=top`
- `connect=tile`
- matching is based on the face's source texture rather than BlockType equality
- the neighbor's vanilla texture is queried through the original `BlockTessellator::_getTexture` trampoline, so the Continuity hook does not recurse

Bundled default Continuity rules:

```properties
method=top
matchTiles=sandstone
tiles=continuity_sandstone_top_0
connect=tile
```

and the equivalent rule for `red_sandstone`.

This allows blocks with different IDs—such as full blocks, slabs, or stairs—to participate when the rendered face uses the same sandstone tile. The current `top` implementation uses Continuity's default Y axis path; AXIS-aware rotated blocks are deferred until block-state/axis support is added.

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

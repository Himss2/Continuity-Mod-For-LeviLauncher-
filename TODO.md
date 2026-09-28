# Roadmap

## Current diagnostic pass
- Verify legacy `BlockTessellator::_getTexture` hook stability.
- Measure legacy-vs-new tessellation activity.
- Keep rendering unchanged.
- Keep connected glass fully delegated to BedrockTools.

## First non-glass visual POC
- Use a Continuity-compatible non-glass rule, preferably horizontal bookshelf.
- Resolve the vanilla source texture plus 4 replacement atlas entries.
- Validate left/right face orientation.
- Validate neighbor lookup through the tessellator cache.

## Generic compatibility layer
- Compile `matchBlocks` / `matchTiles` once on resource reload.
- Add connect=block/state/tile.
- Add faces / orientation / innerSeams.
- Add horizontal, vertical, fixed, random, repeat.
- Add generic 47-tile CTM for non-glass rules.
- Add multipass.

## New renderer pipeline
- RE the mutation point around `SurfaceExtractionStep::run`, `FaceMaterial`, and `MaterialFaceAttributes`.
- Prefer texture-index/UV mutation before final MeshData emission.

## Later renderer features
- compact CTM
- overlay
- emissive
- custom block layers

## Explicit non-goal
- Do not add connected-glass block/pane rendering while BedrockTools provides that module.

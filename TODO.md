# Roadmap

## Runtime test 1
- Build/import with default config.
- Confirm `Atlas probe ready`.
- Confirm whether `glassLegacy` becomes non-zero.

## Runtime test 2 (only if glassLegacy > 0)
- Set `enableLegacyCtm=true`.
- Validate 47-tile selection and face orientation.
- Correct `directionsForFace()` parity if any face is mirrored/rotated.

## If glassLegacy == 0
- Keep legacy CTM disabled.
- RE the new-pipeline mutation point around `SurfaceExtractionStep::run` / `FaceMaterial` / `MaterialFaceAttributes`.
- Prefer changing `mTextureIndex`/UV attributes rather than post-editing final MeshData.

## Compatibility layer
- Compile `matchBlocks` / `matchTiles` once on resource reload.
- Add connect=block/state/tile.
- Add faces / orientation / innerSeams.
- Add horizontal, vertical, fixed, random, repeat.
- Add multipass.

## Later renderer features
- compact CTM: geometry split / extra faces
- overlay: extra-quad path
- emissive: second material/quad or lighting/material override
- glass-pane culling

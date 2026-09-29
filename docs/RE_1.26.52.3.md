# Reverse engineering notes — Bedrock 1.26.52.3

Target binary supplied for analysis:

- size: `328,419,680` bytes
- SHA-256: `5f5baf8e9ded92432e4c2cc9ff58a9313245323e8ca53ce37c169cc720e757c4`

## Required renderer anchors

| Function | RVA |
|---|---:|
| `BlockTessellator::_getTexture(BlockPos const&, Block const&, uchar, int, BlockGraphics const*) const` | `0xA67583C` |
| `BlockTessellatorCache::getBlock(BlockPos const&)` | `0xA65CAD4` |

## Confirmed facts used by Better Grass

1. `_getTexture` receives `this, BlockPos*, Block*, face, forcedVariant, BlockGraphics*` and returns `TextureUVCoordinateSet const*`.
2. Legacy face indices are `0=down, 1=up, 2=north, 3=south, 4=west, 5=east`.
3. A null BlockGraphics argument makes `_getTexture` resolve BlockGraphics from the supplied block.
4. A negative `forcedVariant` makes the function use the block's native state/variant selection.
5. The validated BlockTessellator cache object is at `this + 0x678`.

Better Grass deliberately calls the original selector with `face=UP`,
`forcedVariant=-1`, and `graphics=nullptr` so Minecraft performs the live
texture/resource-pack resolution itself.

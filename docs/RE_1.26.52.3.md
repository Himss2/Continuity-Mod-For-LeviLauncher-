# Reverse engineering notes — Bedrock 1.26.52.3

Target file supplied for analysis:

- size: `328,419,680` bytes
- SHA-256: `5f5baf8e9ded92432e4c2cc9ff58a9313245323e8ca53ce37c169cc720e757c4`

Validated unique signatures / RVAs:

| Function | RVA |
|---|---:|
| `BlockTessellator::_getTexture(BlockPos const&, Block const&, uchar, int, BlockGraphics const*) const` | `0xA67583C` |
| `BlockTessellatorCache::getBlock(BlockPos const&)` | `0xA65CAD4` |
| `BlockGraphics::getTextureUVCoordinateSet(std::string const&, int, int)` | `0xA66EB14` |
| `ClientBlockPipeline::BlockTessellatorPipeline::useNewTessellation(Block const&, bool)` | `0xA66F20C` |
| `RenderChunkBuilder::build(...)` | `0xA5C5EA0` |
| `ClientBlockPipeline::BlockTessellatorPipeline::_run(...)` | `0xA6D1A0C` |
| `WorldExtractorStep::run(...)` | `0xA5E6AC4` |
| `SurfaceExtractionStep::run(...)` | `0xA62807C` |
| texture-variation selector candidate | `0xA66D964` |

Important runtime facts confirmed from disassembly:

1. `BlockTessellator::_getTexture` receives `this, BlockPos*, Block*, face, forcedVariant, BlockGraphics*` and returns a `TextureUVCoordinateSet const*` in `x0`.
2. `BlockGraphics::getTextureUVCoordinateSet` uses AArch64 hidden sret `x8`; atlas entries are `0x58` bytes.
3. A legacy tessellation function at `0xA679C64` forms `this + 0x678` before calling `0xA65CAD4`. Therefore `0x678` is the validated cache-object offset for this exact build.
4. Bedrock 1.26.52.3 has both legacy `BlockTessellator` and the new `ClientBlockPipeline`. Do not assume all blocks use one path.

The POC intentionally hooks only the legacy texture selector and the pipeline decision function. New-pipeline face/material mutation is the next RE stage after runtime logs tell us where vanilla glass actually lands.

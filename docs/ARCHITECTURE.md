# Architecture

```text
Bundled resource pack
  terrain_texture.json + 47 CTM debug tiles
                 |
                 v
BlockGraphics::getTextureUVCoordinateSet
                 |
             AtlasCache
                 |
.properties -> future RuleCompiler -> CTM Resolver
                                  |
BlockTessellator::_getTexture ----+
       |                          |
       +-- BlockTessellatorCache::getBlock (8 neighbors)
                                  |
                         256-mask -> 47 tile
                                  |
                          replacement UV pointer
```

## Current scope

- exact build: Minecraft `1.26.52.3`, arm64
- safe signature resolution; no hardcoded module base
- bundled LeviLauncher 1.5.24 resource-pack layout
- 47-tile Continuity CTM lookup table
- legacy glass-path detection
- optional experimental legacy UV replacement
- dual-pipeline diagnostics

## Next stage

- confirm vanilla glass runtime path from logs
- if glass is new-pipeline: hook the material/face attribute stage around `SurfaceExtractionStep`
- compile `.properties` into generic rules (`matchBlocks`, `matchTiles`, `connect=block/tile/state`, faces, orientation)
- add horizontal/vertical/random/repeat
- later: compact CTM / overlay / emissive require extra-quad or mesh mutation support

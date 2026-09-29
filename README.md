# Better Grass — LeviLauncher

Native Fancy Better Grass for Minecraft Bedrock on LeviLauncher.

## Target

- Minecraft Bedrock **1.26.52.3 arm64**
- LeviLauncher Android **1.5.24**
- Android NDK **28.2.13676358**
- preloader-android commit `7ca94daedfa30d6d4c265fc9b591768e8dc1f5af`

## Fancy rule

The current build changes only horizontal faces of `grass_block`.

For a grass block at `P`, a side becomes full grass only when:

```text
P + faceDirection + DOWN
```

is also grass.

```text
north -> (x,   y-1, z-1)
south -> (x,   y-1, z+1)
west  -> (x-1, y-1, z)
east  -> (x+1, y-1, z)
```

This keeps cliffs vanilla while making exposed shallow grass slopes visually continuous.

## Minecraft-native texture selection

The mod bundles **no grass PNG and no terrain_texture.json override**.

When a side matches the Fancy rule, the hook calls Minecraft's original
`BlockTessellator::_getTexture` for the same grass block's UP face with:

```text
face = UP
forcedVariant = -1
BlockGraphics* = nullptr
```

For the validated Minecraft 1.26.52.3 binary, this makes Minecraft resolve the
block graphics and native block/state variant itself. The returned
`TextureUVCoordinateSet*` therefore comes from the active Minecraft terrain
atlas/resource pack rather than a debug texture or hardcoded UV.

The mod never copies or modifies `TextureUVCoordinateSet`; it only returns
Minecraft's own live pointer.

## Runtime flow

```text
BlockTessellator::_getTexture
        |
        +-- vanilla side texture
        |
        +-- BetterGrassResolver
                |
                +-- grass_block?
                +-- horizontal side?
                +-- grass at face + down?
                |
                +-- YES -> Minecraft native UP texture
                +-- NO  -> vanilla side texture
```

## Mod Menu

The native Levi Mod Menu module **Better Grass** can toggle the feature at
runtime. Existing chunk meshes refresh when Minecraft naturally rebuilds them;
the mod intentionally does not use the previously unsafe forced chunk-rebuild
traversal.

## Current scope

Implemented:

- Fancy grass-block connection logic
- native neighbor lookup
- Minecraft-native live UP texture lookup
- active resource-pack texture selection
- Mod Menu toggle
- typed config
- Minecraft 1.26.52.3 arm64 signatures

Not implemented yet:

- snowy grass
- mycelium
- podzol
- dirt path
- crimson / warped nylium
- explicit new ClientBlockPipeline fallback

## RE anchors — Minecraft 1.26.52.3

Validated against the supplied binary:

- `BlockTessellator::_getTexture`: RVA `0xA67583C`
- `BlockTessellatorCache::getBlock`: RVA `0xA65CAD4`
- BlockTessellator cache object offset: `0x678`

## License

LGPL-3.0. See `LICENSE`.

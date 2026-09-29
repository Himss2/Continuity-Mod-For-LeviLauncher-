# Architecture

```text
Minecraft active resource pack / terrain atlas
                    |
                    v
BlockTessellator::_getTexture
        |                         |
        | vanilla side           | native UP lookup
        v                         |
BetterGrassHook ------------------+
        |
        v
BetterGrassResolver
        |
        +-- classify grass_block
        +-- reject up/down
        +-- BlockTessellatorCache::getBlock(face + down)
        |
        +-- no match -> original side
        |
        +-- match -> native UP-face TextureUVCoordinateSet*
```

The mod never copies, edits, or owns `TextureUVCoordinateSet`. It forwards the
pointer returned by Minecraft. This keeps texture selection tied to the active
resource pack and avoids relying on the texture structure's opaque internals.

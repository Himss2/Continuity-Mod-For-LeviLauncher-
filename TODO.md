# Roadmap

## Done
- Validate legacy BlockTessellator hook on 1.26.52.3.
- Validate cached neighbor lookup.
- Validate custom terrain-atlas UV replacement.
- Validate Continuity horizontal 4-tile mapping with bookshelf.
- Remove the expensive useNewTessellation diagnostic hook.
- Exclude connected glass for BedrockTools compatibility.
- Replace hardcoded bookshelf renderer logic with a compiled .properties rule engine.

## Current supported rule subset
- method=horizontal / bookshelf
- connect=block
- matchBlocks
- matchTiles
- tiles with compact numeric ranges such as 0-3 or prefix_0-3
- faces
- innerSeams parsed
- orient=none

## Next processors
- method=vertical
- method=fixed
- method=top
- method=random
- method=repeat
- generic method=ctm (47 tiles, non-glass)
- horizontal+vertical / vertical+horizontal

## Rule compatibility
- connect=state
- connect=tile
- block state predicates
- resourceCondition
- heights / biomes
- prioritize and deterministic ordering parity
- orientation=state_axis / texture
- multipass

## Later renderer work
- compact CTM geometry splitting
- overlay extra-quad path
- emissive extra material/lighting path
- new ClientBlockPipeline support only where required

## Explicit non-goal
- Do not implement connected glass or glass-pane rendering while BedrockTools provides that module.

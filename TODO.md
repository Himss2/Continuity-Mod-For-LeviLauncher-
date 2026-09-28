# Roadmap

## Completed
- Legacy BlockTessellator renderer path validated on-device.
- Cached neighbour lookup validated.
- Bedrock terrain-atlas UV replacement validated.
- .properties scanner and compiled rule engine validated.
- BlockType candidate cache added.
- Connected glass excluded for BedrockTools compatibility.
- Heavy useNewTessellation diagnostic removed from hot path.
- method=horizontal validated visually.
- Added processors for:
  - fixed
  - vertical
  - horizontal+vertical / h+v
  - vertical+horizontal / v+h
  - random
  - repeat
  - generic 47-tile CTM for non-glass targets

## Next compatibility work
- Add controlled test rules/assets for each new processor.
- connect=state
- connect=tile
- block-state predicates
- top with correct AXIS handling
- orient=state_axis
- orient=texture
- resourceCondition
- prioritize / pack ordering
- heights / biomes
- <skip> / <default>
- multipass

## Later renderer work
- compact CTM: geometry splitting / extra faces
- overlay: extra-quad path
- emissive: second material/quad and lighting
- ClientBlockPipeline support where legacy fallback is unavailable

## Explicit non-goal
- Do not implement connected glass or glass-pane rendering while BedrockTools provides that module.

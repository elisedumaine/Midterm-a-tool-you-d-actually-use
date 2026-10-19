# Midterm Pitch — Interactive World Builder

## The tool in one sentence

Interactive World Builder is an Unreal Engine Editor Utility Widget that lets an artist generate and iterate on a small, playable stylized forest from one control panel.

## The chore it ends

Building an early natural level normally means placing terrain, vegetation, rocks, lighting, and a player start one item at a time. This tool compresses that setup into reproducible settings so an artist can try variations quickly.

## Focus: depth over breadth

Instead of trying to finish four unrelated biomes, the project focuses on one polished forest. The tool lives inside Unreal as an Editor Utility Widget and uses the existing Third Person template for movement, leaving development time for world-building behavior.

## Main controls

- World name and deterministic seed
- Terrain size
- Tree, flower, rock, and grass density
- Lighting intensity and sky/ground/light colors
- Mountain and playable-character options
- Generate, Preview, Refresh Plants, Save Preset, Load Preset, Start Over, Reset, and Delete World

## What makes it more than one script

The widget coordinates several layers of a level: terrain, seeded placement, vegetation, rock clusters, path exclusion, lighting, and player start. It supports presets and preview, separates vegetation regeneration from terrain generation, and safely tags generated actors so replacement or deletion does not destroy unrelated level content.

## Placement rules

- Trees avoid the central path and unsuitable terrain.
- Rocks are placed in small groups instead of an even grid.
- Grass and flowers stay off the walking path.
- The seed rebuilds the same world from the same settings.
- Refresh Plants changes the vegetation layer without rebuilding the terrain or lighting.

## Milestones

| Date | Deliverable |
| --- | --- |
| October 13 | Working Editor Utility Widget and a basic generated terrain with a playable Third Person start. |
| October 20 | Seeded vegetation, rocks, grass, lighting, placement rules, and a polished forest pass. |
| October 26 | Presets, preview/reset/delete workflow, visual clean-up, documentation, and full playtest. |

## Smallest working version

If visual polish has to be reduced, the project still demonstrates the core goal: a GUI that generates one natural environment with adjustable terrain, vegetation, lighting, deterministic results, and a movable template character.

## Main risk and response

The major risk is visual clutter or actors being placed incorrectly. The tool handles this through tagged generated actors, a clean regenerate/delete path, constrained placement rules, and a separate vegetation-refresh action.

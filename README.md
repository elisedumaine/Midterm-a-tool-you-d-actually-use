# Interactive World Builder

**Midterm — a tool you'd actually use**
**Unreal Engine 5.8 · Editor Utility Widget · Python (+ a small C++ module for the play mode)**

Interactive World Builder is an Unreal Editor tool that builds a small, playable stylized forest from one panel. Instead of placing terrain, trees, rocks, paths, and lights one at a time, an artist picks a seed and a few settings and clicks **Build World**. The same seed and settings always give back the same world.

**Demo video (1–3 min):** _link coming: add the video URL here_

## Where the tool lives

The tool is **not a standalone app**. It runs **inside Unreal Engine 5.8** as an **Editor Utility Widget**, a docked panel in the level editor. The Third Person template character lets you walk the generated world in Play mode.

## The chore it ends

Blocking out an early natural level by hand means placing the ground, the hills, the paths, hundreds of trees, rocks, plants, the lights, and a player start one by one. Trying a second version means doing it all again. This tool turns that work into a few settings and one button, so an artist can try ten forests in the time one used to take.

## Install and run from a fresh clone

### What you need

- **Windows** with **Unreal Engine 5.8**, installed from the Epic Games Launcher.
- **Visual Studio 2022** with the **Game development with C++** workload. The project has a small C++ module (`Source/ForestGame`, used for the animals, birds, and game HUD), and Unreal compiles it the first time the project opens.
- Houdini is **not** needed (see [Optional: Houdini river](#optional-houdini-river)).

### Steps

1. Clone the repository:

   ```powershell
   git clone https://github.com/elisedumaine/Midterm-a-tool-you-d-actually-use.git
   ```

2. Open the cloned folder and double-click `InteractiveWorldBuilder.uproject`. If Windows asks which Unreal version to use, choose **5.8**.
3. Unreal says that some modules (ForestGame) are missing or built with a different engine version, and asks to rebuild them. Click **Yes**. The first build and the shader compile take a few minutes.
4. When the editor is open, the **World Builder** panel opens by itself. If it does not, open it from the top menu: **Tools → Open Interactive World Builder**.
5. Click the **Build** tab, then **Build World**. Wait until the forest appears in the viewport and the status line says *World built and saved*. Click it only once.
6. Click the green **Play** button at the top, click once inside the game view, and walk with **W A S D**, look with the mouse, and jump with **Space**. Press **Esc** to stop.

To make another world, change the seed or any setting, click **Build World** again (or **New plants** to change only the plant layer), then press **Play**.

## What it does

The panel has four tabs. Every choice is a large icon tile; there are no dropdowns.

- **World**: name, **Seed** (with **New** to pick a random one), season, color palette, world size, **Hills** (from flat to hilly), **Paths** (None, Trail, Network), and mountains around the world.
- **Nature**: **Vegetation** (how much grows), **Forest cover** (woods versus open glades), Compact / Balanced / Lush profiles, a build estimate (how many trees, shrubs, rocks, and plants will be placed), and **Wildlife**.
- **Mood**: **Time of day** (Golden hour, Sunset, Day, Blue hour), sun strength and warmth, sky and ground tint, **Haze**, **Sparkles**, **Weather** (Clear, Rain, Snow), and **Playable character**.
- **Build**: **Build World**, **New plants**, presets (**Save**, **Load**, **Reset**), and **Delete World**. The status line stays visible under every tab.

What the generator takes care of:

- **Seeded and repeatable.** Every random choice comes from the seed, so a seed is a world you can come back to.
- **Ground**: a low-poly relief with rolling ground, winding valleys, inner hills, and tall mountain ranges beyond the edge. Slopes steeper than 38° inside the playable area are softened so every valley stays walkable.
- **Paths**: winding stepping-stone footpaths lined with lanterns, going around the hills. Trees, rocks, and plants stay off them.
- **Ecology, not random scatter**: dense woods and open glades, young trees and shrubs on woodland edges, stumps, logs, and mushrooms under the trees, rocks in small clusters, and a carpet of grass and flowers so nothing looks bare. Trunks, rocks, and buildings keep their distance from each other. Ground cover uses instanced meshes, so thousands of tufts stay fast.
- **Season and color are separate.** The season picks the tree species and the sun angle. The palette (Soft Dusk, Emberwild, Candy Cloud, Neon Night, Coral Lagoon, Golden Hour, Frost Violet, or Natural) is chosen on its own. Clicking a palette repaints the existing world, lights included, without rebuilding it.
- **Instant mood changes**: time of day, light, haze, sparkles, and weather relight the current world when you let go of a slider. Rain makes surfaces dark and shiny, Snow whitens every upward face, and Clear dries everything again.
- **New plants** regrows only the plant layer (trees, shrubs, flowers, grass, rocks, landmarks) and keeps the ground, paths, weather, and lighting. Each click gives a new layout on the same ground.
- **Safe cleanup**: everything the tool makes is tagged. Rebuilding or **Delete World** only removes the tool's own actors. While a world is active, the template scenery is hidden; Delete World brings it back with the original sun, sky light, and player start.
- **Ready to play**: the ground, trees, rocks, stumps, and lanterns have collision, and an invisible wall keeps the player inside. Grass, flowers, and shrubs can be walked through. **Build World** and **New plants** save the level, so closing Unreal never loses a world.
- **Presets**: **Save** writes the current settings to `Saved/InteractiveWorldBuilderPreset.json`; **Load** brings them back.

### Bonus: a small forest game in Play mode

Every generated world is also a small game. Walk up to a red mushroom and press **E** to pick it. Animals (deer, bunnies, foxes, boars, beavers) are shy and run away from empty hands. Carry a mushroom and they come closer; press **E** next to one to feed it, and it follows you. Feed them all and they dance. Birds hop in the glades and fly off when you come close, and birdsong and forest ambience play around you.

## What does not work yet

- Only one biome: a **forest** (four seasons, eight palettes). Mountain, beach, and meadow worlds are not built.
- It is an **editor tool**, not a packaged game or an in-game menu.
- The **PCG** plugin is enabled, but placement is done in Python, not with an authored PCG graph.
- The **river** needs Houdini open at the same time. Without Houdini, Build World skips it and says *No river this time*.
- After editing the Python code, Unreal must be **fully restarted**: it keeps the old panel callbacks in memory.
- A wide world on **Lush** takes longer to build. The build estimate on the Nature tab shows how many trees, shrubs, rocks, and plants it will place.

## What I would build next

1. **More biomes** on the same code: a meadow, a mountain, and a beach, each with its own species, ground, and palette.
2. **Paint where things go**: brush a zone in the viewport (more forest here, a clearing there) instead of using only global sliders.
3. **Move placement to a PCG graph** so artists can tweak the rules in Unreal's PCG editor without touching Python.
4. **A Houdini river that works without opening Houdini**, cooked through Houdini Engine inside Unreal.
5. **Preset library**: several named presets with a thumbnail of each world, instead of a single preset file.

## Peer run (A6)

**Classmate:** Fiori

1. **What I did:** I cloned elisedumaine/pfa-week06, followed the README, extracted InteractiveWorldBuilder_Project.zip, opened InteractiveWorldBuilder.uproject in Unreal Engine 5.8, and waited for the editor and shaders to finish loading.
2. **What happened:** The editor opened cleanly and the tool loaded — the log printed Interactive World Builder: World Builder v3.1 ready, and the EUW_WorldBuilderPanel widget asset compiled and validated, so the Python side and the panel were ready to use.
3. **What confused me / broke:** The repo's InteractiveWorldBuilder.uproject sits at the root with only Config/Content/Python/Plugins next to it (no level or meshes), so I was only able to run it after extracting the bundled zip into its own folder, and even then the log reported the Third Person level was incomplete — LoadErrors: ... /Game/ExternalActors/ThirdPerson/Lvl_ThirdPerson/D/RI/R8L7GQXCHQGMMKBPTBJTNA ... was not available — meaning the level is missing World Partition tiles, which was unclear to diagnose from the README alone.

**What I fixed after the peer run:** The repo now holds the whole project (level with all its World Partition actor files, meshes, materials, Blueprints, and C++), so there is no zip to extract: you clone and open the `.uproject`. The README lists every requirement and step, and a fresh clone opens with no missing-actor load errors.

## How it is built

| Part | File | Role |
| --- | --- | --- |
| Generator and panel | `Content/Python/interactive_world_builder.py` | Builds the panel, reads its values, and generates the world (`_build_world`). Also handles New plants (`refresh_plants`), palettes, mood, presets, and cleanup. |
| Startup | `Content/Python/init_unreal.py` | Unreal runs it when the project opens: it adds the **Tools** menu entry and opens the panel. |
| River | `Content/Python/iwb_river.py` | Optional river along the valleys, made by Houdini through a local bridge. |
| Play mode | `Source/ForestGame` | C++ actors: `AForestAnimal`, `AForestPickup`, `AForestGameManager`, `AForestHUD`, `AForestBirds`, `AForestAmbience`. |
| Blueprints | `Content/WorldBuilder/Gameplay` | `BP_Deer`, `BP_Bunny`, `BP_Fox`, `BP_Boar`, `BP_Beaver`, `BP_MushroomPickup`, `BP_ForestBirds`, `BP_ForestAmbience`: model, animations, size, and speed of each. |
| Panel art | `Tools/make_icons.py` | Draws the icons and palette swatches used in the panel. |
| Dev helpers | `Tools/ue.py`, `Tools/houdini.py` | Send Python to the open Unreal editor or the open Houdini, from a terminal. |

## Optional: Houdini river

This part is **not needed** to run the tool. With Houdini 22.0 and Houdini Engine for Unreal installed, `Houdini/iwb_river.hda` builds a river and its streams along the generated valleys, and `Houdini/iwb_lowpoly_rock.hda` makes faceted low-poly rocks. The Houdini Engine plugin is disabled in `InteractiveWorldBuilder.uproject` so the project opens on machines without Houdini; set `"HoudiniEngine"` to `"Enabled": true` to use it.

## Assets and credit

- Meshes: **Kenney Nature Kit**, **Kenney Fantasy Town Kit**, and **Kenney Cube Pets** (animated animals), all [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). They are included in `Content/WorldBuilder/Imported`, so nothing has to be downloaded separately.
- Sounds, CC0, from OpenGameArt.org: [Forest Ambience](https://opengameart.org/node/23888), [Ambient Bird Sounds](https://opengameart.org/content/ambient-bird-sounds) by isaiah658, [Bird chirping sounds](https://opengameart.org/content/bird-chirping-sounds) by syncopika, [Crows singing](https://opengameart.org/content/crows-singing).
- Unreal Engine's Third Person template provides the test character and movement.

See [MIDTERM_PITCH.md](MIDTERM_PITCH.md) for the original pitch and [SUBMISSION_NOTES.md](SUBMISSION_NOTES.md) for the demo plan.

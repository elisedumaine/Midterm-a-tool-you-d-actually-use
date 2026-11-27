# Interactive World Builder

**Midterm — a tool you'd actually use**
**Unreal Engine 5.8 · Editor Utility Widget · Python (+ a small C++ module for the play mode)**

Interactive World Builder is an Unreal Editor tool that builds a small, playable stylized forest from one panel. Instead of placing terrain, trees, rocks, paths, and lights one at a time, an artist picks a seed and a few settings and clicks **Build World**. The same seed and settings always give back the same world.

**Demo video (3 min):** https://vimeo.com/1242638803?share=copy&fl=sv&fe=ci

Demo video step 2 : https://vimeo.com/1249552660?share=copy&fl=sv&fe=ci

---

## Research Project 2 — A Living Forest (Dec 2026, in progress)

The proposal is in
[Research-Project-2-proposal](https://github.com/elisedumaine/Research-Project-2-proposal).
The forest the World Builder makes becomes a small survival game: animals with
needs, building, nights that are dangerous, and a forest that remembers how you
treat it. I want it to feel different from Minecraft: you don't mine the world,
you make friends with it.

### How a game goes

1. **Title screen** (Whispering Pines, 7 languages), the camera circling above the forest.
2. **Your character**: body, head, skin, hair and its colour, beard, top, bottom
   and shoes with their colours, or **Surprise me**. The character is cut from my
   Houdini generator into pieces (`Content/Python/lowpoly_character_parts.py`,
   about 220 small meshes) and painted in the game, so every mix works.
3. **Your world**: season, weather, how round the world is (flat to tiny
   planet), and how dangerous the nights are. Every change shows at once.
4. **Waking up**: you fall asleep at sunset and wake up at dawn in the world you made.

### What there is to do

- **Gather, craft, build**: wood, stone, berries, mushrooms; a workbench, tools,
  camp pieces, and blocks on a 1 m grid (log, plank, stone, leaves, glow, door).
  A house with walls and a roof is "home": you can sleep there and the night
  creatures can't reach you.
- **Animals with a life**: deer, bunnies, foxes, boars and beavers get hungry,
  sleep, hide from the rain and are shy. Feed them to win their trust. Tame
  animals **help**: the bunny finds berries, the deer brings wood, the boar digs
  up mushrooms, the beaver gnaws planks, the fox growls when a creature comes
  (and says from where), and a tame boar or fox can chase one away.
- **Animals move in**: a roof or a shelter becomes someone's home (a lit house
  attracts the fox), and residents leave gifts at dawn.
- **Nights**: red-eyed wolves and boars hunt you; they never enter firelight or
  glow blocks. Every night survived counts, dawn gives a reward, and a new animal
  comes to live in the forest every second night.
- **Themed nights**, announced at dusk: fog, **red moon** (twice the creatures,
  twice the reward), silent night (no creature, a star falls with a chest),
  storm. The real calendar plays too: Halloween and the real full moon bring a
  red moon, Christmas and New Year a silent night.
- **Life of the forest** (0-100 %): cutting trees and taking wood and stone
  lowers it; feeding, taming, planting and welcoming residents raise it. A
  thriving forest pays more and welcomes more animals; a suffering one sends
  more creatures. At dawn, **flowers grow** where you cared for the forest.
- **Fireflies and the shop** (U): fireflies are earned by surviving nights,
  taming and welcoming residents, and buy hats, a scarf, glasses or a backpack,
  each with its colour.
- **Seasons** turn every two days (autumn colours, winter snow and hungrier
  animals), the weather changes by itself, and events happen: lightning,
  earthquakes that uncover chests, shooting stars.
- **Music that follows the forest**: title, day, dusk, night, and a danger theme
  when red eyes are near. **Pause menu** (P): volumes, graphics, language, help.

### Keys

| Key | |
| --- | --- |
| W A S D, mouse, Space | walk, look, jump |
| E | pick up, feed, pet, open |
| F | a tame animal stays or follows |
| B | camp pieces (campfire, feeder, shelter...) |
| G / T | blocks / craft |
| Z | sleep at home |
| J | journal |
| U | shop |
| P | pause and settings |
| H | hide the help |

For tests and the presentation (console, the ` key): `Forest.Dawn`,
`Forest.NightTheme RedMoon`, `Forest.Health 90`, `Forest.Fireflies 100`,
`Forest.Season Autumn`, `Forest.House`, `Forest.MoveIn`.

### What is tested and what isn't yet

Everything builds, and the title screen, the creative pages, the music, the
nights counter, newcomers and animals moving in were checked in Play. Not yet
checked on screen: the colours of every character piece, the shop page, the
red moon and the flowers. Rain still falls inside houses.

---

## Research Project 1 — Low-Poly Nature Kit (Nov 2026)

**Research Project 1 video:** https://vimeo.com/1249552660?share=copy&fl=sv&fe=ci (the "step 2" video above)

The Houdini half (HDAs, build scripts, proposal) is in
[Research-Project-1-proposal](https://github.com/elisedumaine/Research-Project-1-proposal)
(`PROPOSAL.md` is what I proposed). This repo is the game it feeds.

### What I set out to do

A Houdini kit of low-poly generators (trees, rocks...) sharing one style, plus a
"Low-Polify" tool for any imported model, feeding this World Builder. Smallest
version: style + trees + rocks, baked and growing in one Unreal forest.

### What I made

- **Houdini HDAs**: one shared style (`lpk_style`), trees with 10 species
  (broadleaf, pine, birch, dead, palm, cherry, willow, cactus, giant mushroom,
  bamboo), rocks (pebble, boulder, slab, cliff, cluster), Low-Polify for imported
  models, a simple character generator. Bonus: photo → 3D with TripoSR, run locally.
- **Low-Poly Studio** (`Content/Python/lowpoly_studio*.py`): a game-style panel in
  Unreal with ◀ ▶ arrows, sliders and a live 3D preview. **Add to forest** bakes 6
  variations into the World Builder's population, so the generator grows my designs
  everywhere. Tabs: Trees, Rocks, Import, Character.
- **Living climate** (`Source/ForestGame/ForestSky`, `Content/Python/lowpoly_climate.py`):
  in Play time passes (a day = 12 min) and the weather changes by itself: clear,
  rain, storm with lightning and thunder, sun shower with a rainbow, mist, snow,
  stars and moon at night, wind that sways the plants.
- **Gameplay** (`Source/ForestGame`): a title screen (**PLAY**, English / Français,
  remembered between sessions), a wake-up intro (you lie in the forest at dawn after
  the rain, then get up), and the forest game: pick up mushrooms with **E**, feed the
  shy animals (deer, bunny, fox, boar, beaver) so they follow you, and a HUD with the
  basket, the animals fed, and the day, hour and weather.

### How it changed, and why

- I didn't use the Houdini Engine plugin. I wrote a small local bridge instead
  (Houdini stays open, Unreal sends it commands), because my Education license
  can't run Houdini in the background and I wanted to test vertex colors early.
- I dropped the mushroom/props tabs for a character tab.
- The climate wasn't in the proposal: once the forest worked I wanted it alive.

### How to run it

1. Install as below and open the project: the **Low-Poly Studio** opens by itself.
2. To design new trees/rocks, Houdini 22 must be open with the bridge (see the kit
   repo). Without it, the forest still works with the meshes already baked.
3. Pick a design, **Add to forest**, then **Grow forest now**.
4. Press **Play**, choose English/Français, press **PLAY**.

### What I learned

Houdini VEX and HDAs, vertex-color materials, Material Parameter Collections (one
value changes every material: snow, wet, wind), Python-driven editor panels, C++
actors. Debugging: a character with inside-out faces (I fixed it by hand in
Houdini), a preview that crashed Unreal out of memory, a rainbow material whose
colour was silently zero. I used an AI agent a lot; my job was to decide what to
build, judge it on screen and understand each part.

### What doesn't work yet

- Classmates can't use the Studio straight from GitHub: the kit uses my local paths.
- Checked in Play: title screen and languages, storm with lightning, sun shower with
  rainbow, stars. Not checked on screen yet: blowing leaves and dawn mist in the valleys.
- After the intro you could only jump (input stayed blocked); fixed the last evening,
  needs one more test.
- Low-Polify only tested on a few models; photo → 3D is slow (~75 s). No LODs.

### What I'd do next

Make the kit work for classmates (relative paths, setup script), aurora and
shooting stars at night, LODs, and a sandbox mode (pick up apples and berries,
feed animals).

---

## Where the tool lives

The tool is **not a standalone app**. It runs **inside Unreal Engine 5.8** as an **Editor Utility Widget**, a docked panel in the level editor. The Third Person template character lets you walk the generated world in Play mode.

## The chore it ends

Blocking out an early natural level by hand means placing the ground, the hills, the paths, hundreds of trees, rocks, plants, the lights, and a player start one by one. Trying a second version means doing it all again. This tool turns that work into a few settings and one button, so an artist can try ten forests in the time one used to take.

## Install and run from a fresh clone

### What you need

- **Windows** with **Unreal Engine 5.8**, installed from the Epic Games Launcher.
- **Visual Studio 2022** with the **Game development with C++** workload. The project has a small C++ module (`Source/ForestGame`, used for the animals, birds, and game HUD), and Unreal compiles it the first time the project opens.
- Houdini is **not** needed. It only adds the river (see [Optional: Houdini river](#optional-houdini-river)).

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
- It is an **editor tool**, not a packaged game (since Research Project 1, Play has a title screen).
- The **PCG** plugin is enabled, but placement is done in Python, not with an authored PCG graph.
- The **river** needs Houdini open at the same time (setup in [Optional: Houdini river](#optional-houdini-river)). Without Houdini, Build World skips it and says *No river this time*. The ground grid (one point every 5.7 m) is coarser than the streams, so right after some cascades the water can sit a little above or below the ground.
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
| Game start | `ForestStartMenu`, `ForestCreator`, `ForestLook`, `ForestIntroDirector` | Title screen, character and world pages, the character's pieces, the wake-up. |
| The living forest | `ForestBonds`, `ForestSpirit`, `ForestNight`, `ForestSeasons`, `ForestEvents`, `ForestLife`, `ForestBlocks` | Animal help and residents, life of the forest and themed nights, night creatures, seasons, events, survival life, blocks. |
| Menus and sound | `ForestShop`, `ForestPauseMenu`, `ForestMusic` | Shop and fireflies, pause menu and settings, music by mood. |
| Character pieces | `Content/Python/lowpoly_character_parts.py` | Cuts the Houdini character into pieces (needs Houdini open with the bridge). |
| Blueprints | `Content/WorldBuilder/Gameplay` | `BP_Deer`, `BP_Bunny`, `BP_Fox`, `BP_Boar`, `BP_Beaver`, `BP_MushroomPickup`, `BP_ForestBirds`, `BP_ForestAmbience`: model, animations, size, and speed of each. |
| Panel art | `Tools/make_icons.py` | Draws the icons and palette swatches used in the panel. |
| Dev helpers | `Tools/ue.py`, `Tools/houdini.py` | Send Python to the open Unreal editor or the open Houdini, from a terminal. |

## Optional: Houdini river

This part is **not needed** to run the tool. Without Houdini you get the whole forest; the river is the only thing missing.

With Houdini open, every **Build World** also makes a river that follows the new valleys, with gentle side streams, cascades with spray mist and the roar of falling water, the sound of the current, foam, boulders, and wooden bridges where the paths cross. Unreal finds the route and carves the bed into the ground; `Houdini/iwb_river.hda` turns the route into the water mesh; Unreal saves it as an ordinary static mesh, so Play mode never needs Houdini.

To get the river on your computer:

1. Install **Houdini 22.0** from [sidefx.com](https://www.sidefx.com/download/). The free **Houdini Apprentice** licence is enough.
2. Open Houdini once and close it (this creates its settings folder in `Documents\houdini22.0`).
3. Double-click `Tools\houdini\install_houdini_bridge.bat`. It copies two small Python files into `Documents\houdini22.0\python3.13libs`, so Houdini starts a local bridge (127.0.0.1 only, with a secret token) every time it opens.
4. Open Houdini and leave it open, then click **Build World** in Unreal. The status line says *River ready: … m of water, … cascades*.

The Houdini Engine plugin itself is **not** used (its bake step crashed the editor), and it is disabled in `InteractiveWorldBuilder.uproject` so the project opens on machines without Houdini. `Houdini/iwb_lowpoly_rock.hda` is a separate test tool: a faceted low-poly rock with Seed, Size, Roughness, and Detail.

## Assets and credit

- Meshes: **Kenney Nature Kit**, **Kenney Fantasy Town Kit**, and **Kenney Cube Pets** (animated animals), all [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). They are included in `Content/WorldBuilder/Imported`, so nothing has to be downloaded separately.
- Sounds, CC0, from OpenGameArt.org: [Forest Ambience](https://opengameart.org/node/23888), [Ambient Bird Sounds](https://opengameart.org/content/ambient-bird-sounds) by isaiah658, [Bird chirping sounds](https://opengameart.org/content/bird-chirping-sounds) by syncopika, [Crows singing](https://opengameart.org/content/crows-singing).
- Music, CC0, from OpenGameArt.org: Cathedral in the forest (congusbongus), Calm Track (pmiller), Beautiful Forest (nene), Insistent (yd), Tension (Komiku). Details in [Media/Music/CREDITS.md](Media/Music/CREDITS.md).
- The waterfall and river sounds (`S_Waterfall`, `S_River`) are synthesised by `iwb_river.py` itself (filtered noise and small rising bubbles), so they need no download or licence.
- Unreal Engine's Third Person template provides the test character and movement.

See [MIDTERM_PITCH.md](MIDTERM_PITCH.md) for the original pitch and [SUBMISSION_NOTES.md](SUBMISSION_NOTES.md) for the demo plan.

# Interactive World Builder

**PFA — Week 06 / Midterm project**  
**Unreal Engine 5.8 · Editor Utility Widget · Python**

Interactive World Builder is an Unreal Editor tool for quickly building a small, playable stylized forest. Instead of placing each element by hand, an artist changes a few controls, keeps a seed, and regenerates the world from one panel.

## Where the tool lives

This is **not a standalone app**. It lives inside **Unreal Engine 5.8** as an **Editor Utility Widget**. It is used while editing a level; the Third Person template is used to test the generated world in Play mode.

## What the tool does

- Generates a seeded forest world with terrain, trees, rocks, grass, flowers, lighting, sky color, and an optional Third Person player start.
- Lets the user set a world name, seed, terrain size, tree/flower/rock/grass density, lighting intensity, color palette, and whether mountains are used.
- Keeps placement intentional: trees avoid the main path and harsh terrain; rocks appear in small clusters; ground cover stays off the path.
- Rebuilds just the vegetation layer with **Refresh Plants**, rather than recreating terrain and lighting.
- Offers **Preview**, **Save Preset**, **Load Preset**, **Start Over**, **Reset**, and **Delete World** controls.
- Removes the default Third Person template scenery while a generated world is active, then restores it when the generated world is deleted.

## What works now

- A functional Editor Utility Widget exposes the forest controls and actions listed above.
- The forest generator supports deterministic seeds, density controls, a playable Third Person start, presets, preview, and safe generated-actor cleanup.
- Vegetation is regenerated separately from terrain and lighting, so an artist can revise the plant layer without rebuilding the whole scene.
- The project contains its Python scripts, enabled project settings, imported low-poly nature meshes, level, and documentation; it is small enough to clone without Git LFS.

## What does not work yet / known limits

- The polished production theme is **Forest**. Mountain, beach, and meadow are not complete, separate finished worlds yet.
- The tool is an editor workflow, not a packaged game or a runtime in-game menu.
- A PCG plugin is enabled in the project but the current iteration uses deterministic Python/editor placement rather than a finished authored PCG graph.
- Visual polish is still being iterated: the user should choose a preferred seed, test Play mode, and capture the final generated result before grading.
- After Python changes, Unreal must restart fully; otherwise it can keep an older widget callback in memory.

## First time in Unreal: install, open, generate, and play

Follow these steps in order. They assume the reader has never used Unreal before.

1. Install **Unreal Engine 5.8** through the Epic Games Launcher on Windows.
2. Clone this repository:

   ```powershell
   git clone https://github.com/elisedumaine/pfa-week06.git
   ```

3. Extract `InteractiveWorldBuilder_Project.zip` into a new folder named `InteractiveWorldBuilder`:

   ```powershell
   Expand-Archive .\InteractiveWorldBuilder_Project.zip -DestinationPath .\InteractiveWorldBuilder
   ```

4. Double-click `InteractiveWorldBuilder\InteractiveWorldBuilder.uproject`. If Windows asks which Unreal version to use, choose **5.8**. If Unreal asks to rebuild or enable project plugins, accept it.
5. Wait until Unreal finishes loading the Third Person level and the bottom-right shader/loading messages have stopped. You should see a 3D view in the middle of the editor.
6. In Unreal's top menu, click **Tools**, then click **Open Interactive World Builder**. A tool panel opens inside Unreal.
7. In the panel, type a number in **Seed** (for example `20261018`). Move the tree, flower, rock, grass, terrain, and lighting sliders. Leave **Playable Character** checked.
8. Click **Generate World** and wait for the new forest to appear in the 3D view. Do not click it repeatedly while Unreal is generating.

### Play and move in the generated world

1. At the top of Unreal, click the green triangle labelled **Play**. Make sure it says **Play**, not **Simulate**.
2. Click once inside the game window so it receives keyboard input.
3. Use **W**, **A**, **S**, and **D** to walk; move the mouse to look around; press **Spacebar** to jump.
4. Press **Esc** to stop playing and return to the editor.
5. To make a different world, change a slider or seed in the World Builder panel, click **Generate World** (or **Refresh Plants** for vegetation only), and press **Play** again.

### If something is missing

- The tool panel is not in the Content Drawer. Open it from **Tools -> Open Interactive World Builder**.
- If the panel has an older layout after editing the Python script, close every Unreal Editor window and reopen this project. Unreal keeps Editor Utility Widget callbacks in memory until restart.
- If a game window opens but the keyboard does nothing, click inside that game window once, then press **WASD**.
- If Play starts in an unexpected place, return with **Esc**, make sure **Playable Character** is checked, generate again, then press Play.

The archive includes the selected Kenney Nature Kit assets in `Content/WorldBuilder/Imported/Kenney`; no separate asset download is required.


## Design choice: depth before breadth

The first finished environment is a forest rather than four shallow themes. The seed makes a result reproducible, and the vegetation refresh keeps terrain stable while the artist iterates on density. Those choices answer the main production problem: make fast variations without creating duplicate actors or losing a playable level.

## Peer run - required before final submission

**Classmate:** _Pending peer test - add the classmate's real name before submitting._

1. _Sentence 1: What they did to install and run the tool._
2. _Sentence 2: What worked or did not work for them._
3. _Sentence 3: One suggestion or observation about the interface/world._

The peer feedback must be written by an actual classmate. Send me their name and exact three sentences after the run, and I will add them here without inventing feedback.

## Assets and credit

This project uses selected meshes from the **Kenney Nature Kit**, available under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Unreal Engine's Third Person template provides the test character and movement.

## Submission evidence

See [SUBMISSION_NOTES.md](SUBMISSION_NOTES.md) for the short demo plan and screenshots to capture before submission. The revised project pitch is in [MIDTERM_PITCH.md](MIDTERM_PITCH.md).

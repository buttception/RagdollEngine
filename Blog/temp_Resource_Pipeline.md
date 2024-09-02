there are resources in the engine lib
the engine lib do not know how to load data into their resources
application will provide the behaviour
engine lib only provide the resource type and actually using the data to do stuff
ie editor will deal with hot reloading and compiling of assets to be used as resource in the engine.

ALL RESOURCE MUST PROVIDE A ON EXE DEFAULT PLACEHOLDER FOR USE
flow in editor
## Start

1. **Scan the Asset Directory**
   - Traverse all folders in the asset directory and load filenames into an asset database.

2. **Check for Descriptors**
   - **If any files do not have a descriptor:**
     - Generate a new descriptor.
   - **Otherwise:**
     - Do nothing.

3. **Check if the Asset Needs to be Compiled**
   - **If the asset needs compilation:**
     - Check if the compiled object exists.
       - **If it does not exist:**
         - Queue it into the asset conditioning pipeline (send descriptor if it exists).
       - **If it exists:**
         - Check if there were any changes.
           - **If changes are detected:**
             - Queue it into the asset conditioning pipeline (send descriptor if it exists).
           - **Otherwise:**
             - Do nothing.
   - **If the asset does not need compilation:**
     - Load the object and update the descriptor with details.

4. **Check if the Asset Needs to be Cached Forever**
   - **If it needs to be cached forever:**
     - Do not unload it; load it into the resource manager.
   - **Otherwise:**
     - Unload it from memory.

## Runtime (new asset)
1. asset thread detects changes blah blah blah

## Runtime (require the resource, can be due to scene or preview in browser etc.)
### Require the Resource
1. **When loading a scene:**
   - Check which resources are needed in the scene.
     - **If a resource is not loaded:**
       - Load it.
     - **Otherwise:**
       - Do nothing.
   - **Note:** A placeholder will be shown while loading since file loading is asynchronous. Textures may cause freezes because they must be processed on the main thread.

### Preview
1. **If in the directory:**
   - Check if all assets in the directory are loaded.
     - **If not loaded:**
       - Load them.
     - **Otherwise:**
       - Do nothing.
2. **Once changing the directory:**
   - Unload assets from the previous directory.
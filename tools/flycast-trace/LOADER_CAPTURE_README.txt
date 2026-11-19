VF3 Dreamcast loader capture
============================

Purpose
-------
Capture the one-time resource-loading path that runs when a match is created.
This is a Dreamcast VF3 capture, not a Naomi boot test.

Before starting
---------------
1. Extract this ZIP into a normal writable folder.
2. Keep all files in the same folder.
3. Make sure you have your legally obtained Dreamcast VF3 game image ready.
4. Close any other Flycast instance.

Capture
-------
1. Double-click run-trace-loader.cmd.
2. In Flycast, load the Dreamcast version of Virtua Fighter 3.
3. Start from the beginning of the game; do not resume an already-running fight
   from a savestate.
4. Navigate through the title/attract screens into character selection.
5. Select/confirm the fighters and continue into Round 1.
6. Once the first playable round has started, close Flycast normally.

Important
---------
- Use the Dreamcast game image, not a Naomi probe ROM.
- Do not use a savestate from an already-running match: it skips the one-time
  character/stage resource-loading boundary we need to observe.
- The trace profile watches the known GDFS/MT loader entrypoints, the object
  table, and selected memory ranges. The output can be large.
- The launcher creates these files beside flycast.exe:
    vf3-loader-trace-loader.bin
    vf3-loader-trace-loader.bin.diag.txt
- Do not rename or edit either output file.

Send back
---------
Zip only the two output files above and upload that ZIP here or to the project
Dropbox folder. Do not include the game image or the entire Flycast folder.

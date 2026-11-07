Expressive Composer 0.1.0 — Windows x64

Contents
--------
Expressive Composer.vst3/   VST3 instrument (copy to C:\Program Files\Common Files\VST3\)
Expressive Composer.exe     Standalone test host
editor.png                  Actual editor snapshot (1200x800)
tools/                      ec_cli.exe, ec_tests.exe, ec_bench.exe
pluginval.log               pluginval v1.0.4 result (SUCCESS)

Install (VST3)
--------------
1. Close your DAW.
2. Copy the whole "Expressive Composer.vst3" folder to:
   C:\Program Files\Common Files\VST3\
3. Start the DAW, rescan VST3 plugins, insert "Expressive Composer" on an instrument track.
4. Press PLAY in the plugin for an internal audition, or start the host transport.
5. The plugin does not emit MIDI to the host in this version; use the MIDI button to export a .mid file.

Standalone
----------
Double-click "Expressive Composer.exe". It is mainly for testing the editor/audio path.

Known limits
------------
- Windows x64 only in this build. AU/CLAP not built.
- Real DAW compatibility was not tested; pluginval strictness 5 passed.
- No physical listening/blind test was performed.
- The editor is 1200x800 logical; on 200% scaling and a small logical desktop it may exceed the screen.
- License: AGPL-3.0 (JUCE AGPLv3/commercial dual license). See the repository LICENSE and DEPENDENCIES_AND_LICENSES.md.
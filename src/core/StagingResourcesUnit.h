// D-Fend Reborn: shaders, SoundFonts and MT-32 ROM folders DOSBox Staging
// can use (choices for the "DOSBox Staging" settings of the profile editors).
//
// The folders are the ones DOSBox Staging 0.83 searches (its manual: "Shaders",
// "FluidSynth" and "Roland MT-32"). The configuration folder of DOSBox Staging
// is the program folder if dosbox-staging.conf is there (portable layout, used
// for the included DOSBox Staging), otherwise the user configuration folder.
#pragma once

#include <QString>
#include <QStringList>

struct TStagingSearchDirs {
    QStringList Shaders;         // *.glsl ("<folder>/<path>/<name>.glsl" -> "path/name")
    QStringList ShaderPresets;   // "<folder>/<shader path>/<preset>.preset" -> "shader path:preset"
    QStringList SoundFonts;      // *.sf2, *.sf3
    QStringList MT32Roms;        // MT-32 / CM-32L ROM files
};

// Folders searched by the DOSBox Staging program DOSBoxExeFile (existing or not)
TStagingSearchDirs StagingSearchDirs(const QString& DOSBoxExeFile);

// Shader names for the "shader" setting: the adaptive CRT shaders and short
// aliases of DOSBox Staging, then the shaders found in ShaderDirs, each one
// followed by its presets found in PresetDirs
QStringList StagingShaderNames(const QStringList& ShaderDirs, const QStringList& PresetDirs);

// SoundFonts for the "soundfont" setting: file names of the SoundFonts directly
// in one of the folders (found by DOSBox Staging by name), paths of the ones
// in subfolders (relative to the D-Fend Reborn folder if possible)
QStringList StagingSoundFonts(const QStringList& Dirs);

// Folders for the "romdir" setting: the folders and their subfolders with ROM
// files (relative to the D-Fend Reborn folder if possible)
QStringList StagingMT32RomDirs(const QStringList& Dirs);

// Values found for StagingSettings[I] (shader, soundfont, romdir; empty list
// for the other settings) of the DOSBox installation of a profile ("default",
// name of an installation or folder / program file of DOSBox)
QStringList StagingSettingChoices(int I, const QString& DOSBoxInstallation);

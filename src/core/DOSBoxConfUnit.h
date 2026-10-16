// D-Fend Reborn: DOSBox conf files for DOSBox Staging and DOSBox-X
// The profiles store their settings with the names of DOSBox 0.74 (also the
// auto setup templates); the conf file for a game is built from the profile
// each time the game is started. For DOSBox Staging and DOSBox-X,
// ConvertConfForDOSBox converts the settings of this conf file to the names
// these DOSBox versions use; settings DOSBox Staging does not have any more
// are left out, so that it uses its own defaults for them. The profiles are
// not changed, so a profile can be switched between DOSBox versions.
#pragma once

#include <QStringList>
#include "CommonTools.h"   // TDOSBoxKind

// Lines: the lines of a DOSBox conf file. The [autoexec] section is not
// changed; settings of other sections that are unknown here (e.g. custom
// settings already using the new names) are kept unchanged. dkDOSBox: no
// changes.
QStringList ConvertConfForDOSBox(TDOSBoxKind Kind, const QStringList& Lines);

// true if a DOSBox setting (DOSBox 0.74 name, e.g. "render", "scaler") has
// no effect with this DOSBox version (it is left out of its conf file); used
// by the profile editor to grey out these settings.
bool DOSBoxSettingIgnored(TDOSBoxKind Kind, const QString& Section, const QString& Key);

// D-Fend Reborn: DOSBox conf files for DOSBox Staging
// D-Fend Reborn writes the settings of the profiles with the names of DOSBox
// 0.74. DOSBox Staging renamed, moved or removed many of them (and warns about
// each one when it starts). ConvertConfForDOSBoxStaging converts the settings
// of a conf file to the DOSBox Staging names; settings Staging does not have
// any more are left out, so that Staging uses its own defaults for them.
#pragma once

#include <QStringList>

// Lines: the lines of a DOSBox conf file. The [autoexec] section is not
// changed; settings of other sections that are unknown here (e.g. custom
// settings already using the DOSBox Staging names) are kept unchanged.
QStringList ConvertConfForDOSBoxStaging(const QStringList& Lines);

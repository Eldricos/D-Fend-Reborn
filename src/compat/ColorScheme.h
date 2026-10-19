// Light / dark colours of the program windows (D-Fend Reborn).
//
// The program follows the colour setting of the operating system (also when
// it changes while the program runs) or uses light or dark colours as set in
// the program options. Windows: the classic style ("windowsvista") cannot
// show dark windows, the Fusion style is used for dark colours.
#pragma once

#include <QColor>
#include <QString>
#include <functional>
#include "Graphics.h"

class QObject;

enum TColorScheme { csSystem = 0, csLight = 1, csDark = 2 };

// Applies the colour scheme (TColorScheme; csSystem: the setting of the
// operating system). Called at the program start and when the setting changes.
void ApplyColorScheme(int Scheme);

// Are the windows shown with dark colours?
bool DarkColorScheme();

// Fixed text colours of hints, warnings and links (Font.Color:=clRed, clBlue,
// ...): lighter with dark colours, so they stay readable. System colours
// (clWindowText, clGrayText, ...) follow the palette anyway.
QColor TextColor(TColor C);

// Style sheets with fixed light colours (e.g. "#F0FBFF" of the info panels):
// the colours for the current scheme
QString ColorSchemeStyleSheet(const QString& LightStyleSheet);

// Handler is called after the operating system switched between light and
// dark colours while the program runs (Context: handler removed when it is
// destroyed)
void AddColorSchemeChangedHandler(QObject* Context, std::function<void()> Handler);

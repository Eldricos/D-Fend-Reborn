#include "ColorScheme.h"

#include <QApplication>
#include <QPalette>
#include <QPointer>
#include <QStyle>
#include <QStyleHints>
#include <QTimer>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

int FScheme = -1;
bool FDark = false;
bool FInApply = false;
QString FLightStyle;      // style for light colours (the style set at the program start)
QPalette FStartPalette;   // palette of the desktop at the program start (Qt < 6.5)
QList<QPair<QPointer<QObject>, std::function<void()>>> FHandlers;

// Fixed light colours of style sheets and their dark counterparts
const QList<QPair<QString, QString>>& StyleSheetColors()
{
    static const QList<QPair<QString, QString>> L = {
        {QStringLiteral("#F0FBFF"), QStringLiteral("#1C2A33")},   // info panels (light blue)
    };
    return L;
}

bool SystemDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme S = QGuiApplication::styleHints()->colorScheme();
    if (S != Qt::ColorScheme::Unknown) return S == Qt::ColorScheme::Dark;
#endif
    // older Qt or unknown: the colours of the desktop (e.g. a dark KDE Plasma colour scheme)
    return FStartPalette.color(QPalette::Window).lightness() < 128;
}

bool PaletteDark(const QPalette& P)
{
    return P.color(QPalette::Window).lightness() < 128;
}

QPalette DarkPalette()
{
    const QColor Window(53, 53, 53), Base(35, 35, 35), Text(230, 230, 230), Disabled(127, 127, 127);
    const QColor Highlight(42, 130, 218);
    QPalette P;
    P.setColor(QPalette::Window, Window);
    P.setColor(QPalette::WindowText, Text);
    P.setColor(QPalette::Base, Base);
    P.setColor(QPalette::AlternateBase, Window);
    P.setColor(QPalette::ToolTipBase, Base);
    P.setColor(QPalette::ToolTipText, Text);
    P.setColor(QPalette::PlaceholderText, Disabled);
    P.setColor(QPalette::Text, Text);
    P.setColor(QPalette::Button, Window);
    P.setColor(QPalette::ButtonText, Text);
    P.setColor(QPalette::BrightText, Qt::red);
    P.setColor(QPalette::Light, Window.lighter(150));
    P.setColor(QPalette::Midlight, Window.lighter(125));
    P.setColor(QPalette::Mid, Window.darker(120));
    P.setColor(QPalette::Dark, Window.darker(150));
    P.setColor(QPalette::Shadow, Qt::black);
    P.setColor(QPalette::Link, QColor(110, 170, 255));
    P.setColor(QPalette::LinkVisited, QColor(170, 140, 255));
    P.setColor(QPalette::Highlight, Highlight);
    P.setColor(QPalette::HighlightedText, Qt::white);
    for (QPalette::ColorRole R : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText})
        P.setColor(QPalette::Disabled, R, Disabled);
    P.setColor(QPalette::Disabled, QPalette::Highlight, Window.lighter(130));
    P.setColor(QPalette::Disabled, QPalette::Base, Window);
    return P;
}

#ifdef Q_OS_WIN
// Title bars of the open windows: light or dark. Windows draws the title bar
// again only when the window is activated / deactivated or changes its size,
// so both are simulated here (as other programs do, e.g. Notepad++).
void UpdateTitleBars()
{
    for (QWidget* W : QApplication::topLevelWidgets()) {
        if (!W->testAttribute(Qt::WA_WState_Created) || !W->isVisible()) continue;
        const HWND H = reinterpret_cast<HWND>(W->winId());
        const BOOL Dark = FDark ? TRUE : FALSE;
        DwmSetWindowAttribute(H, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &Dark, sizeof(Dark));
        // caption drawn as inactive and then again in its real state
        const bool Active = (GetActiveWindow() == H);
        SendMessageW(H, WM_NCACTIVATE, Active ? FALSE : TRUE, 0);
        SendMessageW(H, WM_NCACTIVATE, Active ? TRUE : FALSE, 0);
        // size changed by one pixel and back (normal windows; maximized ones are drawn again by the frame change)
        RECT R;
        if (!IsZoomed(H) && !IsIconic(H) && GetWindowRect(H, &R)) {
            const UINT F = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
            SetWindowPos(H, nullptr, 0, 0, R.right - R.left + 1, R.bottom - R.top, F);
            SetWindowPos(H, nullptr, 0, 0, R.right - R.left, R.bottom - R.top, F);
        }
        SetWindowPos(H, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        RedrawWindow(H, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    }
}
#endif

void UpdateStyleSheets()
{
    for (QWidget* W : QApplication::allWidgets()) {
        const QVariant L = W->property("vclLightStyleSheet");
        if (L.isValid()) W->setStyleSheet(ColorSchemeStyleSheet(L.toString()));
    }
}

void NotifyHandlers()
{
    for (int I = FHandlers.size() - 1; I >= 0; I--) if (FHandlers[I].first.isNull()) FHandlers.removeAt(I);
    const auto Handlers = FHandlers;
    for (const auto& H : Handlers) if (!H.first.isNull()) H.second();
}

void Apply()
{
    FInApply = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // (csSystem: the setting of the operating system; also used for the window frames)
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif
    const bool Dark = (FScheme == csDark) || ((FScheme == csSystem) && SystemDark());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (FScheme != csSystem) QGuiApplication::styleHints()->setColorScheme(Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif

#ifdef Q_OS_WIN
    // the classic Windows style has no dark colours
    const QString Style = Dark ? QStringLiteral("Fusion") : FLightStyle;
    if (QApplication::style()->name().compare(Style, Qt::CaseInsensitive) != 0) QApplication::setStyle(Style);
#endif

    // the colours of the desktop, if they fit; otherwise light or dark colours of the style / program
    QApplication::setPalette(QPalette());
    if (Dark && !PaletteDark(QApplication::palette())) QApplication::setPalette(DarkPalette());
    if (!Dark && PaletteDark(QApplication::palette())) QApplication::setPalette(QApplication::style()->standardPalette());

    FDark = Dark;
    UpdateStyleSheets();
#ifdef Q_OS_WIN
    // (after Qt and Windows have finished their own updates of the windows)
    QTimer::singleShot(150, qApp, [] { UpdateTitleBars(); });
#endif
    FInApply = false;
}

}  // namespace

void ApplyColorScheme(int Scheme)
{
    if ((Scheme < csSystem) || (Scheme > csDark)) Scheme = csSystem;
    if (FScheme < 0) {
        // program start
        FLightStyle = QApplication::style()->name();
        FStartPalette = QApplication::palette();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, [] {
            // (the operating system switched between light and dark colours)
            if (FInApply || (FScheme != csSystem)) return;
            Apply();
            QTimer::singleShot(0, qApp, [] { NotifyHandlers(); });
        });
#endif
    } else if (Scheme == FScheme) {
        return;
    }
    FScheme = Scheme;
    Apply();
}

bool DarkColorScheme()
{
    return FDark;
}

QColor TextColor(TColor C)
{
    QColor Q = TColorToQColor(C);
    if (!FDark || !Q.isValid() || (C < 0)) return Q;
    if (Q.lightnessF() < 0.65) {
        float H, S, L, A;
        Q.getHslF(&H, &S, &L, &A);
        Q.setHslF(H, S, 0.7f, A);
    }
    return Q;
}

QString ColorSchemeStyleSheet(const QString& LightStyleSheet)
{
    if (!FDark) return LightStyleSheet;
    QString S = LightStyleSheet;
    for (const auto& C : StyleSheetColors()) S.replace(C.first, C.second, Qt::CaseInsensitive);
    return S;
}

void AddColorSchemeChangedHandler(QObject* Context, std::function<void()> Handler)
{
    FHandlers.append({QPointer<QObject>(Context), std::move(Handler)});
}

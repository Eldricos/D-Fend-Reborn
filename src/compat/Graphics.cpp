#include "Graphics.h"
#include "SysUtils.h"
#include "VclLayout.h"

#include <QApplication>
#include <QPalette>
#include <QFile>
#include <QHash>

QColor TColorToQColor(TColor C)
{
    if (C == clNone || C == clDefault) return QColor();
    if (C < 0) {
        QPalette P = QApplication::palette();
        switch (C & 0xFF) {
        case 0x04: return P.color(QPalette::Window);
        case 0x05: return P.color(QPalette::Base);
        case 0x07: return P.color(QPalette::WindowText);
        case 0x08: return P.color(QPalette::Text);
        case 0x09: return P.color(QPalette::WindowText);
        case 0x0D: return P.color(QPalette::Highlight);
        case 0x0E: return P.color(QPalette::HighlightedText);
        case 0x0F: return P.color(QPalette::Button);
        case 0x10: return P.color(QPalette::Mid);
        case 0x11: return P.color(QPalette::Disabled, QPalette::Text);
        case 0x12: return P.color(QPalette::ButtonText);
        case 0x14: return P.color(QPalette::Light);
        case 0x15: return P.color(QPalette::Shadow);
        case 0x16: return P.color(QPalette::Midlight);
        case 0x17: return P.color(QPalette::ToolTipText);
        case 0x18: return P.color(QPalette::ToolTipBase);
        case 0x1A: return P.color(QPalette::Link);
        default: return P.color(QPalette::Window);
        }
    }
    return QColor(C & 0xFF, (C >> 8) & 0xFF, (C >> 16) & 0xFF);
}

TColor QColorToTColor(const QColor &C)
{
    if (!C.isValid()) return clNone;
    return RGBToColor(C.red(), C.green(), C.blue());
}

static const QList<QPair<QString, TColor>> &colorNames()
{
    static const QList<QPair<QString, TColor>> L = {
        {"clBlack", clBlack}, {"clMaroon", clMaroon}, {"clGreen", clGreen}, {"clOlive", clOlive},
        {"clNavy", clNavy}, {"clPurple", clPurple}, {"clTeal", clTeal}, {"clGray", clGray},
        {"clSilver", clSilver}, {"clRed", clRed}, {"clLime", clLime}, {"clYellow", clYellow},
        {"clBlue", clBlue}, {"clFuchsia", clFuchsia}, {"clAqua", clAqua}, {"clWhite", clWhite},
        {"clMoneyGreen", clMoneyGreen}, {"clSkyBlue", clSkyBlue}, {"clCream", clCream}, {"clMedGray", clMedGray},
        {"clScrollBar", clScrollBar}, {"clBackground", clBackground}, {"clActiveCaption", clActiveCaption},
        {"clInactiveCaption", clInactiveCaption}, {"clMenu", clMenu}, {"clWindow", clWindow},
        {"clWindowFrame", clWindowFrame}, {"clMenuText", clMenuText}, {"clWindowText", clWindowText},
        {"clCaptionText", clCaptionText}, {"clActiveBorder", clActiveBorder}, {"clInactiveBorder", clInactiveBorder},
        {"clAppWorkSpace", clAppWorkSpace}, {"clHighlight", clHighlight}, {"clHighlightText", clHighlightText},
        {"clBtnFace", clBtnFace}, {"clBtnShadow", clBtnShadow}, {"clGrayText", clGrayText},
        {"clBtnText", clBtnText}, {"clInactiveCaptionText", clInactiveCaptionText}, {"clBtnHighlight", clBtnHighlight},
        {"cl3DDkShadow", cl3DDkShadow}, {"cl3DLight", cl3DLight}, {"clInfoText", clInfoText},
        {"clInfoBk", clInfoBk}, {"clHotLight", clHotLight}, {"clNone", clNone}, {"clDefault", clDefault},
    };
    return L;
}

QString ColorToString(TColor C)
{
    for (const auto &P : colorNames()) if (P.second == C) return P.first;
    return "$" + QString::number(static_cast<quint32>(C), 16).toUpper().rightJustified(8, '0');
}

bool TryStringToColor(const QString &S, TColor &C)
{
    QString T = S.trimmed();
    for (const auto &P : colorNames())
        if (QString::compare(P.first, T, Qt::CaseInsensitive) == 0) { C = P.second; return true; }
    bool Ok = false;
    qint64 V;
    if (T.startsWith('$')) V = T.mid(1).toLongLong(&Ok, 16);
    else if (T.startsWith("0x", Qt::CaseInsensitive)) V = T.mid(2).toLongLong(&Ok, 16);
    else V = T.toLongLong(&Ok, 10);
    if (!Ok) return false;
    C = static_cast<TColor>(static_cast<quint32>(V));
    return true;
}

TColor StringToColor(const QString &S)
{
    TColor C;
    if (!TryStringToColor(S, C)) throw EConvertError(QString("'%1' is not a valid color").arg(S));
    return C;
}

TImageList *TImageList::FromResource(const QString &Unit, const QString &Name)
{
    auto *L = new TImageList();
    bool First = true;
    for (int i = 0;; i++) {
        QString P = QString(":/imagelists/%1/%2/%3.png").arg(Unit, Name).arg(i);
        if (!QFile::exists(P)) break;
        QPixmap Px(P);
        if (First) { L->SetSize(Px.width(), Px.height()); First = false; }
        L->Add(Px);
    }
    return L;
}

int TImageList::Add(const QPixmap &P)
{
    FImages.append(P);
    return FImages.size() - 1;
}

int TImageList::AddIcon(const QIcon &I)
{
    return Add(I.pixmap(FWidth, FHeight));
}

void TImageList::Replace(int Index, const QPixmap &P)
{
    if (Index >= 0 && Index < FImages.size()) FImages[Index] = P;
}

void TImageList::Delete(int Index)
{
    if (Index >= 0 && Index < FImages.size()) FImages.removeAt(Index);
}

QIcon TImageList::Icon(int Index) const
{
    if (Index < 0 || Index >= FImages.size()) return QIcon();
    return QIcon(FImages[Index]);
}

QPixmap TImageList::Pixmap(int Index) const
{
    if (Index < 0 || Index >= FImages.size()) return QPixmap();
    return FImages[Index];
}

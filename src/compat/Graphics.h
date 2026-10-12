// Delphi Graphics/ImgList compatibility: TColor and TImageList.
#pragma once

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>

// Delphi TColor: $00BBGGRR or a negative system color constant.
using TColor = int;
constexpr TColor clNone = 0x1FFFFFFF;
constexpr TColor clDefault = 0x20000000;
constexpr TColor clBlack = 0x000000, clMaroon = 0x000080, clGreen = 0x008000, clOlive = 0x008080,
                 clNavy = 0x800000, clPurple = 0x800080, clTeal = 0x808000, clGray = 0x808080,
                 clSilver = 0xC0C0C0, clRed = 0x0000FF, clLime = 0x00FF00, clYellow = 0x00FFFF,
                 clBlue = 0xFF0000, clFuchsia = 0xFF00FF, clAqua = 0xFFFF00, clWhite = 0xFFFFFF,
                 clMoneyGreen = 0xC0DCC0, clSkyBlue = 0xF0CAA6, clCream = 0xF0FBFF, clMedGray = 0xA4A0A0;
// System colors (COLOR_xxx or $FF000000)
constexpr TColor clScrollBar = int(0xFF000000), clBackground = int(0xFF000001), clActiveCaption = int(0xFF000002),
                 clInactiveCaption = int(0xFF000003), clMenu = int(0xFF000004), clWindow = int(0xFF000005),
                 clWindowFrame = int(0xFF000006), clMenuText = int(0xFF000007), clWindowText = int(0xFF000008),
                 clCaptionText = int(0xFF000009), clActiveBorder = int(0xFF00000A), clInactiveBorder = int(0xFF00000B),
                 clAppWorkSpace = int(0xFF00000C), clHighlight = int(0xFF00000D), clHighlightText = int(0xFF00000E),
                 clBtnFace = int(0xFF00000F), clBtnShadow = int(0xFF000010), clGrayText = int(0xFF000011),
                 clBtnText = int(0xFF000012), clInactiveCaptionText = int(0xFF000013), clBtnHighlight = int(0xFF000014),
                 cl3DDkShadow = int(0xFF000015), cl3DLight = int(0xFF000016), clInfoText = int(0xFF000017),
                 clInfoBk = int(0xFF000018), clHotLight = int(0xFF00001A);

QColor TColorToQColor(TColor C);
TColor QColorToTColor(const QColor &C);
QString ColorToString(TColor C);                 // "clRed" or "$00FF8040"
TColor StringToColor(const QString &S);           // throws EConvertError on invalid input
bool TryStringToColor(const QString &S, TColor &C);
inline TColor RGBToColor(int R, int G, int B) { return (R & 0xFF) | ((G & 0xFF) << 8) | ((B & 0xFF) << 16); }

// TImageList: list of equally sized images (icons) referenced by index.
class TImageList
{
public:
    TImageList(int Width = 16, int Height = 16) : FWidth(Width), FHeight(Height) {}
    // Image list exported from a DFM by dfm2ui ("MainUnit", "ImageList")
    static TImageList *FromResource(const QString &Unit, const QString &Name);

    int Width() const { return FWidth; }
    int Height() const { return FHeight; }
    void SetWidth(int W) { FWidth = W; }
    void SetHeight(int H) { FHeight = H; }
    void SetSize(int W, int H) { FWidth = W; FHeight = H; }
    int Count() const { return FImages.size(); }
    int Add(const QPixmap &P);
    int AddIcon(const QIcon &I);
    void Replace(int Index, const QPixmap &P);
    void Delete(int Index);
    void Clear() { FImages.clear(); }
    QIcon Icon(int Index) const;
    QPixmap Pixmap(int Index) const;
    void Assign(const TImageList &Other) { *this = Other; }
private:
    int FWidth, FHeight;
    QList<QPixmap> FImages;
};

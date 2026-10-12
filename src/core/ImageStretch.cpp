// C++ port of ImageStretch.pas (implementation section).
#include "ImageStretch.h"

#include "PrgSetupUnit.h"

#include <QPainter>
#include <cmath>

// PrgSetup.ImageFilter: 0..8, 0=off, 1=simple&slow, 2=Box, 3=Triangle, 4=Hermite, 5=Bell, 6=B-Spline, 7=Lanczos3, 8=Mitchell

static void SimpleStretch(const QImage& ASource, QImage& Dest)
{
    const QImage Source = ASource.convertToFormat(QImage::Format_ARGB32);
    const int W = Source.width();
    const int H = Source.height();
    if (W <= 0 || H <= 0 || Dest.width() <= 0 || Dest.height() <= 0) return;

    QList<quint8> C0(W * H), C1(W * H), C2(W * H), C3(W * H);
    for (int I = 0; I < W; I++) for (int J = 0; J < H; J++) {
        QRgb C = Source.pixel(I, J);
        C3[H * I + J] = quint8(qRed(C));
        C2[H * I + J] = quint8(qGreen(C));
        C1[H * I + J] = quint8(qBlue(C));
        C0[H * I + J] = quint8(qAlpha(C));
    }

    const double F = double(W) / Dest.width();
    for (int I = 0; I < Dest.width(); I++) for (int J = 0; J < Dest.height(); J++) {
        double X = I * F, Y = J * F;
        int K0 = Min(Round(X), W - 1), L0 = Min(Round(Y), H - 1);
        int K1 = Min(K0 + 1, W - 1), L1 = Min(L0 + 1, H - 1);
        double XVec[2], YVec[2];
        XVec[1] = X - std::floor(X); XVec[0] = 1 - XVec[1]; YVec[1] = Y - std::floor(Y); YVec[0] = 1 - YVec[1];
        auto Mix = [&](const QList<quint8> &C) {
            return qBound(0, Round(C[K0 * H + L0] * XVec[0] * YVec[0] + C[K1 * H + L0] * XVec[1] * YVec[0] + C[K0 * H + L1] * XVec[0] * YVec[1] + C[K1 * H + L1] * XVec[1] * YVec[1]), 255);
        };
        int NewC1 = Mix(C1), NewC2 = Mix(C2), NewC3 = Mix(C3), NewC0 = Mix(C0);
        Dest.setPixel(I, J, qRgba(NewC3, NewC2, NewC1, NewC0));
    }
}

// Scales Source to W x H using the filter setting of the Delphi code
static QImage StretchTo(const QImage& Source, int W, int H, int UseFilter, bool SimpleStretchAllowed)
{
    if (W <= 0 || H <= 0) return QImage();
    if ((UseFilter < 1) || (UseFilter > 7)) {
        if (SimpleStretchAllowed && (UseFilter == 0)) {
            QImage result(W, H, QImage::Format_ARGB32);
            result.fill(Qt::black);
            SimpleStretch(Source, result);
            return result;
        }
        // Canvas.StretchDraw
        return Source.scaled(W, H, Qt::IgnoreAspectRatio, Qt::FastTransformation).convertToFormat(QImage::Format_ARGB32);
    }
    // TODO(port): the Resample filters (Box, Triangle, Hermite, Bell, B-Spline,
    // Lanczos3, Mitchell) are all replaced by Qt's smooth (bilinear) scaling.
    return Source.scaled(W, H, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
}

QImage ScaleImage(const QPixmap& SourcePic, double Factor)
{
    int UseFilter = PrgSetup->ImageFilter() - 1;

    QImage Source = SourcePic.toImage();
    int W = Round(SourcePic.width() * Factor);
    int H = Round(SourcePic.height() * Factor);
    return StretchTo(Source, W, H, UseFilter, Factor < 1);
}

void ScaleImage(const QImage& Source, QImage& Dest, int DestW, int DestH, bool UseFiltering)
{
    int UseFilter;
    if (UseFiltering) UseFilter = PrgSetup->ImageFilter() - 1; else UseFilter = 0;

    if (Dest.isNull()) {
        Dest = QImage(DestW, DestH, QImage::Format_ARGB32);
        Dest.fill(Qt::transparent);
    }
    if (Source.isNull()) return;

    const int TempW = Min(DestW, Dest.width());
    const int TempH = Min(DestH, Dest.height());
    QImage TempDest = StretchTo(Source, TempW, TempH, UseFilter, TempW < Source.width());
    if (TempDest.isNull()) return;

    int W = Dest.width() - TempDest.width();
    int H = Dest.height() - TempDest.height();

    if (Dest.format() == QImage::Format_Indexed8 || Dest.format() == QImage::Format_Mono || Dest.format() == QImage::Format_MonoLSB)
        Dest = Dest.convertToFormat(QImage::Format_ARGB32);
    QPainter P(&Dest);
    P.setCompositionMode(QPainter::CompositionMode_Source);
    P.drawImage(QRect(W / 2, H / 2, Dest.width() - (W / 2) - (W / 2), Dest.height() - (H / 2) - (H / 2)), TempDest);
    P.end();
}

void ScaleImage(const QImage& Source, QImage& Dest, bool UseFiltering)
{
    ScaleImage(Source, Dest, Dest.width(), Dest.height(), UseFiltering);
}

void ScaleImage(const QPixmap& Source, QImage& Dest, bool UseFiltering)
{
    QImage Temp = Source.toImage();
    ScaleImage(Temp, Dest, UseFiltering);
}

void ScaleImage(const QIcon& Source, QImage& Dest, bool UseFiltering)
{
    // Temp.SetSize(Source.Width,Source.Height): the largest image of the icon
    QList<QSize> Sizes = Source.availableSizes();
    QSize S(32, 32);
    for (const QSize &Z : Sizes) if (Z.width() * Z.height() > S.width() * S.height()) S = Z;
    QImage Temp = Source.pixmap(S).toImage();
    ScaleImage(Temp, Dest, UseFiltering);
}

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

// -----------------------------------------------------------------------------
// Resample filters (port of Resample.pas, "Interpolated Bitmap Resampling using
// filters" by Anders Melander, as used by the original program)
// -----------------------------------------------------------------------------

using TFilterProc = float (*)(float Value);

// Hermite filter
static float HermiteFilter(float Value)
{
    // f(t) = 2|t|^3 - 3|t|^2 + 1, -1 <= t <= 1
    if (Value < 0.0f) Value = -Value;
    if (Value < 1.0f) return (2.0f * Value - 3.0f) * Value * Value + 1.0f;
    return 0.0f;
}

// Box filter (a.k.a. "Nearest Neighbour" filter)
static float BoxFilter(float Value)
{
    if ((Value > -0.5f) && (Value <= 0.5f)) return 1.0f;
    return 0.0f;
}

// Triangle filter (a.k.a. "Linear" or "Bilinear" filter)
static float TriangleFilter(float Value)
{
    if (Value < 0.0f) Value = -Value;
    if (Value < 1.0f) return 1.0f - Value;
    return 0.0f;
}

// Bell filter
static float BellFilter(float Value)
{
    if (Value < 0.0f) Value = -Value;
    if (Value < 0.5f) return 0.75f - Value * Value;
    if (Value < 1.5f) { Value = Value - 1.5f; return 0.5f * Value * Value; }
    return 0.0f;
}

// B-spline filter
static float SplineFilter(float Value)
{
    if (Value < 0.0f) Value = -Value;
    if (Value < 1.0f) { const float tt = Value * Value; return 0.5f * tt * Value - tt + 2.0f / 3.0f; }
    if (Value < 2.0f) { Value = 2.0f - Value; return 1.0f / 6.0f * Value * Value * Value; }
    return 0.0f;
}

// Lanczos3 filter
static float Lanczos3Filter(float Value)
{
    auto SinC = [](float V) -> float {
        if (V != 0.0f) { V = V * 3.14159265358979323846f; return std::sin(V) / V; }
        return 1.0f;
    };
    if (Value < 0.0f) Value = -Value;
    if (Value < 3.0f) return SinC(Value) * SinC(Value / 3.0f);
    return 0.0f;
}

// Mitchell filter
static float MitchellFilter(float Value)
{
    const float B = 1.0f / 3.0f;
    const float C = 1.0f / 3.0f;
    if (Value < 0.0f) Value = -Value;
    const float tt = Value * Value;
    if (Value < 1.0f) {
        Value = ((12.0f - 9.0f * B - 6.0f * C) * (Value * tt)) + ((-18.0f + 12.0f * B + 6.0f * C) * tt) + (6.0f - 2.0f * B);
        return Value / 6.0f;
    }
    if (Value < 2.0f) {
        Value = ((-1.0f * B - 6.0f * C) * (Value * tt)) + ((6.0f * B + 30.0f * C) * tt) + ((-12.0f * B - 48.0f * C) * Value) + (8.0f * B + 24.0f * C);
        return Value / 6.0f;
    }
    return 0.0f;
}

struct TResampleFilter { const char *Name; TFilterProc Filter; float Width; };

static const TResampleFilter ResampleFilters[7] = {
    {"Box", BoxFilter, 0.5f},
    {"Triangle", TriangleFilter, 1.0f},
    {"Hermite", HermiteFilter, 1.0f},
    {"Bell", BellFilter, 1.5f},
    {"B-Spline", SplineFilter, 2.0f},
    {"Lanczos3", Lanczos3Filter, 3.0f},
    {"Mitchell", MitchellFilter, 2.0f},
};

// Source pixels contributing to a destination pixel
struct TContributor { int Pixel; float Weight; };

// Pre-calculates the filter contributions for a row or a column
static QList<QList<TContributor>> Contributions(int SrcSize, int DstSize, TFilterProc Filter, float FWidth)
{
    // Improvement suggested by David Ullrich (as in Resample.pas)
    float Scale;
    if (SrcSize == 1 || DstSize == 1) Scale = float(DstSize) / SrcSize; else Scale = float(DstSize - 1) / (SrcSize - 1);

    // sub-sampling (bigger to smaller): the filter is widened
    const bool SubSampling = (Scale < 1.0f);
    const float Width = SubSampling ? FWidth / Scale : FWidth;
    const float FScale = SubSampling ? 1.0f / Scale : 1.0f;

    QList<QList<TContributor>> Result(DstSize);
    for (int I = 0; I < DstSize; I++) {
        QList<TContributor>& C = Result[I];
        const float Center = I / Scale;
        const int Left = int(std::floor(Center - Width));
        const int Right = int(std::ceil(Center + Width));
        for (int J = Left; J <= Right; J++) {
            const float Weight = Filter((Center - J) / FScale) / FScale;
            if (Weight == 0.0f) continue;
            // mirrored at the borders
            int N;
            if (J < 0) N = -J; else if (J >= SrcSize) N = SrcSize - J + SrcSize - 1; else N = J;
            N = qBound(0, N, SrcSize - 1);   // (very small images)
            C.append({N, Weight});
        }
        // Port note: the weights are normalized (sum = 1); Resample.pas used
        // them as they were, which made images slightly darker or lighter
        // (and would make opaque images partly transparent here)
        float Sum = 0.0f;
        for (const TContributor& X : C) Sum += X.Weight;
        if (Sum != 0.0f && Sum != 1.0f) for (TContributor& X : C) X.Weight /= Sum;
    }
    return Result;
}

// Strecth (Resample.pas): scales Src to DstW x DstH, first horizontally, then
// vertically.
// Port note: the Delphi code worked on 24 bit RGB bitmaps; here all four
// channels of premultiplied ARGB pixels are filtered, so transparent images
// (icons) are scaled correctly, too.
static QImage Strecth(const QImage& ASrc, int DstW, int DstH, TFilterProc Filter, float FWidth)
{
    const QImage Src = ASrc.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int SrcW = Src.width(), SrcH = Src.height();
    if (SrcW < 1 || SrcH < 1 || DstW < 1 || DstH < 1) return QImage();

    // a premultiplied color channel can not be bigger than the alpha channel
    auto Pixel = [](const float RGB[4]) -> QRgb {
        int A = qBound(0, int(std::lround(RGB[3])), 255);
        auto Ch = [A](float V) { return qBound(0, int(std::lround(V)), A); };
        return qRgba(Ch(RGB[0]), Ch(RGB[1]), Ch(RGB[2]), A);
    };

    // horizontal pass: Src -> Work (DstW x SrcH)
    QImage Work(DstW, SrcH, QImage::Format_ARGB32_Premultiplied);
    {
        const QList<QList<TContributor>> Contrib = Contributions(SrcW, DstW, Filter, FWidth);
        for (int K = 0; K < SrcH; K++) {
            const QRgb *SourceLine = reinterpret_cast<const QRgb *>(Src.constScanLine(K));
            QRgb *DestLine = reinterpret_cast<QRgb *>(Work.scanLine(K));
            for (int I = 0; I < DstW; I++) {
                float RGB[4] = {0, 0, 0, 0};
                for (const TContributor& C : Contrib[I]) {
                    const QRgb Color = SourceLine[C.Pixel];
                    RGB[0] += qRed(Color) * C.Weight;
                    RGB[1] += qGreen(Color) * C.Weight;
                    RGB[2] += qBlue(Color) * C.Weight;
                    RGB[3] += qAlpha(Color) * C.Weight;
                }
                DestLine[I] = Pixel(RGB);
            }
        }
    }

    // vertical pass: Work -> Dst
    QImage Dst(DstW, DstH, QImage::Format_ARGB32_Premultiplied);
    {
        const QList<QList<TContributor>> Contrib = Contributions(SrcH, DstH, Filter, FWidth);
        for (int I = 0; I < DstH; I++) {
            QRgb *DestLine = reinterpret_cast<QRgb *>(Dst.scanLine(I));
            for (int K = 0; K < DstW; K++) {
                float RGB[4] = {0, 0, 0, 0};
                for (const TContributor& C : Contrib[I]) {
                    const QRgb Color = reinterpret_cast<const QRgb *>(Work.constScanLine(C.Pixel))[K];
                    RGB[0] += qRed(Color) * C.Weight;
                    RGB[1] += qGreen(Color) * C.Weight;
                    RGB[2] += qBlue(Color) * C.Weight;
                    RGB[3] += qAlpha(Color) * C.Weight;
                }
                DestLine[K] = Pixel(RGB);
            }
        }
    }
    return Dst.convertToFormat(QImage::Format_ARGB32);
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
    const TResampleFilter& F = ResampleFilters[Min(6, Max(0, UseFilter - 1))];
    return Strecth(Source, W, H, F.Filter, F.Width);
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

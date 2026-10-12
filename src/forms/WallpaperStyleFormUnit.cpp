// C++ port of WallpaperStyleFormUnit.pas (implementation section).
#include "WallpaperStyleFormUnit.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"

TWallpaperStyleForm* WallpaperStyleForm = nullptr;

TWallpaperStyleForm::TWallpaperStyleForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);
}

TWallpaperStyleForm::~TWallpaperStyleForm()
{
}

void TWallpaperStyleForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed in Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->WallpaperCaption());
    ui.TileRadioButton->setText(LanguageSetup->WallpaperTile());
    ui.CenterRadioButton->setText(LanguageSetup->WallpaperCenter());
    ui.StretchRadioButton->setText(LanguageSetup->WallpaperStretch());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
}

void TWallpaperStyleForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender); Q_UNUSED(Key); Q_UNUSED(Shift);
}

// global

bool ShowWallpaperStyleDialog(QWidget* AOwner, TWallpaperStyle& WPStyle)
{
    bool result;
    WallpaperStyleForm = new TWallpaperStyleForm(AOwner);
    result = (WallpaperStyleForm->ShowModal() == mrOk);
    if (result) {
        if (WallpaperStyleForm->ui.TileRadioButton->isChecked()) WPStyle = WSTile;
        if (WallpaperStyleForm->ui.CenterRadioButton->isChecked()) WPStyle = WSCenter;
        if (WallpaperStyleForm->ui.StretchRadioButton->isChecked()) WPStyle = WSStretch;
    }
    delete WallpaperStyleForm;
    WallpaperStyleForm = nullptr;
    return result;
}

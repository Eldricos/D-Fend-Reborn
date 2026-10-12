// C++ port of CheatDBInternalPrivateerPositionUnit.pas (implementation section).
#include "CheatDBInternalPrivateerPositionUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"

TCheatDBInternalPrivateerPositionForm* CheatDBInternalPrivateerPositionForm = nullptr;

static const int StarCount = 59;

struct TStar {
    const char* Name;
    quint8 pos;
};

static const TStar Planets[StarCount] = {
    {"Aldebran  Matahari(Pleasure)", 0x19},
    {"Auriga  Beaconsfield(Refinery)", 0x04},
    {"Auriga  Elysia(Agricular)", 0x0B},
    {"Capella  -(Pirates)", 0x08},
    {"Castor  Romulus(Mining)", 0x2D},
    {"DN-N1921  N1912-1(Pleasure)", 0x1E},
    {"Hind's Variable N  Meadow(Refinery)", 0x1A},
    {"Hind's Variable N  Oresville(Agricular)", 0x26},
    {"Hyades  Charon(Mining", 0x07},
    {"Junction  Burton(Agricular)", 0x06},
    {"Junction  Speke(Pleasure)", 0x32},
    {"Junction  Victoria(Agricular)", 0x38},
    {"KM252  -(Pirates)", 0x31},
    {"Lisacc  Lisacc(Mining)", 0x15},
    {"Manchester  Thisbury(Refinery)", 0x34},
    {"Manchester  Wickerton(Refinery)", 0x3A},
    {"Midgard  Heimdel(Agricular)", 0x10},
    {"ND-57  New Reno(Pleasure)", 0x22},
    {"New Caledonia  Edinburgh(Refinery)", 0x09},
    {"New Caledonia  Glasgow(Refinery)", 0x0D},
    {"Newcastle  Liverpool(Refinery)", 0x16},
    {"New Constantinople  Edam(Agricular)", 0x0A},
    {"New Constantinople  New Const.(Base)", 0x1F},
    {"New Detroit  New Detroit", 0x20},
    {"Nexus  Macabee(Mining)", 0x17},
    {"Nitir  Nitir(Agricular)", 0x23},
    {"Oxford  Oxford(Base)", 0x27},
    {"Padre  Magdalina(Pleasure)", 0x18},
    {"Palan  Basra(Refinery)", 0x03},
    {"Palan  Palan", 0x28},
    {"Pentonville  -(Pirates)", 0x24},
    {"Perry  Anapolis(Refinery)", 0x01},
    {"Perry  Perry Naval(Base))", 0x29},
    {"Pollux  Remus(Refinery)", 0x2A},
    {"Prosepe  Savatov(Mining)", 0x2F},
    {"Pyrenees  Basque(Mining)", 0x02},
    {"Pyrenees  New Iberia(Agricular)", 0x21},
    {"Ragnarok  Mjolnar(Agricular)", 0x1C},
    {"Raxis  Gracchus(Refinery)", 0x0E},
    {"Raxis  Trinsic(Agricular)", 0x35},
    {"Regallis  Kronecker(Mining)", 0x14},
    {"Rikel  Siva(Agricular)", 0x30},
    {"Rikel  Vishnu(Mining)", 0x39},
    {"Rygannon  Rygannon(Mining)", 0x2E},
    {"Saxtogue  Olympus(Pleasure)", 0x25},
    {"Shangri-La  Erwhon(Pleasure)", 0x0C},
    {"Sherwood  -(Pirates)", 0x36},
    {"Surtur  Surtur(Agricular)", 0x33},
    {"Telar  -(Pirates)", 0x1B},
    {"Tingerhoff  Bodensee(Agricular)", 0x05},
    {"Tingerhoff  Munchen(Refinery)", 0x1D},
    {"Troy  Achilles(Mining)", 0x00},
    {"Troy  Hector(Mining)", 0x0F},
    {"Troy  Helen(Agricular)", 0x11},
    {"Valhalla  Valkyrie(Mining)", 0x37},
    {"Varnus  Rodin(Agricular)", 0x2C},
    {"Varnus  Rilke(Refinery)", 0x2B},
    {"XXN-1927  Jalson(Pleasure)", 0x12},
    {"XXN-1927  Joplin(Refinery)", 0x13}};

TCheatDBInternalPrivateerPositionForm::TCheatDBInternalPrivateerPositionForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    FormCreate(this);  // OnCreate
}

TCheatDBInternalPrivateerPositionForm::~TCheatDBInternalPrivateerPositionForm() = default;

void TCheatDBInternalPrivateerPositionForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=CharsetNameToFontCharSet(...) - not needed with Qt (Unicode fonts)

    setWindowTitle(LanguageSetup->EditCheatsInternalPrivateerPosition());
    ui.PositionLabel->setText(LanguageSetup->EditCheatsInternalPrivateerPositionPrompt());

    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    for (int I = 0; I < StarCount; I++) ui.PositionComboBox->addItem(QString::fromLatin1(Planets[I].Name), int(Planets[I].pos));
    ui.PositionComboBox->setCurrentIndex(0);
}

/* global */

bool ShowCheatDBInternalPrivateerPositionDialog(QWidget* AOwner, quint8& APosition)
{
    bool result;
    CheatDBInternalPrivateerPositionForm = new TCheatDBInternalPrivateerPositionForm(AOwner);
    for (int I = 0; I < CheatDBInternalPrivateerPositionForm->ui.PositionComboBox->count(); I++)
        if (CheatDBInternalPrivateerPositionForm->ui.PositionComboBox->itemData(I).toInt() == APosition) {
            CheatDBInternalPrivateerPositionForm->ui.PositionComboBox->setCurrentIndex(I);
            break;
        }
    result = (CheatDBInternalPrivateerPositionForm->ShowModal() == mrOk) && (CheatDBInternalPrivateerPositionForm->ui.PositionComboBox->currentIndex() >= 0);
    // Note: the original code returns the ItemIndex (not the stored position value) - kept as is.
    if (result) APosition = quint8(CheatDBInternalPrivateerPositionForm->ui.PositionComboBox->currentIndex());
    delete CheatDBInternalPrivateerPositionForm;
    CheatDBInternalPrivateerPositionForm = nullptr;
    return result;
}

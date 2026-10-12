// C++ port of ProfileMountEditorFormUnit.pas (implementation section).
#include "ProfileMountEditorFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "ProfileMountEditorDriveFrameUnit.h"
#include "ProfileMountEditorFloppyDriveFrameUnit.h"
#include "ProfileMountEditorCDDriveFrameUnit.h"
#include "ProfileMountEditorFloppyImage1FrameUnit.h"
#include "ProfileMountEditorFloppyImage2FrameUnit.h"
#include "ProfileMountEditorCDImageFrameUnit.h"
#include "ProfileMountEditorHDImageFrameUnit.h"
#include "ProfileMountEditorPhysFSFrameUnit.h"
#include "ProfileMountEditorZipFrameUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TProfileMountEditorForm* ProfileMountEditorForm = nullptr;

TProfileMountEditorForm::TProfileMountEditorForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // The frames are placed into MainPanel (Align=alClient); a layout replaces the VCL align engine here.
    auto *L = new QVBoxLayout(ui.MainPanel);
    L->setContentsMargins(0, 0, 0, 0);
    L->setSpacing(0);

    // DFM event wiring
    connect(ui.TypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { TypeComboBoxChange(ui.TypeComboBox); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return false;
    });

    FormCreate(this);
}

TProfileMountEditorForm::~TProfileMountEditorForm()
{
}

void TProfileMountEditorForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TProfileMountEditorForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered, SetVistaFonts and Font.Charset are not needed with Qt.

    UsedDriveLetters = "";

    ui.TypeLabel->setText(LanguageSetup->ProfileMountingDriveType() + ":");
    setWindowTitle(LanguageSetup->ProfileMounting());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TProfileMountEditorForm::AddFrame(QWidget* Frame, TProfileMountEditorFrame* FrameI)
{
    TInfoData InfoData;

    // Frame.DoubleBuffered / SetVistaFonts / Font.Charset: not needed with Qt.

    // Frame.Parent:=MainPanel; Frame.Align:=alClient; Frame.Visible:=False;
    Frame->setParent(ui.MainPanel);
    Frame->setProperty("vclAlign", "alClient");
    ui.MainPanel->layout()->addWidget(Frame);
    Frame->setVisible(false);

    int I = FrameInfoRec.size();
    TFrameInfoRec R;
    R.Frame = Frame;
    R.FrameI = FrameI;
    FrameInfoRec.append(R);

    InfoData.Data = Data;
    InfoData.UsedDriveLetters = UsedDriveLetters;
    InfoData.CloseRequest = [this](QObject *S) { CloseRequest(S); };
    InfoData.BaseGameFrame = BaseGameFrame;
    if (ExtUpperCase(Copy(DefaultInitialDir, 1, 7)) == "DOSBOX:")
        InfoData.DefaultInitialDir = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
    else
        InfoData.DefaultInitialDir = MakeAbsPath(DefaultInitialDir, PrgSetup->BaseDir());
    if (!DirectoryExists(InfoData.DefaultInitialDir)) InfoData.DefaultInitialDir = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
    InfoData.ProfileFileName = ProfileFileName;
    InfoData.GameDB = GameDB;

    if (FrameI->Init(InfoData)) LastVisible = I;

    // TypeComboBox.Items.AddObject(FrameI.GetName,TObject(I));
    ui.TypeComboBox->blockSignals(true);
    ui.TypeComboBox->addItem(FrameI->GetName(), I);
    ui.TypeComboBox->blockSignals(false);
}

void TProfileMountEditorForm::FormShow(QObject* Sender)
{
    TStringList* St;
    int I;

    // general: RealFolder;Type;Letter;IO;Label;FreeSpace
    St = ValueToList(Data);
    if (St->Count() < 2) {
        St->Add("");
        St->Add("Drive");
        Data = ListToValue(St);
    }
    delete St;

    LastVisible = -1;

    { auto *F = new TProfileMountEditorDriveFrame(ui.MainPanel); AddFrame(F, F); }
    { auto *F = new TProfileMountEditorFloppyDriveFrame(ui.MainPanel); AddFrame(F, F); }
    { auto *F = new TProfileMountEditorCDDriveFrame(ui.MainPanel); AddFrame(F, F); }
    if (PrgSetup->AllowMultiFloppyImagesMount()) {
        auto *F = new TProfileMountEditorFloppyImage2Frame(ui.MainPanel); AddFrame(F, F);
    } else {
        auto *F = new TProfileMountEditorFloppyImage1Frame(ui.MainPanel); AddFrame(F, F);
    }
    { auto *F = new TProfileMountEditorCDImageFrame(ui.MainPanel); AddFrame(F, F); }
    { auto *F = new TProfileMountEditorHDImageFrame(ui.MainPanel); AddFrame(F, F); }
    { auto *F = new TProfileMountEditorZipFrame(ui.MainPanel); AddFrame(F, F); }
    if (PrgSetup->AllowPhysFSUsage() || (LastVisible == -1)) {
        auto *F = new TProfileMountEditorPhysFSFrame(ui.MainPanel); AddFrame(F, F);
    }

    I = LastVisible; LastVisible = -1;
    ui.TypeComboBox->blockSignals(true);
    ui.TypeComboBox->setCurrentIndex(I);
    ui.TypeComboBox->blockSignals(false);
    TypeComboBoxChange(Sender);
}

void TProfileMountEditorForm::CloseRequest(QObject* Sender)
{
    setModalResult(mrOk);
    OKButtonClick(Sender);
}

void TProfileMountEditorForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;

    if (LastVisible < 0) { setModalResult(mrNone); return; }
    if (!FrameInfoRec[LastVisible].FrameI->CheckBeforeDone()) { setModalResult(mrNone); return; }
    S = FrameInfoRec[LastVisible].FrameI->Done();
    if (S == "") { setModalResult(mrNone); return; }
    Data = S;
}

void TProfileMountEditorForm::TypeComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastVisible >= 0) {
        FrameInfoRec[LastVisible].Frame->setVisible(false);
    }

    LastVisible = ui.TypeComboBox->currentIndex();

    if (ui.TypeComboBox->currentIndex() >= 0) {
        FrameInfoRec[ui.TypeComboBox->currentIndex()].Frame->setVisible(true);
        FrameInfoRec[ui.TypeComboBox->currentIndex()].FrameI->ShowFrame();
    }
}

void TProfileMountEditorForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ApplicationHelpCommand(HELP_CONTEXT, ID_ProfileEditDrives);
}

void TProfileMountEditorForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowProfileMountEditorDialog(QWidget* AOwner, QString& AData, const QString& AUsedDriveLetters, const QString& ADefaultInitialDir, const QString& AProfileFileName, TGameDB* AGameDB, TModernProfileEditorBaseFrame* Frame, const QString& NextFreeDriveLetter)
{
    QString S;
    bool result;

    if (AData == "") {
        S = Trim(PrgSetup->LastAddedDriveType()); if (S == "") S = "Drive";
        AData = ";" + S + ";" + NextFreeDriveLetter + ";";
    }
    ProfileMountEditorForm = new TProfileMountEditorForm(AOwner);
    ProfileMountEditorForm->Data = AData;
    ProfileMountEditorForm->BaseGameFrame = Frame;
    if (Trim(ADefaultInitialDir) != "\\")
        ProfileMountEditorForm->DefaultInitialDir = ADefaultInitialDir;
    else
        ProfileMountEditorForm->DefaultInitialDir = PrgSetup->GameDir();
    ProfileMountEditorForm->UsedDriveLetters = AUsedDriveLetters;
    ProfileMountEditorForm->ProfileFileName = AProfileFileName;
    // Note: the original code never assigns AGameDB to the form (GameDB stays nil).
    Q_UNUSED(AGameDB);
    result = (ProfileMountEditorForm->ShowModal() == mrOk);
    if (result) {
        AData = ProfileMountEditorForm->Data;
        S = Trim(AData);
        if (Pos(';', S) > 0) {
            S = Trim(Copy(S, Pos(';', S) + 1, MaxInt));
            if (Pos(';', S) > 0) S = Trim(Copy(S, 1, Pos(';', S) - 1));
            PrgSetup->SetLastAddedDriveType(S);
        }
    }
    delete ProfileMountEditorForm;
    ProfileMountEditorForm = nullptr;
    return result;
}

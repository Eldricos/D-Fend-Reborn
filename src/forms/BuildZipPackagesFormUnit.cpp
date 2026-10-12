// C++ port of BuildZipPackagesFormUnit.pas (implementation section).
#include "BuildZipPackagesFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "GameDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "ZipPackageUnit.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"

TBuildZipPackagesForm *BuildZipPackagesForm = nullptr;

static constexpr quint16 VK_F1_Code = 0x70;   // VK_F1

TBuildZipPackagesForm::TBuildZipPackagesForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.DestFolderButton, &QToolButton::clicked, this, [this]() { DestFolderButtonClick(ui.DestFolderButton); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        // Delphi virtual key codes (only VK_F1 is evaluated)
        quint16 K = (Key == Qt::Key_F1) ? VK_F1_Code : quint16(Key & 0xFFFF);
        FormKeyDown(this, K, Mods);
        return (Key == Qt::Key_F1) && (Mods == Qt::NoModifier);
    });
    FormCreate(this);   // OnCreate
}

TBuildZipPackagesForm::~TBuildZipPackagesForm() = default;

void TBuildZipPackagesForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    if (!FShown) { FShown = true; FormShow(this); }
}

void TBuildZipPackagesForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->BuildZipPackages());

    ui.InfoLabel->setText(LanguageSetup->BuildZipPackagesInfo());
    ui.DestFolderEdit_EditLabel->setText(LanguageSetup->BuildZipPackagesDestFolder());
    ui.DestFolderButton->setToolTip(LanguageSetup->ChooseFile());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());
    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    UserIconLoader->DialogImage(DI_SelectFolder, ui.DestFolderButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TBuildZipPackagesForm::FormShow(QObject * /*Sender*/)
{
    ui.DestFolderEdit->setText(IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY)));

    BuildCheckList(ui.ListBox, GameDB, false, false, true);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject *S) { SelectButtonClick(S); }, false, true);
}

void TBuildZipPackagesForm::SelectButtonClick(QObject *Sender)
{
    if (qobject_cast<QPushButton *>(Sender) != nullptr) {
        const int Tag = Sender->property("tag").toInt();
        switch (Tag) {
            case 0: case 1:
                for (int I = 0; I < ui.ListBox->count(); I++) ui.ListBox->item(I)->setCheckState((Tag == 0) ? Qt::Checked : Qt::Unchecked);
                break;
            case 2: {
                QPoint P = mapToGlobal(QPoint(ui.SelectGenreButton->x(), ui.SelectGenreButton->y()));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
            }
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

void TBuildZipPackagesForm::DestFolderButtonClick(QObject * /*Sender*/)
{
    QString S = MakeAbsPath(ui.DestFolderEdit->text(), PrgSetup->BaseDir());
    if (!SelectDirectory(THandle(0), LanguageSetup->ChooseFolder(), S)) return;
    ui.DestFolderEdit->setText(MakeRelPath(S, PrgSetup->BaseDir(), true));
}

void TBuildZipPackagesForm::OKButtonClick(QObject * /*Sender*/)
{
    QString Dir = IncludeTrailingPathDelimiter(MakeAbsPath(ui.DestFolderEdit->text(), PrgSetup->BaseDir()));
    if (!ForceDirectories(Dir)) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Dir}), mtError, mbOK, 0);
        setModalResult(mrNone);
        return;
    }

    setEnabled(false);
    for (int I = 0; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() == Qt::Checked) {
        auto *G = static_cast<TGame *>(ui.ListBox->item(I)->data(Qt::UserRole).value<void *>());
        BuildZipPackage(this, G, Dir + ChangeFileExt(ExtractFileName(G->SetupFile()), ".zip"), false);
    }
    setEnabled(true);
}

void TBuildZipPackagesForm::HelpButtonClick(QObject * /*Sender*/)
{
    ApplicationHelpCommand(HELP_CONTEXT, ID_FileExportBuildZipPackages);
}

void TBuildZipPackagesForm::FormKeyDown(QObject *Sender, quint16 &Key, TShiftState Shift)
{
    if ((Key == VK_F1_Code) && (Shift == Qt::NoModifier)) HelpButtonClick(Sender);
}

// global

bool ShowBuildZipPackagesDialog(QWidget *AOwner, TGameDB *AGameDB)
{
    auto *BuildZipPackagesForm = new TBuildZipPackagesForm(AOwner);
    BuildZipPackagesForm->GameDB = AGameDB;
    bool result = (BuildZipPackagesForm->ShowModal() == mrOk);
    delete BuildZipPackagesForm;
    return result;
}

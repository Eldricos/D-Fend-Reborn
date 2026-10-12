// C++ port of CreateShortcutsFormUnit.pas (implementation section).
#include "CreateShortcutsFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "PrgSetupUnit.h"
#include "GameDBToolsUnit.h"
#include "CreateShortcutFormUnit.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TCreateShortcutsForm* CreateShortcutsForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

TCreateShortcutsForm::TCreateShortcutsForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.SelectFolderButton, &QToolButton::clicked, this, [this]() { SelectFolderButtonClick(ui.SelectFolderButton); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { OKButtonClick(ui.OKButton); });
    connect(ui.HelpButton, &QPushButton::clicked, this, [this]() { HelpButtonClick(ui.HelpButton); });
    connect(ui.StartmenuEdit, &QLineEdit::textChanged, this, [this]() { StartmenuEditChange(ui.StartmenuEdit); });
    connect(ui.SelectAllButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectAllButton); });
    connect(ui.SelectNoneButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectNoneButton); });
    connect(ui.SelectGenreButton, &QPushButton::clicked, this, [this]() { SelectButtonClick(ui.SelectGenreButton); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TCreateShortcutsForm::~TCreateShortcutsForm() = default;

void TCreateShortcutsForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TCreateShortcutsForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // DoubleBuffered:=True; SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->CreateShortcutsCaption());

    ui.InfoLabel->setText(LanguageSetup->CreateShortcutsInfo());
    ui.FolderEdit_EditLabel->setText(LanguageSetup->CreateShortcutsInfoWine());
    ui.SelectFolderButton->setToolTip(LanguageSetup->ChooseFolder());

    ui.SelectAllButton->setText(LanguageSetup->All());
    ui.SelectNoneButton->setText(LanguageSetup->None());
    ui.SelectGenreButton->setText(LanguageSetup->GameBy());
    ui.DesktopRadioButton->setText(LanguageSetup->CreateShortcutFormDesktop());
    ui.StartmenuRadioButton->setText(LanguageSetup->CreateShortcutFormStartmenu());
    ui.UseProfileIconCheckBox->setText(LanguageSetup->CreateShortcutsUseProfileIcons());
    ui.OKButton->setText(LanguageSetup->OK());
    ui.CancelButton->setText(LanguageSetup->Cancel());
    ui.HelpButton->setText(LanguageSetup->Help());

    if (WineSupportEnabled() && PrgSetup->LinuxLinkMode()) {
        ui.DesktopRadioButton->setVisible(false);
        ui.StartmenuRadioButton->setVisible(false);
        ui.StartmenuEdit->setVisible(false);
        ui.UseProfileIconCheckBox->setVisible(false);
        ui.FolderEdit->setVisible(true);
        ui.FolderEdit_EditLabel->setVisible(true);
        ui.SelectFolderButton->setVisible(true);
        const int T = ui.FolderEdit->y() + ui.FolderEdit->height() + 15;
        VclLayout::setBounds(ui.OKButton, ui.OKButton->x(), T, ui.OKButton->width(), ui.OKButton->height());
        VclLayout::setBounds(ui.CancelButton, ui.CancelButton->x(), T, ui.CancelButton->width(), ui.CancelButton->height());
        VclLayout::setBounds(ui.HelpButton, ui.HelpButton->x(), T, ui.HelpButton->width(), ui.HelpButton->height());
        VclLayout::setClientSize(this, width(), ui.OKButton->y() + ui.OKButton->height() + 10);
    }

    ui.FolderEdit->setText(GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY));

    UserIconLoader->DialogImage(DI_SelectFolder, ui.SelectFolderButton);
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);
    UserIconLoader->DialogImage(DI_Help, ui.HelpButton);
}

void TCreateShortcutsForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    BuildCheckList(ui.ListBox, GameDB, true, false, false);
    BuildSelectPopupMenu(ui.PopupMenu, GameDB, [this](QObject* S) { SelectButtonClick(S); }, true, true);
}

void TCreateShortcutsForm::SelectButtonClick(QObject* Sender)
{
    int I;
    QPoint P;

    if (qobject_cast<QPushButton*>(Sender) != nullptr) {
        switch (Sender->property("tag").toInt()) {
            case 0:
            case 1:
                for (I = 0; I < ui.ListBox->count(); I++)
                    ui.ListBox->item(I)->setCheckState((Sender->property("tag").toInt() == 0) ? Qt::Checked : Qt::Unchecked);
                break;
            case 2:
                P = mapToGlobal(QPoint(ui.SelectGenreButton->x(), ui.SelectGenreButton->y()));
                ui.PopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
                break;
        }
        return;
    }

    SelectGamesByPopupMenu(Sender, ui.ListBox);
}

void TCreateShortcutsForm::StartmenuEditChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.StartmenuRadioButton->setChecked(true);
}

void TCreateShortcutsForm::SelectFolderButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    S = ui.FolderEdit->text(); if (S == "") S = PrgDataDir();
    if (SelectDirectory(THandle(winId()), LanguageSetup->SetupFormBaseDir(), S)) ui.FolderEdit->setText(S);
}

void TCreateShortcutsForm::OKButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    TGame* G;
    QString Dir, LinkFile, Icon;

    for (I = 0; I < ui.ListBox->count(); I++) if (ui.ListBox->item(I)->checkState() == Qt::Checked) {
        G = static_cast<TGame*>(ui.ListBox->item(I)->data(Qt::UserRole).value<void*>());
        if (WineSupportEnabled() && PrgSetup->LinuxLinkMode()) {
            CreateLinuxShortCut(this, G);
        } else {

            if (ui.UseProfileIconCheckBox->isChecked() && (G->Icon() != "") && FileExists(MakeAbsIconName(G->Icon()))) Icon = MakeAbsIconName(G->Icon()); else Icon = "";

            if (ui.DesktopRadioButton->isChecked()) {
                Dir = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_DESKTOPDIRECTORY));
            } else {
                Dir = IncludeTrailingPathDelimiter(GetSpecialFolder(winId(), CSIDL_PROGRAMS));
                Dir = Dir + ui.StartmenuEdit->text() + PathDelim;
                if (!ForceDirectories(Dir)) {
                    MessageDlg(Format(LanguageSetup->MessageCouldNotCreateDir(), {Dir}), mtError, mbOK, 0, this);
                    return;
                }
            }
            LinkFile = Dir + MakeFileSysOKFolderName(G->Name()) + ".lnk";

            if (WindowsExeMode(G)) {
                CreateLink(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir()), G->GameParameters(), LinkFile, Icon, Format(LanguageSetup->CreateShortcutFormRunGameInScummVM(), {G->Name()}));
            } else {
                if (ScummVMMode(G)) {
                    CreateLink(ExpandFileName(QCoreApplication::applicationFilePath()), G->Name(), LinkFile, Icon, Format(LanguageSetup->CreateShortcutFormRunGameInScummVM(), {G->Name()}));
                } else {
                    CreateLink(ExpandFileName(QCoreApplication::applicationFilePath()), G->Name(), LinkFile, Icon, Format(LanguageSetup->CreateShortcutFormRunGameInDOSBox(), {G->Name()}));
                }
            }
        }
    }
}

void TCreateShortcutsForm::HelpButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    HelpCommandContext(ID_ExtrasCreateShortcuts);
}

void TCreateShortcutsForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpButtonClick(Sender); Key = 0; }
}

/* global */

bool ShowCreateShortcutsDialog(QWidget* AOwner, TGameDB* AGameDB)
{
    bool result;
    CreateShortcutsForm = new TCreateShortcutsForm(AOwner);
    CreateShortcutsForm->GameDB = AGameDB;
    result = (CreateShortcutsForm->ShowModal() == mrOk);
    delete CreateShortcutsForm;
    CreateShortcutsForm = nullptr;
    return result;
}

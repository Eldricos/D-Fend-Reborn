// C++ port of ProviderFormUnit.pas (implementation section).
#include "ProviderFormUnit.h"
#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TProviderForm* ProviderForm = nullptr;

static void SetFontStyle(QWidget* W, bool Bold, bool Underline)
{
    QFont F = W->font();
    F.setBold(Bold);
    F.setUnderline(Underline);
    W->setFont(F);
}

TProviderForm::TProviderForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    ui.ProviderURLLabel->installEventFilter(this);   // ProviderURLLabel.OnClick
    FormCreate(this);   // OnCreate
}

TProviderForm::~TProviderForm()
{
}

void TProviderForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

bool TProviderForm::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Watched == ui.ProviderURLLabel && Event->type() == QEvent::MouseButtonRelease) {
        auto* ME = static_cast<QMouseEvent*>(Event);
        if (ME->button() == Qt::LeftButton && ui.ProviderURLLabel->rect().contains(ME->position().toPoint())) ProviderURLLabelClick(ui.ProviderURLLabel);
    }
    return TVclForm::eventFilter(Watched, Event);
}

void TProviderForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts / Font.Charset: not needed with Qt

    setWindowTitle(LanguageSetup->PackageManagerProviderCaption());
    ui.Label1->setText(LanguageSetup->PackageManagerProviderFiles());
    ui.Label2->setText(LanguageSetup->PackageManagerProviderProvider());
    ui.Label3->setText(LanguageSetup->PackageManagerProviderInformation());
    SetFontStyle(ui.Label3, false, true);
    ui.Label4->setText(LanguageSetup->PackageManagerProviderHomepage());
    SetFontStyle(ui.Label4, false, true);

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
}

void TProviderForm::FormShow(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (Files != nullptr) for (int I = 0; I < Files->Count(); I++) ui.ListBox->addItem((*Files)[I]);
    ui.ProviderNameLabel->setText(ProviderName);
    SetFontStyle(ui.ProviderNameLabel, true, false);
    ui.ProviderTextLabel->setText(ProviderText);
    ui.ProviderURLLabel->setText(ProviderURL);
    ui.ProviderURLLabel->setForegroundRole(QPalette::Link);   // Font.Color:=clBlue
    SetFontStyle(ui.ProviderURLLabel, false, true);
    ui.ProviderURLLabel->setCursor(Qt::PointingHandCursor);
}

void TProviderForm::ProviderURLLabelClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ShellOpen(ui.ProviderURLLabel->text());
}

/* global */

void ShowProviderDialog(QWidget* AOwner, TStringList* AFiles, const QString& AProviderName, const QString& AProviderText, const QString& AProviderURL)
{
    ProviderForm = new TProviderForm(AOwner);
    ProviderForm->Files = AFiles;
    ProviderForm->ProviderName = AProviderName;
    ProviderForm->ProviderText = AProviderText;
    ProviderForm->ProviderURL = AProviderURL;
    ProviderForm->ShowModal();
    delete ProviderForm;
    ProviderForm = nullptr;
}

void WriteProverInfoToProfile(TGame* AProfile, const QString& AProviderName, const QString& AProviderURL)
{
    TStringList* St = StringToStringList(AProfile->Notes());
    St->Insert(0, Format(LanguageSetup->PackageManagerProviderProfileString(), {AProviderName}));
    if (St->Count() > 1) St->Insert(1, "");
    AProfile->SetNotes(StringListToString(St));
    delete St;
    if (!Trim(AProviderURL).isEmpty()) for (int I = 1; I <= 9; I++) if ((Trim(AProfile->WWW(I)).isEmpty()) || (I == 9)) {
        AProfile->SetWWWName(I, LanguageSetup->PackageManagerProviderHomepage());
        AProfile->SetWWW(I, AProviderURL);
        break;
    }
    AProfile->LoadCache();
    AProfile->StoreAllValues();
}

void WriteProverInfoToProfile(const QString& AProfileFile, const QString& AProviderName, const QString& AProviderURL)
{
    TGame* Game = new TGame(AProfileFile);
    WriteProverInfoToProfile(Game, AProviderName, AProviderURL);
    delete Game;
}

void WriteMetaInformationToProfile(TGame* AProfile, TDownloadAutoSetupData* ADownloadData)
{
    if (!Trim(ADownloadData->Name()).isEmpty()) AProfile->SetName(ADownloadData->Name());
    if (!Trim(ADownloadData->Genre()).isEmpty()) AProfile->SetGenre(ADownloadData->Genre());
    if (!Trim(ADownloadData->Developer()).isEmpty()) AProfile->SetDeveloper(ADownloadData->Developer());
    if (!Trim(ADownloadData->Publisher()).isEmpty()) AProfile->SetPublisher(ADownloadData->Publisher());
    if (!Trim(ADownloadData->Year()).isEmpty()) AProfile->SetYear(ADownloadData->Year());
    if (!Trim(ADownloadData->Language()).isEmpty()) AProfile->SetLanguage(ADownloadData->Language());
    AProfile->LoadCache();
    AProfile->StoreAllValues();
}

void WriteMetaInformationToProfile(const QString& AProfileFile, TDownloadAutoSetupData* ADownloadData)
{
    TGame* Game = new TGame(AProfileFile);
    WriteMetaInformationToProfile(Game, ADownloadData);
    delete Game;
}

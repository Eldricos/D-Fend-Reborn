// C++ port of ZipWaitInfoFormUnit.pas (implementation section).
#include "ZipWaitInfoFormUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "ZipManagerUnit.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"

TZipWaitInfoForm *ZipWaitInfoForm = nullptr;

void ShowZipWaitInfoDialog(QWidget *AOwner)
{
    ZipWaitInfoForm = new TZipWaitInfoForm(AOwner);
    ZipWaitInfoForm->ShowModal();
    delete ZipWaitInfoForm;
    ZipWaitInfoForm = nullptr;
}

TZipWaitInfoForm::TZipWaitInfoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    ImageList = TImageList::FromResource("ZipWaitInfoFormUnit", "ImageList");
    FormCreate(this);
}

TZipWaitInfoForm::~TZipWaitInfoForm()
{
    // Make sure the original notify event is restored
    if (SaveInfoBarNotify) {
        ZipManager->SetOnInfoBarNotify(SaveInfoBarNotify);
        SaveInfoBarNotify = nullptr;
    }
    delete ImageList;
}

void TZipWaitInfoForm::showEvent(QShowEvent *E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TZipWaitInfoForm::hideEvent(QHideEvent *E)
{
    TVclForm::hideEvent(E);
    FormHide(this);
}

void TZipWaitInfoForm::FormCreate(QObject * /*Sender*/)
{
    // SetVistaFonts / Font.Charset: not needed with Qt

    setWindowTitle(LanguageSetup->ZipWaitInfoForm());
    ui.InfoLabel->setText(LanguageSetup->ZipWaitInfoFormInfo());
    ui.CloseButton->setText(LanguageSetup->Close());

    UserIconLoader->DialogImage(DI_ImageHD, ImageList, 1);
    UserIconLoader->DialogImage(DI_ZipFile, ImageList, 2);
    UserIconLoader->DialogImage(DI_SelectFolder, ImageList, 3);
    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);

    SaveInfoBarNotify = nullptr;
}

void TZipWaitInfoForm::FormShow(QObject * /*Sender*/)
{
    if (!SaveInfoBarNotify) {
        SaveInfoBarNotify = ZipManager->OnInfoBarNotify();
        if (!SaveInfoBarNotify) SaveInfoBarNotify = [](QObject *) {};   // mark as saved (Delphi stores nil but keeps state via Assigned check on the method pointer)
        ZipManager->SetOnInfoBarNotify([this](QObject *S) { InfoBarEvent(S); });
    }
    UpdateTree();
}

void TZipWaitInfoForm::FormHide(QObject * /*Sender*/)
{
    if (SaveInfoBarNotify) {
        ZipManager->SetOnInfoBarNotify(SaveInfoBarNotify);
        SaveInfoBarNotify = nullptr;
    }
}

void TZipWaitInfoForm::InfoBarEvent(QObject *Sender)
{
    UpdateTree();
    if (SaveInfoBarNotify) SaveInfoBarNotify(Sender);

    if (ui.Tree->topLevelItemCount() == 0) Close();
}

void TZipWaitInfoForm::UpdateTree()
{
    QTreeWidget *Tree = ui.Tree;
    Tree->setUpdatesEnabled(false);
    Tree->clear();
    for (int I = 0; I < ZipManager->Count(); I++) {
        TZipRecord R = ZipManager->Data(I);
        QString S, T;

        if (R.IsScummVM) {
            S = LanguageSetup->ZipWaitInfoFormDriveScummVM();
            T = LanguageSetup->ZipWaitInfoFormScummVM();
        } else {
            S = LanguageSetup->ZipWaitInfoFormDriveDOSBox();
            T = LanguageSetup->ZipWaitInfoFormDOSBox();
        }

        auto *N = new QTreeWidgetItem(Tree, QStringList{T + ": $" + IntToHex(R.DOSBoxHandle, 8)});
        N->setIcon(0, ImageList->Icon(0));
        for (int J = 0; J < R.ZipDrives; J++) {
            auto *N2 = new QTreeWidgetItem(N, QStringList{S + " " + R.DriveLetter[J]});
            N2->setIcon(0, ImageList->Icon(1));
            auto *N3 = new QTreeWidgetItem(N2, QStringList{LanguageSetup->ZipWaitInfoFormZipFile() + ": " + MakeRelPath(R.ZipFile[J], PrgSetup->BaseDir())});
            N3->setIcon(0, ImageList->Icon(2));
            N3 = new QTreeWidgetItem(N2, QStringList{LanguageSetup->ZipWaitInfoFormFolder() + ": " + MakeRelPath(R.Folder[J], PrgSetup->BaseDir(), true)});
            N3->setIcon(0, ImageList->Icon(3));
        }
    }
    Tree->expandAll();
    Tree->setUpdatesEnabled(true);
}

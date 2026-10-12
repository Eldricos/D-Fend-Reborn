// C++ port of ModernProfileEditorAddtionalChecksumFrameUnit.pas (implementation section).
#include "ModernProfileEditorAddtionalChecksumFrameUnit.h"

#include "CommonTools.h"
#include "LanguageSetupUnit.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "HashCalc.h"

// NoFlicker (VistaToolsUnit) is not needed in the Qt port.

TModernProfileEditorAddtionalChecksumFrame::TModernProfileEditorAddtionalChecksumFrame(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    OpenDialog = new TOpenDialog(this);  // DFM: OpenDialog: TOpenDialog

    // DFM wiring
    for (QToolButton* B : {ui.SelectButton1, ui.DeleteButton1, ui.SelectButton2, ui.DeleteButton2, ui.SelectButton3, ui.DeleteButton3,
                           ui.SelectButton4, ui.DeleteButton4, ui.SelectButton5, ui.DeleteButton5})
        connect(B, &QToolButton::clicked, this, [this, B]() { ButtonWork(B); });
    connect(ui.AddAllButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.AddAllButton); });
    connect(ui.ClearAllButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.ClearAllButton); });
}

TModernProfileEditorAddtionalChecksumFrame::~TModernProfileEditorAddtionalChecksumFrame()
{
    delete OpenDialog;
}

// { TModernProfileEditorAddtionalChecksumFrame }

void TModernProfileEditorAddtionalChecksumFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    Q_UNUSED(InitData);

    ui.FilenameLabel->setText(LanguageSetup->ProfileEditorAdditionalChecksumsFiles());
    ui.ChecksumLabel->setText(LanguageSetup->ProfileEditorAdditionalChecksumsChecksums());
    ui.SelectButton1->setToolTip(LanguageSetup->ChooseFile());
    ui.SelectButton2->setToolTip(LanguageSetup->ChooseFile());
    ui.SelectButton3->setToolTip(LanguageSetup->ChooseFile());
    ui.SelectButton4->setToolTip(LanguageSetup->ChooseFile());
    ui.SelectButton5->setToolTip(LanguageSetup->ChooseFile());
    ui.DeleteButton1->setToolTip(LanguageSetup->ProfileEditorAdditionalChecksumsDelete());
    ui.DeleteButton2->setToolTip(LanguageSetup->ProfileEditorAdditionalChecksumsDelete());
    ui.DeleteButton3->setToolTip(LanguageSetup->ProfileEditorAdditionalChecksumsDelete());
    ui.DeleteButton4->setToolTip(LanguageSetup->ProfileEditorAdditionalChecksumsDelete());
    ui.DeleteButton5->setToolTip(LanguageSetup->ProfileEditorAdditionalChecksumsDelete());
    ui.AddAllButton->setText(LanguageSetup->ProfileEditorAdditionalChecksumsAddAll());
    ui.ClearAllButton->setText(LanguageSetup->ProfileEditorAdditionalChecksumsDeleteAll());
    OpenDialog->Title = LanguageSetup->ChooseFile();
    OpenDialog->Filter = LanguageSetup->ProfileEditorAdditionalChecksumsSelectFileFilter();
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectButton1);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectButton2);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectButton3);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectButton4);
    UserIconLoader->DialogImage(DI_SelectFile, ui.SelectButton5);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton1);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton2);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton3);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton4);
    UserIconLoader->DialogImage(DI_Delete, ui.DeleteButton5);
    UserIconLoader->DialogImage(DI_Load, ui.AddAllButton);
    UserIconLoader->DialogImage(DI_Delete, ui.ClearAllButton);

    setProperty("HelpContext", ID_ProfileEditAdditionalChecksums);  // HelpContext:=...

    LastDir = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
}

void TModernProfileEditorAddtionalChecksumFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    ui.FilenameEdit1->setText(Game->AddtionalChecksumFile1());
    ui.FilenameEdit2->setText(Game->AddtionalChecksumFile2());
    ui.FilenameEdit3->setText(Game->AddtionalChecksumFile3());
    ui.FilenameEdit4->setText(Game->AddtionalChecksumFile4());
    ui.FilenameEdit5->setText(Game->AddtionalChecksumFile5());
    ui.ChecksumEdit1->setText(Game->AddtionalChecksumFile1Checksum());
    ui.ChecksumEdit2->setText(Game->AddtionalChecksumFile2Checksum());
    ui.ChecksumEdit3->setText(Game->AddtionalChecksumFile3Checksum());
    ui.ChecksumEdit4->setText(Game->AddtionalChecksumFile4Checksum());
    ui.ChecksumEdit5->setText(Game->AddtionalChecksumFile5Checksum());

    FGame = Game;
}

void TModernProfileEditorAddtionalChecksumFrame::ButtonWork(QObject* Sender)
{
    int I, J, K;
    T5StringArray Files, Checksums;
    QString Path;
    // TEdit(FindComponent(Name))
    auto Edit = [this](const QString& Name) { return findChild<QLineEdit*>(Name); };

    const int Tag = Sender->property("tag").toInt();
    if (Tag >= 0 && Tag <= 4) {
        OpenDialog->InitialDir = LastDir;
        if (!OpenDialog->Execute()) return;
        LastDir = ExtractFilePath(OpenDialog->FileName);
        Edit("FilenameEdit" + IntToStr(Tag + 1))->setText(ExtractFileName(OpenDialog->FileName));
        Edit("ChecksumEdit" + IntToStr(Tag + 1))->setText(GetMD5Sum(OpenDialog->FileName, false));
    } else if (Tag >= 5 && Tag <= 9) {
        Edit("FilenameEdit" + IntToStr(Tag - 4))->setText("");
        Edit("ChecksumEdit" + IntToStr(Tag - 4))->setText("");
    } else if (Tag == -1) {
        I = 0;
        for (J = 1; J <= 5; J++) if (Edit("FilenameEdit" + IntToStr(J))->text().isEmpty()) I++;
        if (I == 0) return;
        Path = MakeAbsPath(PrgSetup->GameDir(), PrgSetup->BaseDir());
        if (!SelectDirectory(LanguageSetup->ProfileEditorAdditionalChecksumsSelectFolder(), QString(), Path, this)) return;
        I = GetAdditionalChecksumData(FGame, Path, I, Files, Checksums);
        K = 0;
        for (J = 1; J <= 5; J++) if (Edit("FilenameEdit" + IntToStr(J))->text().isEmpty()) {
            if (K >= I) break;
            Edit("FilenameEdit" + IntToStr(J))->setText(Files[K]);
            Edit("ChecksumEdit" + IntToStr(J))->setText(Checksums[K]);
            K++;
        }
    } else if (Tag == -2) {
        for (I = 1; I <= 5; I++) {
            Edit("FilenameEdit" + IntToStr(I))->setText("");
            Edit("ChecksumEdit" + IntToStr(I))->setText("");
        }
    }
}

void TModernProfileEditorAddtionalChecksumFrame::GetGame(TGame* Game)
{
    Game->SetAddtionalChecksumFile1(ui.FilenameEdit1->text());
    Game->SetAddtionalChecksumFile2(ui.FilenameEdit2->text());
    Game->SetAddtionalChecksumFile3(ui.FilenameEdit3->text());
    Game->SetAddtionalChecksumFile4(ui.FilenameEdit4->text());
    Game->SetAddtionalChecksumFile5(ui.FilenameEdit5->text());
    Game->SetAddtionalChecksumFile1Checksum(ui.ChecksumEdit1->text());
    Game->SetAddtionalChecksumFile2Checksum(ui.ChecksumEdit2->text());
    Game->SetAddtionalChecksumFile3Checksum(ui.ChecksumEdit3->text());
    Game->SetAddtionalChecksumFile4Checksum(ui.ChecksumEdit4->text());
    Game->SetAddtionalChecksumFile5Checksum(ui.ChecksumEdit5->text());
}

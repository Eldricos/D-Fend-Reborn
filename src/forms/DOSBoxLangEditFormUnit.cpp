// C++ port of DOSBoxLangEditFormUnit.pas (implementation section).
#include "DOSBoxLangEditFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "DOSBoxLangTools.h"

TDOSBoxLangEditForm* DOSBoxLangEditForm = nullptr;

TDOSBoxLangEditForm::TDOSBoxLangEditForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    connect(ui.LanguageStringComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { LanguageStringComboBoxChange(ui.LanguageStringComboBox); });
    connect(ui.OKButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.OKButton); });
    connect(ui.CancelButton, &QPushButton::clicked, this, [this]() { ButtonWork(ui.CancelButton); });
    FormCreate(this);  // OnCreate
}

TDOSBoxLangEditForm::~TDOSBoxLangEditForm()
{
    FormDestroy(this);  // OnDestroy
}

void TDOSBoxLangEditForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

bool TDOSBoxLangEditForm::closeQuery()
{
    bool CanClose = true;
    FormCloseQuery(this, CanClose);
    return CanClose;
}

void TDOSBoxLangEditForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    LangIDStrings = new TStringList;
    LangValueStrings = new TStringList;
    Changed = false;
}

void TDOSBoxLangEditForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    delete LangIDStrings; LangIDStrings = nullptr;
    delete LangValueStrings; LangValueStrings = nullptr;
}

void TDOSBoxLangEditForm::FormShow(QObject* Sender)
{
    int I;
    QString S;

    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(...) - not needed with Qt

    setWindowTitle(LanguageSetup->DOSBoxLangEditor());
    ui.LanguageStringLabel->setText(LanguageSetup->DOSBoxLangEditorKey());
    ui.OKButton->setText(LanguageSetup->DOSBoxLangEditorSave());
    ui.CancelButton->setText(LanguageSetup->DOSBoxLangEditorCancel());

    UserIconLoader->DialogImage(DI_OK, ui.OKButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    LastID = -1;
    {
        const QSignalBlocker Block(ui.LanguageStringComboBox);
        ui.LanguageStringComboBox->clear();
        for (I = 0; I < LangIDStrings->Count(); I++) {
            S = (*LangIDStrings)[I]; if (Trim(S) == "") continue;
            ui.LanguageStringComboBox->addItem(S, I);
        }
    }

    if (ui.LanguageStringComboBox->count() > 0) {
        const QSignalBlocker Block(ui.LanguageStringComboBox);
        ui.LanguageStringComboBox->setCurrentIndex(0);
        LanguageStringComboBoxChange(Sender);
    }
}

void TDOSBoxLangEditForm::LanguageStringComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, T;

    if (LastID >= 0) {
        // Memo.Text (TRichEdit lines are separated by #13#10)
        S = ui.Memo->toPlainText().replace("\n", "\r\n");
        T = (*LangValueStrings)[ui.LanguageStringComboBox->itemData(LastID).toInt()];
        if (Pos("\r\n", T) == 0) S = Replace(S, "\r\n", "\r");
        if ((S != "") && (T != "") && (S[Length(S) - 1] == QChar('\r')) && (T[Length(T) - 1] != QChar('\r'))) S.chop(1);
        if ((S != "") && (T != "") && (S[0] == QChar('\r')) && (T[0] != QChar('\r'))) S = Copy(S, 2, MaxInt);
        if (S != T) {
            Changed = true;
            (*LangValueStrings)[ui.LanguageStringComboBox->itemData(LastID).toInt()] = S;
        }
    }

    LastID = ui.LanguageStringComboBox->currentIndex();
    if (LastID < 0) return;
    QString V = (*LangValueStrings)[ui.LanguageStringComboBox->itemData(LastID).toInt()];
    V.replace("\r\n", "\n"); V.replace('\r', '\n');
    ui.Memo->setPlainText(V);
}

void TDOSBoxLangEditForm::ButtonWork(QObject* Sender)
{
    switch (Sender->property("tag").toInt()) {
        case 0: setModalResult(mrOk); break;
        case 1: setModalResult(mrCancel); break;
    }
}

void TDOSBoxLangEditForm::FormCloseQuery(QObject* Sender, bool& CanClose)
{
    LanguageStringComboBoxChange(Sender);

    if (Changed && (ModalResult() != mrOk)) {
        if (MessageDlg(LanguageSetup->DOSBoxLangEditorCancelConfirmation(), mtConfirmation, mbYes | mbNo, 0, this) != mrYes) {
            setModalResult(mrNone); CanClose = false;
        }
    }
}

/* global */

bool ShowDOSBoxLangEditDialog(QWidget* AOwner, const QString& ALoadDOSBoxLangFile, const QString& ASaveDOSBoxLangFile)
{
    bool result = false;
    QString LangFile;
    bool IsTemp;

    TDOSBoxLangEditForm* DOSBoxLangEditForm = new TDOSBoxLangEditForm(AOwner);
    if (ALoadDOSBoxLangFile == "") {
        IsTemp = true;
        LangFile = GetTempDOSBoxEnglishLangFile();
    } else {
        IsTemp = false;
        LangFile = ALoadDOSBoxLangFile;
    }
    bool LoadOK = LoadDOSBoxLangFile(LangFile, DOSBoxLangEditForm->LangIDStrings, DOSBoxLangEditForm->LangValueStrings);
    if (IsTemp) ExtDeleteFile(LangFile, ftTemp);
    if (!LoadOK) {
        MessageDlg(Format(LanguageSetup->MessageCouldNotFindFile(), {LangFile}), mtError, mbOK, 0, AOwner);
        delete DOSBoxLangEditForm;
        return result;
    }
    result = (DOSBoxLangEditForm->ShowModal() == mrOk);
    if (result) {
        if (!SaveDOSBoxLangFile(ASaveDOSBoxLangFile, DOSBoxLangEditForm->LangIDStrings, DOSBoxLangEditForm->LangValueStrings)) {
            MessageDlg(Format(LanguageSetup->MessageCouldNotSaveFile(), {ASaveDOSBoxLangFile}), mtError, mbOK, 0, AOwner);
            result = false;
        }
    }
    delete DOSBoxLangEditForm;
    return result;
}

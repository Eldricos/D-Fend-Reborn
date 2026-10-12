// C++ port of LanguageEditorFormUnit.pas (implementation section).
#include "LanguageEditorFormUnit.h"

#include "IniFiles.h"
#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "HelpTools.h"
#include "IconLoaderUnit.h"
#include "Process.h"

TLanguageEditorForm* LanguageEditorForm = nullptr;

// Application.HelpCommand(HELP_CONTEXT, Data)
static void HelpCommandContext(int Data)
{
    // TODO(port): no HTML help (hh.exe) context jump available; open the help file instead.
    Q_UNUSED(Data);
    if (GLOBAL_HELPROUTER != nullptr && !GLOBAL_HELPROUTER->Helpfile().isEmpty()) ShellOpen(GLOBAL_HELPROUTER->Helpfile());
}

// The TStringGrid "Tab" (ColCount=3, FixedCols=2, FixedRows=1) is a QTableWidget
// with 3 columns: the fixed row 0 is the horizontal header, the fixed columns 0
// and 1 are read only cells drawn with a light gray background (TabDrawCell).

QString TLanguageEditorForm::Cells(int ACol, int ARow) const
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.Tab->horizontalHeaderItem(ACol);
        return (H != nullptr) ? H->text() : QString();
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    return (It != nullptr) ? It->text() : QString();
}

void TLanguageEditorForm::SetCells(int ACol, int ARow, const QString& Value)
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.Tab->horizontalHeaderItem(ACol);
        if (H == nullptr) { H = new QTableWidgetItem; ui.Tab->setHorizontalHeaderItem(ACol, H); }
        H->setText(Value);
        return;
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    if (It == nullptr) { It = new QTableWidgetItem; ui.Tab->setItem(ARow - 1, ACol, It); }
    It->setText(Value);
    TabDrawCell(ui.Tab, ACol, ARow, ui.Tab->visualItemRect(It), 0);
}

void TLanguageEditorForm::SetRowCount(int ARowCount)
{
    ui.Tab->setRowCount(ARowCount - 1);
}

TLanguageEditorForm::TLanguageEditorForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ui.Tab->setColumnCount(3);
    ui.Tab->verticalHeader()->setVisible(false);
    ui.Tab->setEditTriggers(QAbstractItemView::AllEditTriggers);   // goEditing, goAlwaysShowEditor
    ui.Tab->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);   // goColSizing
    SetRowCount(5);

    connect(ui.SectionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { SectionComboBoxChange(ui.SectionComboBox); });
    connect(ui.ShowComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ShowComboBoxChange(ui.ShowComboBox); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return K == 0;
    });
    FormCreate(this);  // OnCreate
}

TLanguageEditorForm::~TLanguageEditorForm() = default;

void TLanguageEditorForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

void TLanguageEditorForm::resizeEvent(QResizeEvent* E)
{
    TVclForm::resizeEvent(E);
    FormResize(this);
}

void TLanguageEditorForm::done(int R)
{
    TCloseAction Action = caHide;
    FormClose(this, Action);
    TVclForm::done(R);
}

void TLanguageEditorForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    // SetVistaFonts(self); Font.Charset:=...; NoFlicker(...) - not needed with Qt

    setWindowTitle(LanguageSetup->LanguageEditorCaption());

    ui.CloseButton->setText(LanguageSetup->Close());

    {
        const QSignalBlocker Block(ui.ShowComboBox);   // ShowComboBox.OnChange:=nil
        ui.ShowComboBox->clear();
        ui.ShowComboBox->addItem(RemoveUnderline(LanguageSetup->LanguageEditorShowAll()));
        ui.ShowComboBox->addItem(RemoveUnderline(LanguageSetup->LanguageEditorShowUntranslatedOnly()));
        ViewMode = Min(1, Max(0, PrgSetup->LanguageEditorViewMode()));
        ui.ShowComboBox->setCurrentIndex(ViewMode);
    }   // ShowComboBox.OnChange:=ShowComboBoxChange

    UserIconLoader->DialogImage(DI_Close, ui.CloseButton);

    DefaultLanguageFile = PrgDir() + LanguageSubDir + PathDelim + "English.ini";
    LastSection = -1;

    SetComboHint(ui.ShowComboBox);
    SetComboHint(ui.SectionComboBox);
    ComboBoxDropDown(ui.ShowComboBox);   // OnDropDown (QComboBox has no drop down event, the width is set in advance)
}

void TLanguageEditorForm::FormShow(QObject* Sender)
{
    TIniFile* Ini;
    int I;

    setWindowTitle(windowTitle() + " [" + LanguageFile + "]");

    /* Init table */
    FormResize(Sender);
    SetCells(0, 0, LanguageSetup->LanguageEditorIdentifier());
    SetCells(1, 0, LanguageSetup->LanguageEditorEnglish());
    SetCells(2, 0, ChangeFileExt(ExtractFileName(LanguageFile), ""));

    /* Init sections */
    TStringList St, St2, St3;

    Ini = new TIniFile(DefaultLanguageFile);
    St3.Clear(); Ini->ReadSections(St3);
    for (I = 0; I < St3.Count(); I++) if (St2.IndexOf(ExtUpperCase(St3[I])) < 0) { St2.Add(ExtUpperCase(St3[I])); St.Add(St3[I]); }
    delete Ini;

    Ini = new TIniFile(LanguageFile);
    St3.Clear(); Ini->ReadSections(St3);
    for (I = 0; I < St3.Count(); I++) if (St2.IndexOf(ExtUpperCase(St3[I])) < 0) { St2.Add(ExtUpperCase(St3[I])); St.Add(St3[I]); }
    delete Ini;

    St.Sort();
    {
        const QSignalBlocker Block(ui.SectionComboBox);
        ui.SectionComboBox->addItems(St.ToQStringList());
    }
    ComboBoxDropDown(ui.SectionComboBox);   // OnDropDown

    /* Load first section */
    if (ui.SectionComboBox->count() > 0) {
        {
            const QSignalBlocker Block(ui.SectionComboBox);
            ui.SectionComboBox->setCurrentIndex(0);
        }
        SectionComboBoxChange(Sender);
    }
}

void TLanguageEditorForm::FormResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I;
    I = ui.Tab->viewport()->width() - 10;
    ui.Tab->setColumnWidth(0, I * 1 / 5);
    ui.Tab->setColumnWidth(1, I * 2 / 5);
    ui.Tab->setColumnWidth(2, I * 2 / 5);
}

void TLanguageEditorForm::TabDrawCell(QObject* Sender, int ACol, int ARow, QRect Rect, int State)
{
    Q_UNUSED(Sender); Q_UNUSED(Rect); Q_UNUSED(State);
    // Owner draw of the fixed columns: light gray background ($F7F7F7), not editable
    if ((ARow > 0) && (ACol < 2)) {
        QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
        if (It == nullptr) return;
        It->setBackground(TColorToQColor(0xF7F7F7));
        It->setFlags(It->flags() & ~Qt::ItemIsEditable);
    }
}

void TLanguageEditorForm::ComboBoxDropDown(QObject* Sender)
{
    SetComboDropDownDropDownWidth(qobject_cast<QComboBox*>(Sender));
}

void TLanguageEditorForm::FormClose(QObject* Sender, TCloseAction& Action)
{
    Q_UNUSED(Action);
    SectionComboBoxChange(Sender);
}

void TLanguageEditorForm::LoadSection(const QString& Section)
{
    TIniFile *Ini1, *Ini2;
    int I, J, Untranslated;
    QString S, T;

    Untranslated = 0;
    Ini1 = new TIniFile(DefaultLanguageFile);
    Ini2 = new TIniFile(LanguageFile);
    {
        TStringList St, St2, St3;
        St3.Clear(); Ini1->ReadSection(Section, St3);
        for (I = 0; I < St3.Count(); I++) if (St2.IndexOf(ExtUpperCase(St3[I])) < 0) { St2.Add(ExtUpperCase(St3[I])); St.Add(St3[I]); }
        St3.Clear(); Ini2->ReadSection(Section, St3);
        for (I = 0; I < St3.Count(); I++) if (St2.IndexOf(ExtUpperCase(St3[I])) < 0) { St2.Add(ExtUpperCase(St3[I])); St.Add(St3[I]); }

        for (I = 0; I < St.Count(); I++) {
            S = Ini1->ReadString(Section, St[I], "");
            if ((ExtUpperCase(Section) == "AUTHOR") && (ExtUpperCase(St[I]) == "NAME")) S = LanguageSetup->NotSet();
            T = Ini2->ReadString(Section, St[I], S);
            if (S == T) Untranslated++;
        }

        if (ViewMode == 0) SetRowCount(Max(2, St.Count() + 1)); else SetRowCount(Max(2, Untranslated + 1));
        SetCells(0, 1, ""); SetCells(1, 1, ""); SetCells(2, 1, "");
        J = 1;
        for (I = 0; I < St.Count(); I++) {
            S = Ini1->ReadString(Section, St[I], "");
            if ((ExtUpperCase(Section) == "AUTHOR") && (ExtUpperCase(St[I]) == "NAME")) S = LanguageSetup->NotSet();
            T = Ini2->ReadString(Section, St[I], S);
            if ((S != T) && (ViewMode == 1)) continue;
            SetCells(0, J, St[I]); SetCells(2, J, T); SetCells(1, J, S); J++;
        }
        ui.Tab->setEnabled(J > 1);
    }
    delete Ini2;
    delete Ini1;

    switch (Untranslated) {
        case 0: VclLayout::setSimpleText(ui.StatusBar, LanguageSetup->LanguageEditorInfoAllTranslated()); break;
        case 1: VclLayout::setSimpleText(ui.StatusBar, LanguageSetup->LanguageEditorInfoOneTranslationMissing()); break;
        default: VclLayout::setSimpleText(ui.StatusBar, Format(LanguageSetup->LanguageEditorInfoTranslationsMissing(), {Untranslated}));
    }
}

void TLanguageEditorForm::SaveSection(const QString& Section)
{
    TIniFile* Ini;
    int I;

    // Commit a cell that is still being edited
    QWidget* Editor = QApplication::focusWidget();
    if (Editor != nullptr && Editor != ui.Tab && ui.Tab->isAncestorOf(Editor))
        QMetaObject::invokeMethod(ui.Tab, "commitData", Q_ARG(QWidget*, Editor));

    Ini = new TIniFile(LanguageFile);
    for (I = 1; I < ui.Tab->rowCount() + 1; I++) if (Cells(0, I) != "")
        Ini->WriteString(Section, Cells(0, I), Cells(2, I));
    delete Ini;
}

void TLanguageEditorForm::SectionComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (LastSection >= 0) SaveSection(ui.SectionComboBox->itemText(LastSection));

    LastSection = ui.SectionComboBox->currentIndex();
    if (LastSection >= 0) LoadSection(ui.SectionComboBox->itemText(LastSection));

    SetComboHint(ui.ShowComboBox);
    SetComboHint(ui.SectionComboBox);
}

void TLanguageEditorForm::ShowComboBoxChange(QObject* Sender)
{
    ViewMode = ui.ShowComboBox->currentIndex();
    SectionComboBoxChange(Sender);
}

void TLanguageEditorForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Sender);
    if ((Key == Qt::Key_F1) && (Shift == Qt::NoModifier)) { HelpCommandContext(ID_ExtrasLanguageEditor); Key = 0; }
}

/* global */

void ShowLanguageEditorDialog(QWidget* AOwner, const QString& ALanguageFile)
{
    LanguageEditorForm = new TLanguageEditorForm(AOwner);
    LanguageEditorForm->LanguageFile = ALanguageFile;
    LanguageEditorForm->ShowModal();
    delete LanguageEditorForm;
    LanguageEditorForm = nullptr;
}

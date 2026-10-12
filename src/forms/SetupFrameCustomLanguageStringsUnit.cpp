// C++ port of SetupFrameCustomLanguageStringsUnit.pas (implementation section).
#include "SetupFrameCustomLanguageStringsUnit.h"

#include "LanguageSetupUnit.h"
#include "PrgSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"

namespace {

// TStringGrid emulation (Tab: FixedRows=1 = the horizontal header, FixedCols=0)
QString Cell(QTableWidget *T, int C, int R)
{
    if (R == 0) { QTableWidgetItem *H = T->horizontalHeaderItem(C); return H ? H->text() : QString(); }
    QTableWidgetItem *It = T->item(R - 1, C);
    return It ? It->text() : QString();
}

void SetCell(QTableWidget *T, int C, int R, const QString &S)
{
    if (R == 0) {
        QTableWidgetItem *H = T->horizontalHeaderItem(C);
        if (H == nullptr) { H = new QTableWidgetItem; T->setHorizontalHeaderItem(C, H); }
        H->setText(S);
        return;
    }
    QTableWidgetItem *It = T->item(R - 1, C);
    if (It == nullptr) { It = new QTableWidgetItem; T->setItem(R - 1, C, It); }
    It->setText(S);
}

int RowCount(QTableWidget *T) { return T->rowCount() + 1; }
void SetRowCount(QTableWidget *T, int N) { T->setRowCount(qMax(0, N - 1)); }
int Row(QTableWidget *T) { return T->currentRow() + 1; }
int Col(QTableWidget *T) { return qMax(0, T->currentColumn()); }
void SetRow(QTableWidget *T, int R) { T->setCurrentCell(R - 1, Col(T)); }
void SetCol(QTableWidget *T, int C) { T->setCurrentCell(qMax(0, T->currentRow()), C); }
bool Editing(QTableWidget *T) { return T->editTriggers() != QAbstractItemView::NoEditTriggers; }
void SetEditing(QTableWidget *T, bool B)
{
    T->setEditTriggers(B ? QAbstractItemView::AllEditTriggers : QAbstractItemView::NoEditTriggers);
}

}  // namespace

TSetupFrameCustomLanguageStrings::TSetupFrameCustomLanguageStrings(QWidget *AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    // DFM event wiring
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.AddButton); });
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DelButton); });
    connect(ui.UpButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.UpButton); });
    connect(ui.DownButton, &QToolButton::clicked, this, [this]() { ButtonWork(ui.DownButton); });
    connect(ui.TypeComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) { TypeComboBoxChange(ui.TypeComboBox); });
    connect(ui.Tab, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) { TabClick(ui.Tab); });
}

TSetupFrameCustomLanguageStrings::~TSetupFrameCustomLanguageStrings()
{
    delete GenreEnglish;
    delete GenreCustom;
    delete LanguageEnglish;
    delete LanguageCustom;
    delete LicenseEnglish;
    delete LicenseCustom;
}

void TSetupFrameCustomLanguageStrings::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    FrameResize(this);
}

/* TSetupFrameCustomLanguageStrings */

QString TSetupFrameCustomLanguageStrings::GetName()
{
    return LanguageSetup->SetupFormGamesListTranslations();
}

void TSetupFrameCustomLanguageStrings::InitGUIAndLoadSetup(TInitData& InitData)
{
    // NoFlicker(...): not needed in Qt

    LastType = -1;
    ui.TypeComboBox->clear();
    ui.TypeComboBox->addItem("");
    ui.TypeComboBox->addItem("");
    ui.TypeComboBox->addItem("");

    GenreEnglish = new TStringList;
    GenreCustom = new TStringList;
    LanguageEnglish = new TStringList;
    LanguageCustom = new TStringList;
    LicenseEnglish = new TStringList;
    LicenseCustom = new TStringList;

    GameDB = InitData.GameDB;

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    UserIconLoader->DialogImage(DI_Up, ui.UpButton);
    UserIconLoader->DialogImage(DI_Down, ui.DownButton);

    setProperty("HelpContext", ID_FileOptionsGamesListTranslations);
}

void TSetupFrameCustomLanguageStrings::BeforeChangeLanguage()
{
    TypeComboBoxChange(this);

    LanguageSetupEnglishGenres->Clear();
    LanguageSetupEnglishGenres->AddStrings(*GenreEnglish);
    LanguageSetupCustomGenres->Clear();
    LanguageSetupCustomGenres->AddStrings(*GenreCustom);
    LanguageSetupEnglishLanguages->Clear();
    LanguageSetupEnglishLanguages->AddStrings(*LanguageEnglish);
    LanguageSetupCustomLanguages->Clear();
    LanguageSetupCustomLanguages->AddStrings(*LanguageCustom);
    LanguageSetupEnglishLicenses->Clear();
    LanguageSetupEnglishLicenses->AddStrings(*LicenseEnglish);
    LanguageSetupCustomLicenses->Clear();
    LanguageSetupCustomLicenses->AddStrings(*LicenseCustom);

    SaveGameListTranslations();
}

void TSetupFrameCustomLanguageStrings::LoadLanguage()
{
    int I;
    bool Engl;

    I = ui.TypeComboBox->currentIndex();
    ui.TypeComboBox->setItemText(0, LanguageSetup->GameGenre());
    ui.TypeComboBox->setItemText(1, LanguageSetup->GameLanguage());
    ui.TypeComboBox->setItemText(2, LanguageSetup->GameLicense());
    ui.TypeComboBox->setCurrentIndex(I);

    ui.AddButton->setToolTip(LanguageSetup->AddNewEmptyLine());
    ui.DelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));
    ui.UpButton->setToolTip(LanguageSetup->MoveUp());
    ui.DownButton->setToolTip(LanguageSetup->MoveDown());

    SetCell(ui.Tab, 0, 0, LanguageSetup->SetupFormGamesListTranslationsEnglish());
    SetCell(ui.Tab, 1, 0, LanguageSetup->SetupFormGamesListTranslationsCustom());

    ui.InfoLabel->setText(LanguageSetup->SetupFormGamesListTranslationsInfo());

    GenreEnglish->Clear();
    GenreCustom->Clear();
    LanguageEnglish->Clear();
    LanguageCustom->Clear();
    LicenseEnglish->Clear();
    LicenseCustom->Clear();

    GenreEnglish->AddStrings(*LanguageSetupEnglishGenres);
    GenreCustom->AddStrings(*LanguageSetupCustomGenres);
    while (GenreEnglish->Count() < GenreCustom->Count()) GenreEnglish->Add("");
    while (GenreEnglish->Count() > GenreCustom->Count()) GenreCustom->Add("");
    LanguageEnglish->AddStrings(*LanguageSetupEnglishLanguages);
    LanguageCustom->AddStrings(*LanguageSetupCustomLanguages);
    while (LanguageEnglish->Count() < LanguageCustom->Count()) LanguageEnglish->Add("");
    while (LanguageEnglish->Count() > LanguageCustom->Count()) LanguageCustom->Add("");
    LicenseEnglish->AddStrings(*LanguageSetupEnglishLicenses);
    LicenseCustom->AddStrings(*LanguageSetupCustomLicenses);
    while (LicenseEnglish->Count() < LicenseCustom->Count()) LicenseEnglish->Add("");
    while (LicenseEnglish->Count() > LicenseCustom->Count()) LicenseCustom->Add("");

    LastType = -1;
    TypeComboBoxChange(this);

    Engl = (ExtUpperCase(LanguageSetup->LocalLanguageName()) == "ENGLISH");
    ui.EnglsihLabel->setVisible(Engl);
    ui.TypeComboBox->setVisible(!Engl);
    ui.AddButton->setVisible(!Engl);
    ui.DelButton->setVisible(!Engl);
    ui.UpButton->setVisible(!Engl);
    ui.DownButton->setVisible(!Engl);
    ui.Tab->setVisible(!Engl);
    ui.InfoLabel->setVisible(!Engl);
}

void TSetupFrameCustomLanguageStrings::DOSBoxDirChanged()
{
}

void TSetupFrameCustomLanguageStrings::ShowFrame(bool AdvancedMode)
{
    Q_UNUSED(AdvancedMode);
    bool Engl;

    FrameResize(this);
    LastType = -1;
    ui.TypeComboBox->setCurrentIndex(0);
    TypeComboBoxChange(this);

    Engl = (ExtUpperCase(LanguageSetup->LocalLanguageName()) == "ENGLISH");
    ui.EnglsihLabel->setVisible(Engl);
    ui.TypeComboBox->setVisible(true); ui.TypeComboBox->setVisible(!Engl);
    ui.AddButton->setVisible(!Engl);
    ui.DelButton->setVisible(!Engl);
    ui.UpButton->setVisible(!Engl);
    ui.DownButton->setVisible(!Engl);
    ui.Tab->setVisible(true); ui.Tab->setVisible(!Engl);
    ui.InfoLabel->setVisible(!Engl);
}

void TSetupFrameCustomLanguageStrings::HideFrame()
{
    TypeComboBoxChange(this);
}

void TSetupFrameCustomLanguageStrings::RestoreDefaults()
{
    TStringList *DefaultGenreEnglish = nullptr, *DefaultGenreCustom = nullptr, *DefaultLanguageEnglish = nullptr,
                *DefaultLanguageCustom = nullptr, *DefaultLicenseEnglish = nullptr, *DefaultLicenseCustom = nullptr;

    GetDefaultGameListTranslations(DefaultGenreEnglish, DefaultGenreCustom, DefaultLanguageEnglish, DefaultLanguageCustom, DefaultLicenseEnglish, DefaultLicenseCustom);

    GenreEnglish->Clear();
    GenreCustom->Clear();
    LanguageEnglish->Clear();
    LanguageCustom->Clear();
    LicenseEnglish->Clear();
    LicenseCustom->Clear();

    GenreEnglish->AddStrings(*DefaultGenreEnglish);
    GenreCustom->AddStrings(*DefaultGenreCustom);
    while (GenreEnglish->Count() < GenreCustom->Count()) GenreEnglish->Add("");
    while (GenreEnglish->Count() > GenreCustom->Count()) GenreCustom->Add("");
    LanguageEnglish->AddStrings(*DefaultLanguageEnglish);
    LanguageCustom->AddStrings(*DefaultLanguageCustom);
    while (LanguageEnglish->Count() < LanguageCustom->Count()) LanguageEnglish->Add("");
    while (LanguageEnglish->Count() > LanguageCustom->Count()) LanguageCustom->Add("");
    LicenseEnglish->AddStrings(*DefaultLicenseEnglish);
    LicenseCustom->AddStrings(*DefaultLicenseCustom);
    while (LicenseEnglish->Count() < LicenseCustom->Count()) LicenseEnglish->Add("");
    while (LicenseEnglish->Count() > LicenseCustom->Count()) LicenseCustom->Add("");

    LastType = -1;
    TypeComboBoxChange(this);

    delete DefaultGenreEnglish;
    delete DefaultGenreCustom;
    delete DefaultLanguageEnglish;
    delete DefaultLanguageCustom;
    // (the Delphi code leaks the two license lists)
    delete DefaultLicenseEnglish;
    delete DefaultLicenseCustom;
}

void TSetupFrameCustomLanguageStrings::SaveSetup()
{
    BeforeChangeLanguage();
}

void TSetupFrameCustomLanguageStrings::FrameResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.Tab->setColumnWidth(0, (ui.Tab->viewport()->width() - 20) / 2);
    ui.Tab->setColumnWidth(1, (ui.Tab->viewport()->width() - 20) / 2);
}

void TSetupFrameCustomLanguageStrings::ButtonWork(QObject* Sender)
{
    int I, J;
    QString S;
    TStringList* St;
    QAction* M;
    QTableWidget *Tab = ui.Tab;

    switch (Sender->property("tag").toInt()) {
        case 0: {
            /* Add button */
            switch (ui.TypeComboBox->currentIndex()) {
                case 0: St = GameDB->GetGenreList(); break;
                case 1: St = GameDB->GetLanguageList(); break;
                case 2: St = GameDB->GetLicenseList(); break;
                default: St = new TStringList; break;
            }
            I = 0;
            while (I < St->Count()) {
                S = Trim(ExtUpperCase((*St)[I]));
                for (J = 1; J < RowCount(Tab); J++) if (Trim(ExtUpperCase(Cell(Tab, 0, J))) == S) { St->Delete(I); I--; break; }
                I++;
            }
            if (St->Count() == 0) {
                delete St;
                SetRowCount(Tab, RowCount(Tab) + 1);
                SetRow(Tab, RowCount(Tab) - 1);
                SetCol(Tab, 0);
                Tab->setFocus();
                return;
            } else {
                ui.AddLinePopupMenu->clear();
                M = ui.AddLinePopupMenu->addAction(LanguageSetup->AddNewEmptyLine()); M->setProperty("tag", -1);
                connect(M, &QAction::triggered, this, [this, M]() { ButtonWork(M); });
                ui.AddLinePopupMenu->addSeparator();
                for (I = 0; I < St->Count(); I++) {
                    M = ui.AddLinePopupMenu->addAction(MaskUnderlineAmpersand((*St)[I])); M->setProperty("tag", -2);
                    connect(M, &QAction::triggered, this, [this, M]() { ButtonWork(M); });
                }
            }
            delete St;
            QWidget *W = qobject_cast<QWidget*>(Sender);
            QPoint P = W ? mapToGlobal(W->pos()) : QCursor::pos();
            ui.AddLinePopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
            break;
        }
        case -1:
            /* Add empty line */
            SetRowCount(Tab, RowCount(Tab) + 1);
            SetRow(Tab, RowCount(Tab) - 1);
            SetCol(Tab, 0);
            SetCell(Tab, 0, Row(Tab), ""); SetCell(Tab, 1, Row(Tab), "");
            Tab->setFocus();
            break;
        case -2: {
            /* Add used name */
            if ((Trim(Cell(Tab, 0, RowCount(Tab) - 1)) != "") || (Trim(Cell(Tab, 1, RowCount(Tab) - 1)) != "")) SetRowCount(Tab, RowCount(Tab) + 1);
            SetRow(Tab, RowCount(Tab) - 1);
            SetCol(Tab, 0);
            QAction *A = qobject_cast<QAction*>(Sender);
            SetCell(Tab, 0, Row(Tab), RemoveUnderline(A ? A->text() : QString())); SetCell(Tab, 1, Row(Tab), "");
            SetCol(Tab, 1);
            Tab->setFocus();
            break;
        }
        case 1:
            if ((RowCount(Tab) > 2) && (Row(Tab) > 0)) {
                /* Del button */
                for (I = Row(Tab) + 1; I < RowCount(Tab); I++) { SetCell(Tab, 0, I - 1, Cell(Tab, 0, I)); SetCell(Tab, 1, I - 1, Cell(Tab, 1, I)); }
                SetRow(Tab, qMax(1, Row(Tab) - 1));
                SetRowCount(Tab, RowCount(Tab) - 1);
            }
            break;
        case 2:
            if (Row(Tab) > 1) {
                /* Up button */
                S = Cell(Tab, 0, Row(Tab)); SetCell(Tab, 0, Row(Tab), Cell(Tab, 0, Row(Tab) - 1)); SetCell(Tab, 0, Row(Tab) - 1, S);
                S = Cell(Tab, 1, Row(Tab)); SetCell(Tab, 1, Row(Tab), Cell(Tab, 1, Row(Tab) - 1)); SetCell(Tab, 1, Row(Tab) - 1, S);
                SetRow(Tab, Row(Tab) - 1);
            }
            break;
        case 3:
            if ((Row(Tab) >= 1) && (Row(Tab) < RowCount(Tab) - 1)) {
                /* Down button */
                S = Cell(Tab, 0, Row(Tab)); SetCell(Tab, 0, Row(Tab), Cell(Tab, 0, Row(Tab) + 1)); SetCell(Tab, 0, Row(Tab) + 1, S);
                S = Cell(Tab, 1, Row(Tab)); SetCell(Tab, 1, Row(Tab), Cell(Tab, 1, Row(Tab) + 1)); SetCell(Tab, 1, Row(Tab) + 1, S);
                SetRow(Tab, Row(Tab) + 1);
            }
            break;
    }

    TabClick(Sender);
}

void TSetupFrameCustomLanguageStrings::TabClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QTableWidget *Tab = ui.Tab;
    ui.DelButton->setEnabled(Editing(Tab) && (RowCount(Tab) > 2));
    ui.UpButton->setEnabled(Editing(Tab) && (Row(Tab) > 1));
    ui.DownButton->setEnabled(Editing(Tab) && (Row(Tab) < RowCount(Tab) - 1) && (RowCount(Tab) > 2));
}

void TSetupFrameCustomLanguageStrings::TypeComboBoxChange(QObject* Sender)
{
    int I;
    QTableWidget *Tab = ui.Tab;

    switch (LastType) {
        case 0:
            GenreEnglish->Clear();
            GenreCustom->Clear();
            for (I = 1; I < RowCount(Tab); I++) { GenreEnglish->Add(Cell(Tab, 0, I)); GenreCustom->Add(Cell(Tab, 1, I)); }
            break;
        case 1:
            LanguageEnglish->Clear();
            LanguageCustom->Clear();
            for (I = 1; I < RowCount(Tab); I++) { LanguageEnglish->Add(Cell(Tab, 0, I)); LanguageCustom->Add(Cell(Tab, 1, I)); }
            break;
        case 2:
            LicenseEnglish->Clear();
            LicenseCustom->Clear();
            for (I = 1; I < RowCount(Tab); I++) { LicenseEnglish->Add(Cell(Tab, 0, I)); LicenseCustom->Add(Cell(Tab, 1, I)); }
            break;
    }

    LastType = ui.TypeComboBox->currentIndex();
    SetRowCount(Tab, 2);
    SetCell(Tab, 0, 1, "");
    SetCell(Tab, 1, 1, "");
    if (LastType < 0) {
        SetEditing(Tab, false);
        TabClick(Sender);
        return;
    }

    SetEditing(Tab, true);
    switch (LastType) {
        case 0:
            if (GenreEnglish->Count() == 0) return;
            SetRowCount(Tab, 1 + GenreEnglish->Count());
            for (I = 0; I < GenreEnglish->Count(); I++) { SetCell(Tab, 0, 1 + I, (*GenreEnglish)[I]); SetCell(Tab, 1, 1 + I, (*GenreCustom)[I]); }
            break;
        case 1:
            if (LanguageEnglish->Count() == 0) return;
            SetRowCount(Tab, 1 + LanguageEnglish->Count());
            for (I = 0; I < LanguageEnglish->Count(); I++) { SetCell(Tab, 0, 1 + I, (*LanguageEnglish)[I]); SetCell(Tab, 1, 1 + I, (*LanguageCustom)[I]); }
            break;
        case 2:
            if (LicenseEnglish->Count() == 0) return;
            SetRowCount(Tab, 1 + LicenseEnglish->Count());
            for (I = 0; I < LicenseEnglish->Count(); I++) { SetCell(Tab, 0, 1 + I, (*LicenseEnglish)[I]); SetCell(Tab, 1, 1 + I, (*LicenseCustom)[I]); }
            break;
    }

    TabClick(Sender);
}

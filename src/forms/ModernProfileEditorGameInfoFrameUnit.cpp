// C++ port of ModernProfileEditorGameInfoFrameUnit.pas (implementation section).
#include "ModernProfileEditorGameInfoFrameUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "HelpConsts.h"
#include "IconLoaderUnit.h"
#include "TextEditPopupUnit.h"
#include "DataReaderFormUnit.h"
#include "PrgSetupUnit.h"
#include "LinkFileEditFormUnit.h"

// Memo.Lines.Assign(St)
static void SetMemoLines(QTextEdit* Memo, TStringList* St)
{
    Memo->setPlainText(St->ToQStringList().join('\n'));
}

// Memo.Lines (as TStringList, caller deletes)
static TStringList* MemoLines(QTextEdit* Memo)
{
    TStringList* St = new TStringList();
    QString T = Memo->toPlainText();
    if (T != "") {
        QStringList L = T.split('\n');
        if (!L.isEmpty() && L.last().isEmpty()) L.removeLast();
        St->AddStrings(L);
    }
    return St;
}

// Replacement for TStringGridEx (ClassExtensions): cells can be edited with a
// drop down list (OnListForCell / OnGetListForCell).  Delphi grid coordinates
// (row 0 = fixed title row) are passed to the callbacks.
class TStringGridExDelegate : public QStyledItemDelegate
{
public:
    std::function<void(int, int, bool&)> OnListForCell;
    std::function<void(int, int, TStringList*)> OnGetListForCell;
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* Parent, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override
    {
        bool UseList = false;
        if (OnListForCell) OnListForCell(Index.column(), Index.row() + 1, UseList);
        if (!UseList) return QStyledItemDelegate::createEditor(Parent, Option, Index);
        auto* C = new QComboBox(Parent);
        C->setEditable(true);
        C->setInsertPolicy(QComboBox::NoInsert);
        if (OnGetListForCell) {
            TStringList St;
            OnGetListForCell(Index.column(), Index.row() + 1, &St);
            C->addItems(St.ToQStringList());
        }
        return C;
    }
    void setEditorData(QWidget* Editor, const QModelIndex& Index) const override
    {
        if (auto* C = qobject_cast<QComboBox*>(Editor)) { C->setCurrentText(Index.data(Qt::EditRole).toString()); return; }
        QStyledItemDelegate::setEditorData(Editor, Index);
    }
    void setModelData(QWidget* Editor, QAbstractItemModel* Model, const QModelIndex& Index) const override
    {
        if (auto* C = qobject_cast<QComboBox*>(Editor)) { Model->setData(Index, C->currentText(), Qt::EditRole); return; }
        QStyledItemDelegate::setModelData(Editor, Model, Index);
    }
};

/* TModernProfileEditorGameInfoFrame */

TModernProfileEditorGameInfoFrame::TModernProfileEditorGameInfoFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ImageList = new TImageList(16, 16);

    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { DelButtonClick(ui.DelButton); });
    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { AddButtonClick(ui.AddButton); });
    connect(ui.GameInfoValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this]() { GameInfoValueListEditorEditButtonClick(ui.GameInfoValueListEditor); });
    connect(ui.SearchGameButton, &QAction::triggered, this, [this]() { SearchClick(ui.SearchGameButton); });
    connect(ui.DownloadDataButton, &QAction::triggered, this, [this]() { SearchClick(ui.DownloadDataButton); });

    WWWNames = new TStringList();
    WWWLinks = new TStringList();
}

TModernProfileEditorGameInfoFrame::~TModernProfileEditorGameInfoFrame()
{
    delete WWWNames;
    delete WWWLinks;
    delete ImageList;
}

void TModernProfileEditorGameInfoFrame::resizeEvent(QResizeEvent* E)
{
    QWidget::resizeEvent(E);
    FrameResize(this);
}

QString TModernProfileEditorGameInfoFrame::TabCell(int ACol, int ARow) const
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.Tab->horizontalHeaderItem(ACol);
        return (H != nullptr) ? H->text() : QString();
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    return (It != nullptr) ? It->text() : QString();
}

void TModernProfileEditorGameInfoFrame::SetTabCell(int ACol, int ARow, const QString& S)
{
    if (ARow == 0) {
        ui.Tab->setHorizontalHeaderItem(ACol, new QTableWidgetItem(S));
        return;
    }
    QTableWidgetItem* It = ui.Tab->item(ARow - 1, ACol);
    if (It == nullptr) ui.Tab->setItem(ARow - 1, ACol, new QTableWidgetItem(S)); else It->setText(S);
}

int TModernProfileEditorGameInfoFrame::TabRowCount() const
{
    return ui.Tab->rowCount() + 1;
}

void TModernProfileEditorGameInfoFrame::SetTabRowCount(int Count)
{
    ui.Tab->setRowCount(Max(0, Count - 1));
}

void TModernProfileEditorGameInfoFrame::InitGUI(TModernProfileEditorInitData& InitData)
{
    TStringList* St;
    TValueListEditorWidget* E = ui.GameInfoValueListEditor;

    InitData.AllowDefaultValueReset = false;

    // NoFlicker(...): not needed with Qt

    SetRichEditPopup(ui.NotesMemo);

    E->setTitleCaptions(QStringList{LanguageSetup->Key(), LanguageSetup->Value()});
    if (E->rowCount() > 0) E->deleteRow(0);
    E->insertRow(LanguageSetup->GameGenre(), "");
    St = ExtGenreList(GetCustomGenreName(InitData.GameDB->GetGenreList())); E->setPickList(E->rowCount() - 1, St->ToQStringList()); delete St;
    E->insertRow(LanguageSetup->GameDeveloper(), "");
    St = InitData.GameDB->GetDeveloperList(); E->setPickList(E->rowCount() - 1, St->ToQStringList()); delete St;
    E->insertRow(LanguageSetup->GamePublisher(), "");
    St = InitData.GameDB->GetPublisherList(); E->setPickList(E->rowCount() - 1, St->ToQStringList()); delete St;
    E->insertRow(LanguageSetup->GameYear(), "");
    St = InitData.GameDB->GetYearList(); E->setPickList(E->rowCount() - 1, St->ToQStringList()); delete St;
    E->insertRow(LanguageSetup->GameLanguage(), "");
    St = ExtLanguageList(GetCustomLanguageName(InitData.GameDB->GetLanguageList())); E->setPickList(E->rowCount() - 1, St->ToQStringList()); delete St;
    E->insertRow(LanguageSetup->GameWWW(), "");
    E->setEllipsis(E->rowCount() - 1, true);
    E->insertRow(LanguageSetup->GameLicense(), "");
    St = ExtLicenseList(GetCustomLicenseName(InitData.GameDB->GetLicenseList())); E->setPickList(E->rowCount() - 1, St->ToQStringList()); delete St;

    ui.DownloadDataButton->setText(LanguageSetup->DataReaderButton());
    ui.FavouriteCheckBox->setText(LanguageSetup->GameFavorite());
    {
        int L = E->x() + E->width() - ui.FavouriteCheckBox->fontMetrics().horizontalAdvance(ui.FavouriteCheckBox->text()) - 20;
        VclLayout::setBounds(ui.FavouriteCheckBox, L, ui.FavouriteCheckBox->y(), ui.FavouriteCheckBox->width(), ui.FavouriteCheckBox->height());
    }

    ui.MultiValueInfoLabel->setText(LanguageSetup->ProfileEditorMultipleValuesInfo());

    ui.UserDefinedDataLabel->setText(LanguageSetup->ProfileEditorUserDefinedInfo() + ":");

    // Tab:=TStringGrid(NewWinControlType(Tab,TStringGridEx,ctcmCopyProperties))
    auto* Delegate = new TStringGridExDelegate(ui.Tab);
    Delegate->OnListForCell = [this](int ACol, int ARow, bool& B) { TabListForCell(ACol, ARow, B); };
    Delegate->OnGetListForCell = [this](int ACol, int ARow, TStringList* L) { TabGetListForCell(ACol, ARow, L); };
    ui.Tab->setItemDelegate(Delegate);

    SetTabCell(0, 0, LanguageSetup->Key());
    SetTabCell(1, 0, LanguageSetup->Value());
    ui.Tab->verticalHeader()->setDefaultSectionSize(E->verticalHeader()->defaultSectionSize());
    ui.AddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.DelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));

    ui.NotesLabel->setText(LanguageSetup->GameNotes() + ":");

    UserIconLoader->DialogImage(DI_Internet, ImageList, 0);
    UserIconLoader->DialogImage(DI_Edit, ImageList, 1);
    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    if (ImageList->Count() > 0) {
        ui.SearchGameButton->setIcon(ImageList->Icon(0));
        ui.DownloadDataButton->setIcon(ImageList->Icon(0));
    }

    PProfileName = InitData.CurrentProfileName;
    PCaptureDir = InitData.CurrentCaptureDir;
    LinkFile = InitData.SearchLinkFile;
    GameDB = InitData.GameDB;

    LoadLinks();

    ProfileExe = InitData.CurrentProfileExe;
    ProfileSetup = InitData.CurrentProfileSetup;
    ProfileScummVMGameName = InitData.CurrentScummVMGameName;
    ProfileScummVMPath = InitData.CurrentScummVMPath;
    ProfileDOSBoxInstallation = InitData.CurrentDOSBoxInstallation;
    ProfileCaptureDir = InitData.CurrentCaptureDir;
    FOnProfileNameChange = InitData.OnProfileNameChange;

    setProperty("HelpContext", ID_ProfileEditProgramInformation);  // HelpContext
}

void TModernProfileEditorGameInfoFrame::LoadLinks()
{
    LinkFile->AddLinksToMenu(ui.SearchPopupMenu, 0, 1, [this](QObject* Sender) { SearchClick(Sender); }, 1);
    // SearchPopupMenu.Images=ImageList: edit item uses image 1
    if (ImageList->Count() > 1) for (QAction* A : ui.SearchPopupMenu->actions()) if (!A->isSeparator() && A->property("tag").toInt() == 1) A->setIcon(ImageList->Icon(1));
    ui.SearchGameButton->setText(LanguageSetup->ProfileEditorLookUpGame() + " " + LinkFile->Name(0));
    ui.SearchGameButton->setToolTip(LinkFile->Link(0));
}

void TModernProfileEditorGameInfoFrame::SearchClick(QObject* Sender)
{
    QString S, Name, Genre, Developer, Publisher, Year, Internet, Notes;
    TValueListEditorWidget* E = ui.GameInfoValueListEditor;

    switch (Sender->property("tag").toInt()) {
    case 0:
        if (auto* A = qobject_cast<QAction*>(Sender)) LinkFile->MoveToTop(A->text());
        LoadLinks();
        break;
    case 1: if (LinkFile->EditFile(false)) LoadLinks(); break;
    case 2: OpenLink(LinkFile->Link(0), "<GAMENAME>", *PProfileName); break;
    case 3:
        if (Trim(*PCaptureDir) == "") S = ""; else S = MakeAbsPath(*PCaptureDir, PrgSetup->BaseDir());
        if (ShowDataReaderDialog(this, *PProfileName, Name, Genre, Developer, Publisher, Year, Internet, Notes, S)) {
            if (Name != "") FOnProfileNameChange(this, Name, *ProfileExe, *ProfileSetup, *ProfileScummVMGameName, *ProfileScummVMPath, *ProfileDOSBoxInstallation, *ProfileCaptureDir);
            if (Genre != "") E->setValue(0, Genre);
            if (Developer != "") E->setValue(1, Developer);
            if (Publisher != "") E->setValue(2, Publisher);
            if (Year != "") E->setValue(3, Year);
            if (Internet != "" && Trim(E->value(5)) == "") E->setValue(5, Internet);
            if (Notes != "") ui.NotesMemo->append(Notes);
        }
        break;
    }
}

void TModernProfileEditorGameInfoFrame::SetGame(TGame* Game, bool LoadFromTemplate)
{
    Q_UNUSED(LoadFromTemplate);
    TStringList* St;
    int I, J, C;
    QString S, T;
    TValueListEditorWidget* E = ui.GameInfoValueListEditor;

    if (Game == nullptr) {
        E->setValue(6, RemoveUnderline(LanguageSetup->No()));
        return;
    }

    if (Game->Genre() != "") E->setValue(0, GetCustomGenreName(Game->Genre())); else E->setValue(0, "");
    if (Game->Developer() != "") E->setValue(1, Game->Developer()); else E->setValue(1, "");
    if (Game->Publisher() != "") E->setValue(2, Game->Publisher()); else E->setValue(2, "");
    if (Game->Year() != "") E->setValue(3, Game->Year()); else E->setValue(3, "");
    if (Game->Language() != "") E->setValue(4, GetCustomLanguageName(Game->Language())); else E->setValue(4, "");
    if (Game->WWW(1) != "") E->setValue(5, Game->WWW(1)); else E->setValue(5, "");
    if (Game->License() != "") E->setValue(6, GetCustomLicenseName(Game->License())); else E->setValue(6, "");
    ui.FavouriteCheckBox->setChecked(Game->Favorite());
    for (I = 1; I <= 9; I++) { WWWNames->Add(Game->WWWName(I)); WWWLinks->Add(Game->WWW(I)); }

    St = StringToStringList(Game->UserInfo());
    SetTabRowCount(Max(St->Count(), 1) + 1);
    SetTabCell(0, 1, ""); SetTabCell(1, 1, ""); C = 1;
    for (I = 0; I < St->Count(); I++) {
        S = (*St)[I];
        J = Pos("=", S);
        if (J == 0) T = ""; else { T = Trim(Copy(S, J + 1, MaxInt)); S = Trim(Copy(S, 1, J - 1)); }
        if (Trim(ExtUpperCase(S)) == "LICENSE") continue;
        SetTabCell(0, C, S);
        SetTabCell(1, C, T);
        C++;
    }
    SetTabRowCount(Max(2, C));
    delete St;

    St = StringToStringList(Game->Notes());
    SetMemoLines(ui.NotesMemo, St);
    delete St;

    FrameResize(this);
}

void TModernProfileEditorGameInfoFrame::GetGame(TGame* Game)
{
    TStringList* St;
    int I;
    QString S, T, License;
    TValueListEditorWidget* E = ui.GameInfoValueListEditor;

    Game->SetGenre(GetEnglishGenreName(E->value(0)));
    Game->SetDeveloper(E->value(1));
    Game->SetPublisher(E->value(2));
    Game->SetYear(E->value(3));
    Game->SetLanguage(GetEnglishLanguageName(E->value(4)));
    Game->SetWWW(1, E->value(5));
    License = GetEnglishLicenseName(E->value(6));
    Game->SetFavorite(ui.FavouriteCheckBox->isChecked());
    if (WWWNames->Count() > 0) Game->SetWWWName(1, (*WWWNames)[0]); else Game->SetWWWName(1, "");
    for (I = 1; I <= Min(8, Min(WWWNames->Count(), WWWLinks->Count())); I++) { Game->SetWWWName(I + 1, (*WWWNames)[I]); Game->SetWWW(I + 1, (*WWWLinks)[I]); }

    St = new TStringList();
    if (License != "") St->Add("License=" + License);
    for (I = 1; I < TabRowCount(); I++) {
        S = Trim(TabCell(0, I)); T = Trim(TabCell(1, I));
        if (S != "" || T != "") St->Add(S + "=" + T);
    }
    if (St->Count() == 0) Game->SetUserInfo(""); else Game->SetUserInfo(StringListToString(St));
    delete St;

    St = MemoLines(ui.NotesMemo);
    Game->SetNotes(StringListToString(St));
    delete St;
}

void TModernProfileEditorGameInfoFrame::FrameResize(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.Tab->setColumnWidth(0, ui.GameInfoValueListEditor->columnWidth(0));
    ui.Tab->setColumnWidth(1, Max(0, ui.Tab->viewport()->width() - 10 - ui.Tab->columnWidth(0)));
}

void TModernProfileEditorGameInfoFrame::AddButtonClick(QObject* Sender)
{
    TStringList* St;
    QString S;
    int I, J;
    QPoint P;
    int Tag = Sender->property("tag").toInt();

    if (Tag > 0) {
        if (Tag == 1 || Trim(TabCell(0, TabRowCount() - 1)) != "" || Trim(TabCell(1, TabRowCount() - 1)) != "") {
            SetTabRowCount(TabRowCount() + 1);
        }
        ui.Tab->setCurrentCell(TabRowCount() - 2, 0);
        if (Tag == 2) {
            if (auto* A = qobject_cast<QAction*>(Sender)) SetTabCell(0, TabRowCount() - 1, RemoveUnderline(A->text()));
            ui.Tab->setCurrentCell(TabRowCount() - 2, 1);
        }

        ui.Tab->setFocus();
        return;
    }

    St = GameDB->GetUserKeys();
    I = 0;
    while (I < St->Count()) {
        S = Trim(ExtUpperCase((*St)[I]));
        if (S == "LICENSE") { St->Delete(I); continue; }
        for (J = 1; J < TabRowCount(); J++) if (Trim(ExtUpperCase(TabCell(0, J))) == S) { St->Delete(I); I--; break; }
        I++;
    }
    if (St->Count() == 0) {
        SetTabRowCount(TabRowCount() + 1);
        ui.Tab->setCurrentCell(TabRowCount() - 2, 0);
        ui.Tab->setFocus();
        delete St;
        return;
    } else {
        ui.AddUserDataPopupMenu->clear();
        QAction* M = ui.AddUserDataPopupMenu->addAction(LanguageSetup->AddNewEmptyLine());
        M->setProperty("tag", 1);
        connect(M, &QAction::triggered, this, [this, M]() { AddButtonClick(M); });
        ui.AddUserDataPopupMenu->addSeparator();
        for (I = 0; I < St->Count(); I++) {
            M = ui.AddUserDataPopupMenu->addAction(MaskUnderlineAmpersand((*St)[I]));
            M->setProperty("tag", 2);
            connect(M, &QAction::triggered, this, [this, M]() { AddButtonClick(M); });
        }
    }
    delete St;

    if (auto* W = qobject_cast<QWidget*>(Sender)) P = W->mapToGlobal(QPoint(0, 0)); else P = QCursor::pos();
    ui.AddUserDataPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TModernProfileEditorGameInfoFrame::DelButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    int I, Row;

    if (TabRowCount() == 2) {
        SetTabCell(0, 1, "");
        SetTabCell(1, 1, "");
        return;
    }

    Row = (ui.Tab->currentRow() < 0) ? -1 : ui.Tab->currentRow() + 1;
    if (Row < 0) {
        MessageDlg(LanguageSetup->MessageNoRecordSelected(), mtError, mbOK, 0);
        return;
    }

    for (I = Row + 1; I < TabRowCount(); I++) {
        SetTabCell(0, I - 1, TabCell(0, I));
        SetTabCell(1, I - 1, TabCell(1, I));
    }
    SetTabRowCount(TabRowCount() - 1);
}

void TModernProfileEditorGameInfoFrame::TabListForCell(int ACol, int ARow, bool& UseDropdownListForCell)
{
    Q_UNUSED(ARow);
    UseDropdownListForCell = (ACol == 1);
}

void TModernProfileEditorGameInfoFrame::TabGetListForCell(int ACol, int ARow, TStringList* DropdownListForCell)
{
    Q_UNUSED(ACol);
    TStringList* St;

    DropdownListForCell->Clear();
    St = GameDB->GetKeyValueList(TabCell(0, ARow));
    DropdownListForCell->AddStrings(*St);
    delete St;
}

void TModernProfileEditorGameInfoFrame::GameInfoValueListEditorEditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    (*WWWLinks)[0] = ui.GameInfoValueListEditor->value(5);
    ShowLinkFileEditDialog(this, WWWNames, WWWLinks, false, true, -1);
    if ((*WWWLinks)[0] != "") ui.GameInfoValueListEditor->setValue(5, (*WWWLinks)[0]);
}

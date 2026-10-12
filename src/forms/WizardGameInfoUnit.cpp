// C++ port of WizardGameInfoUnit.pas (implementation section).
#include "WizardGameInfoUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "IconLoaderUnit.h"
#include "TextEditPopupUnit.h"
#include "DataReaderFormUnit.h"
#include "GameDBToolsUnit.h"
#include "PrgSetupUnit.h"
#include "LinkFileEditFormUnit.h"

namespace {

using ListForCellFn = std::function<void(int, int, bool &)>;          // TListForCellEvent
using GetListForCellFn = std::function<void(int, int, TStringList *)>;  // TGetListForCellEvent

// TStringGrid with FixedRows=1: row 0 is the horizontal header of the QTableWidget.
QString Cell(QTableWidget *Tab, int ACol, int ARow)
{
    if (ARow == 0) {
        QTableWidgetItem *H = Tab->horizontalHeaderItem(ACol);
        return H ? H->text() : QString();
    }
    QTableWidgetItem *It = Tab->item(ARow - 1, ACol);
    return It ? It->text() : QString();
}

void SetCell(QTableWidget *Tab, int ACol, int ARow, const QString &S)
{
    if (ARow == 0) {
        QTableWidgetItem *H = Tab->horizontalHeaderItem(ACol);
        if (!H) { H = new QTableWidgetItem(); Tab->setHorizontalHeaderItem(ACol, H); }
        H->setText(S);
        return;
    }
    QTableWidgetItem *It = Tab->item(ARow - 1, ACol);
    if (!It) { It = new QTableWidgetItem(); Tab->setItem(ARow - 1, ACol, It); }
    It->setText(S);
}

int RowCount(QTableWidget *Tab) { return Tab->rowCount() + 1; }
void SetRowCount(QTableWidget *Tab, int N) { Tab->setRowCount(qMax(0, N - 1)); }
int Row(QTableWidget *Tab) { return Tab->currentRow() < 0 ? -1 : Tab->currentRow() + 1; }
void SetRowHeight(QTableWidget *Tab, int ARow, int H)
{
    if (ARow == 0) Tab->horizontalHeader()->setFixedHeight(H); else Tab->setRowHeight(ARow - 1, H);
}

// TStringGridEx.OnListForCell / OnGetListForCell: drop down list editor for cells
class ListForCellDelegate : public QStyledItemDelegate
{
public:
    ListForCellDelegate(QObject *Parent, ListForCellFn AOnListForCell, GetListForCellFn AOnGetListForCell)
        : QStyledItemDelegate(Parent), FOnListForCell(std::move(AOnListForCell)), FOnGetListForCell(std::move(AOnGetListForCell)) {}

    QWidget *createEditor(QWidget *Parent, const QStyleOptionViewItem &Option, const QModelIndex &Index) const override
    {
        bool UseDropdownListForCell = false;
        const int ACol = Index.column(), ARow = Index.row() + 1;   // VCL coordinates (fixed row 0)
        if (FOnListForCell) FOnListForCell(ACol, ARow, UseDropdownListForCell);
        if (!UseDropdownListForCell) return QStyledItemDelegate::createEditor(Parent, Option, Index);
        auto *C = new QComboBox(Parent);
        C->setEditable(true);
        C->setInsertPolicy(QComboBox::NoInsert);
        TStringList Items;
        if (FOnGetListForCell) FOnGetListForCell(ACol, ARow, &Items);
        C->addItems(Items.ToQStringList());
        return C;
    }

    void setEditorData(QWidget *Editor, const QModelIndex &Index) const override
    {
        if (auto *C = qobject_cast<QComboBox *>(Editor)) { C->setEditText(Index.data(Qt::EditRole).toString()); return; }
        QStyledItemDelegate::setEditorData(Editor, Index);
    }

    void setModelData(QWidget *Editor, QAbstractItemModel *Model, const QModelIndex &Index) const override
    {
        if (auto *C = qobject_cast<QComboBox *>(Editor)) { Model->setData(Index, C->currentText(), Qt::EditRole); return; }
        QStyledItemDelegate::setModelData(Editor, Model, Index);
    }

private:
    ListForCellFn FOnListForCell;
    GetListForCellFn FOnGetListForCell;
};

void SetBold(QWidget *W)
{
    QFont F = W->font(); F.setBold(true); W->setFont(F);
}

QStringList MemoLines(QTextEdit *Memo)
{
    const QString S = Memo->toPlainText();
    if (S.isEmpty()) return QStringList();
    return S.split('\n');
}

}  // namespace

// TWizardGameInfoFrame

TWizardGameInfoFrame::TWizardGameInfoFrame(QWidget* AOwner) : QWidget(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    WWWNames = new TStringList;
    WWWLinks = new TStringList;

    connect(ui.AddButton, &QToolButton::clicked, this, [this]() { AddButtonClick(ui.AddButton); });
    connect(ui.DelButton, &QToolButton::clicked, this, [this]() { DelButtonClick(ui.DelButton); });
    connect(ui.GameInfoValueListEditor, &TValueListEditorWidget::editButtonClicked, this, [this](int) { GameInfoValueListEditorEditButtonClick(ui.GameInfoValueListEditor); });
    connect(ui.SearchGameButton, &QAction::triggered, this, [this]() { SearchClick(ui.SearchGameButton); });
    connect(ui.DownloadDataButton, &QAction::triggered, this, [this]() { SearchClick(ui.DownloadDataButton); });
}

TWizardGameInfoFrame::~TWizardGameInfoFrame()
{
    delete WWWNames;
    delete WWWLinks;
    delete FImageList;
}

void TWizardGameInfoFrame::Init(TGameDB* AGameDB, TLinkFile* ALinkFile)
{
    TStringList* St;

    // SetVistaFonts(self); - not needed

    SetRichEditPopup(ui.NotesMemo);

    SetBold(ui.InfoLabel);

    ui.InfoLabel->setText(LanguageSetup->WizardFormPage4Info());
    ui.BaseName_EditLabel->setText(LanguageSetup->WizardFormBaseName());
    ui.GameInfoValueListEditor->setTitleCaptions(QStringList() << LanguageSetup->Key() << LanguageSetup->Value());
    {
        TValueListEditorWidget *E = ui.GameInfoValueListEditor;
        if (E->rowCount() > 0) E->deleteRow(0);
        int R;
        R = E->insertRow(LanguageSetup->GameGenre(), QString());
        St = ExtGenreList(GetCustomGenreName(AGameDB->GetGenreList())); E->setPickList(R, St->ToQStringList()); delete St;
        R = E->insertRow(LanguageSetup->GameDeveloper(), QString());
        St = AGameDB->GetDeveloperList(); E->setPickList(R, St->ToQStringList()); delete St;
        R = E->insertRow(LanguageSetup->GamePublisher(), QString());
        St = AGameDB->GetPublisherList(); E->setPickList(R, St->ToQStringList()); delete St;
        R = E->insertRow(LanguageSetup->GameYear(), QString());
        St = AGameDB->GetYearList(); E->setPickList(R, St->ToQStringList()); delete St;
        R = E->insertRow(LanguageSetup->GameLanguage(), QString());
        St = ExtLanguageList(GetCustomLanguageName(AGameDB->GetLanguageList())); E->setPickList(R, St->ToQStringList()); delete St;
        R = E->insertRow(LanguageSetup->GameWWW(), QString());
        E->setEllipsis(R, true);
        R = E->insertRow(LanguageSetup->GameLicense(), QString());
        St = ExtLicenseList(GetCustomLicenseName(AGameDB->GetLicenseList())); E->setPickList(R, St->ToQStringList()); delete St;
    }
    ui.DownloadDataButton->setText(LanguageSetup->DataReaderButton());
    ui.FavouriteCheckBox->setText(LanguageSetup->GameFavorite());
    VclLayout::setBounds(ui.FavouriteCheckBox,
        ui.GameInfoValueListEditor->x() + ui.GameInfoValueListEditor->width() - ui.FavouriteCheckBox->fontMetrics().horizontalAdvance(ui.FavouriteCheckBox->text()) - 20,
        ui.FavouriteCheckBox->y(), ui.FavouriteCheckBox->width(), ui.FavouriteCheckBox->height());

    ui.MultiValueInfoLabel->setText(LanguageSetup->ProfileEditorMultipleValuesInfo());

    ui.UserDefinedDataLabel->setText(LanguageSetup->ProfileEditorUserDefinedInfo() + ":");

    // Tab:=TStringGrid(NewWinControlType(Tab,TStringGridEx,ctcmCopyProperties));
    ui.Tab->setItemDelegate(new ListForCellDelegate(ui.Tab,
        [this](int ACol, int ARow, bool &UseDropdownListForCell) { TabListForCell(ACol, ARow, UseDropdownListForCell); },
        [this](int ACol, int ARow, TStringList *DropdownListForCell) { TabGetListForCell(ACol, ARow, DropdownListForCell); }));

    SetCell(ui.Tab, 0, 0, LanguageSetup->Key());
    SetCell(ui.Tab, 1, 0, LanguageSetup->Value());
    const int RowHeight0 = ui.GameInfoValueListEditor->rowCount() > 0 ? ui.GameInfoValueListEditor->rowHeight(0) : ui.GameInfoValueListEditor->verticalHeader()->defaultSectionSize();
    SetRowHeight(ui.Tab, 0, ui.GameInfoValueListEditor->horizontalHeader()->height());
    if (RowCount(ui.Tab) < 2) SetRowCount(ui.Tab, 2);
    SetRowHeight(ui.Tab, 1, RowHeight0);
    ui.AddButton->setToolTip(RemoveUnderline(LanguageSetup->Add()));
    ui.DelButton->setToolTip(RemoveUnderline(LanguageSetup->Del()));

    ui.NotesLabel->setText(LanguageSetup->GameNotes() + ":");

    ui.Tab->setColumnWidth(0, ui.GameInfoValueListEditor->columnWidth(0));
    ui.Tab->setColumnWidth(1, ui.Tab->viewport()->width() - 10 - ui.Tab->columnWidth(0));

    LinkFile = ALinkFile;
    LoadLinks();

    UserIconLoader->DialogImage(DI_Add, ui.AddButton);
    UserIconLoader->DialogImage(DI_Delete, ui.DelButton);
    if (FImageList == nullptr) FImageList = TImageList::FromResource("WizardGameInfoUnit", "ImageList");
    if (FImageList == nullptr) FImageList = new TImageList();
    UserIconLoader->DialogImage(DI_Internet, FImageList, 0);
    UserIconLoader->DialogImage(DI_Edit, FImageList, 1);
    // ImageIndex of the tool buttons (DFM: SearchGameButton=0, DownloadDataButton=0)
    if (FImageList->Count() > 0) {
        ui.SearchGameButton->setIcon(FImageList->Icon(0));
        ui.DownloadDataButton->setIcon(FImageList->Icon(0));
    }

    GameDB = AGameDB;
    TempCaptureDir.clear();
}

void TWizardGameInfoFrame::LoadLinks()
{
    LinkFile->AddLinksToMenu(ui.SearchPopupMenu, 0, 1, [this](QObject *S) { SearchClick(S); }, 1);
    ui.SearchGameButton->setText(LanguageSetup->ProfileEditorLookUpGame() + " " + LinkFile->Name(0));
    ui.SearchGameButton->setToolTip(LinkFile->Link(0));
}

void TWizardGameInfoFrame::SearchClick(QObject* Sender)
{
    QString Name, Genre, Developer, Publisher, Year, Internet, Notes;

    switch (Sender->property("tag").toInt()) {
    case 0:
        if (auto *A = qobject_cast<QAction *>(Sender)) LinkFile->MoveToTop(A->text());
        LoadLinks();
        break;
    case 1:
        if (LinkFile->EditFile(false)) LoadLinks();
        break;
    case 2:
        OpenLink(LinkFile->Link(0), "<GAMENAME>", ui.BaseName->text());
        break;
    case 3:
        TempCaptureDir = TempDir() + "DFR-TempCapture" + PathDelim; ForceDirectories(TempCaptureDir);
        if (ShowDataReaderDialog(this, ui.BaseName->text(), Name, Genre, Developer, Publisher, Year, Internet, Notes, TempCaptureDir)) {
            TValueListEditorWidget *E = ui.GameInfoValueListEditor;
            if (!Name.isEmpty()) ui.BaseName->setText(Name);
            if (!Genre.isEmpty()) E->setValue(0, Genre);
            if (!Developer.isEmpty()) E->setValue(1, Developer);
            if (!Publisher.isEmpty()) E->setValue(2, Publisher);
            if (!Year.isEmpty()) E->setValue(3, Year);
            if ((!Internet.isEmpty()) && (Trim(E->value(5)).isEmpty())) E->setValue(5, Internet);
            if (!Notes.isEmpty()) ui.NotesMemo->append(Notes);
        }
        break;
    }
}

void TWizardGameInfoFrame::SetGameName(const QString& AName, TGame* Template)
{
    TStringList* St;
    int I, J;
    QString S, T;

    ui.BaseName->setText(AName);

    if (Template != nullptr) {
        TValueListEditorWidget *E = ui.GameInfoValueListEditor;
        if (!Trim(Template->Genre()).isEmpty()) E->setValue(0, GetCustomGenreName(Template->Genre()));
        if (!Trim(Template->Developer()).isEmpty()) E->setValue(1, Template->Developer());
        if (!Trim(Template->Publisher()).isEmpty()) E->setValue(2, Template->Publisher());
        if (!Trim(Template->Year()).isEmpty()) E->setValue(3, Template->Year());
        if (!Trim(Template->Language()).isEmpty()) E->setValue(4, GetCustomLanguageName(Template->Language()));
        if (!Trim(Template->WWW(1)).isEmpty()) E->setValue(5, Template->WWW(1));
        if (!Trim(Template->License()).isEmpty()) E->setValue(6, GetCustomLicenseName(Template->License())); else E->setValue(6, QString());

        for (I = 1; I <= 9; I++) { WWWNames->Add(Template->WWWName(I)); WWWLinks->Add(Template->WWW(I)); }

        St = StringToStringList(Template->UserInfo());
        I = 0;
        while (I < St->Count()) {
            S = (*St)[I];
            J = Pos("=", S);
            if (J != 0) S = Trim(Copy(S, 1, J - 1));
            if (Trim(ExtUpperCase(S)) == "LICENSE") St->Delete(I); else I++;
        }
        if (St->Count() > 0) {
            SetRowCount(ui.Tab, St->Count() + 1);
            for (I = 0; I < St->Count(); I++) {
                S = (*St)[I];
                J = Pos("=", S);
                if (J == 0) T.clear(); else { T = Trim(Copy(S, J + 1, MaxInt)); S = Trim(Copy(S, 1, J - 1)); }
                SetCell(ui.Tab, 0, I + 1, S);
                SetCell(ui.Tab, 1, I + 1, T);
            }
        }
        delete St;

        St = StringToStringList(Template->Notes());
        ui.NotesMemo->setPlainText(St->ToQStringList().join("\n"));
        delete St;
    } else {
        for (I = 1; I <= 9; I++) { WWWNames->Add(QString()); WWWLinks->Add(QString()); }
    }
}

void TWizardGameInfoFrame::AddButtonClick(QObject* Sender)
{
    TStringList* St;
    QString S;
    int I, J;

    const int Tag = Sender->property("tag").toInt();
    const int RowHeight0 = ui.GameInfoValueListEditor->rowCount() > 0 ? ui.GameInfoValueListEditor->rowHeight(0) : ui.GameInfoValueListEditor->verticalHeader()->defaultSectionSize();

    if (Tag > 0) {
        if ((Tag == 1) || (!Trim(Cell(ui.Tab, 0, RowCount(ui.Tab) - 1)).isEmpty()) || (!Trim(Cell(ui.Tab, 1, RowCount(ui.Tab) - 1)).isEmpty())) {
            SetRowCount(ui.Tab, RowCount(ui.Tab) + 1);
            SetRowHeight(ui.Tab, RowCount(ui.Tab) - 1, RowHeight0);
        }
        ui.Tab->setCurrentCell(RowCount(ui.Tab) - 2, 0);
        if (Tag == 2) {
            if (auto *A = qobject_cast<QAction *>(Sender)) SetCell(ui.Tab, 0, RowCount(ui.Tab) - 1, RemoveUnderline(A->text()));
            ui.Tab->setCurrentCell(RowCount(ui.Tab) - 2, 1);
        }

        ui.Tab->setFocus();
        return;
    }

    St = GameDB->GetUserKeys();
    I = 0;
    while (I < St->Count()) {
        S = Trim(ExtUpperCase((*St)[I]));
        if (S == "LICENSE") { St->Delete(I); continue; }
        for (J = 1; J <= RowCount(ui.Tab) - 1; J++) if (Trim(ExtUpperCase(Cell(ui.Tab, 0, J))) == S) { St->Delete(I); I--; break; }
        I++;
    }
    if (St->Count() == 0) {
        delete St;
        SetRowCount(ui.Tab, RowCount(ui.Tab) + 1);
        SetRowHeight(ui.Tab, RowCount(ui.Tab) - 1, RowHeight0);
        ui.Tab->setCurrentCell(RowCount(ui.Tab) - 2, 0);
        ui.Tab->setFocus();
        return;
    } else {
        ui.AddUserDataPopupMenu->clear();
        QAction *M = ui.AddUserDataPopupMenu->addAction(LanguageSetup->AddNewEmptyLine());
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

    QPoint P(0, 0);
    if (auto *W = qobject_cast<QWidget *>(Sender)) P = mapToGlobal(QPoint(W->x(), W->y()));
    ui.AddUserDataPopupMenu->popup(QPoint(P.x() + 5, P.y() + 5));
}

void TWizardGameInfoFrame::DelButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);

    if (RowCount(ui.Tab) == 2) {
        SetCell(ui.Tab, 0, 1, QString());
        SetCell(ui.Tab, 1, 1, QString());
        return;
    }

    if (Row(ui.Tab) < 0) {
        MessageDlg(LanguageSetup->MessageNoRecordSelected(), mtError, mbOK, 0);
        return;
    }

    for (int I = Row(ui.Tab) + 1; I <= RowCount(ui.Tab) - 1; I++) {
        SetCell(ui.Tab, 0, I - 1, Cell(ui.Tab, 0, I));
        SetCell(ui.Tab, 1, I - 1, Cell(ui.Tab, 1, I));
    }
    SetRowCount(ui.Tab, RowCount(ui.Tab) - 1);
}

void TWizardGameInfoFrame::WriteDataToGame(TGame* Game)
{
    int I;
    QString S, T, License;

    if (!ui.BaseName->text().isEmpty()) Game->SetName(ui.BaseName->text());

    {
        TValueListEditorWidget *E = ui.GameInfoValueListEditor;
        Game->SetGenre(GetEnglishGenreName(E->value(0)));
        Game->SetDeveloper(E->value(1));
        Game->SetPublisher(E->value(2));
        Game->SetYear(E->value(3));
        Game->SetLanguage(GetEnglishLanguageName(E->value(4)));
        Game->SetWWW(1, E->value(5));
        License = Trim(E->value(6));
    }
    Game->SetFavorite(ui.FavouriteCheckBox->isChecked());
    if (WWWNames->Count() > 0) Game->SetWWWName(1, (*WWWNames)[0]); else Game->SetWWWName(1, QString());
    for (I = 1; I <= Min(8, Min(WWWNames->Count(), WWWLinks->Count())); I++) {
        // WWWNames/WWWLinks have at least I+1 entries only if Count>I
        if (I >= WWWNames->Count() || I >= WWWLinks->Count()) break;
        Game->SetWWWName(I + 1, (*WWWNames)[I]); Game->SetWWW(I + 1, (*WWWLinks)[I]);
    }

    {
        TStringList St;
        if (!License.isEmpty()) St.Add("License=" + License);
        for (I = 1; I <= RowCount(ui.Tab) - 1; I++) {
            S = Trim(Cell(ui.Tab, 0, I)); T = Trim(Cell(ui.Tab, 1, I));
            if ((!S.isEmpty()) || (!T.isEmpty())) St.Add(S + "=" + T);
        }
        if (St.Count() == 0) Game->SetUserInfo(QString()); else Game->SetUserInfo(StringListToString(&St));
    }

    {
        TStringList Lines(MemoLines(ui.NotesMemo));
        Game->SetNotes(StringListToString(&Lines));
    }

    if (!TempCaptureDir.isEmpty()) {
        if (!Trim(Game->CaptureFolder()).isEmpty()) {
            S = MakeAbsPath(Game->CaptureFolder(), PrgSetup->BaseDir());
            ForceDirectories(S);
            CopyFiles(TempCaptureDir, S, false, true);
        }
        ExtDeleteFolder(TempCaptureDir, ftTemp);
    }
}

void TWizardGameInfoFrame::TabListForCell(int ACol, int ARow, bool& UseDropdownListForCell)
{
    Q_UNUSED(ARow);
    UseDropdownListForCell = (ACol == 1);
}

void TWizardGameInfoFrame::TabGetListForCell(int ACol, int ARow, TStringList* DropdownListForCell)
{
    Q_UNUSED(ACol);
    DropdownListForCell->Clear();
    TStringList* St = GameDB->GetKeyValueList(Cell(ui.Tab, 0, ARow));
    DropdownListForCell->AddStrings(*St);
    delete St;
}

void TWizardGameInfoFrame::GameInfoValueListEditorEditButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    (*WWWLinks)[0] = ui.GameInfoValueListEditor->value(5);
    ShowLinkFileEditDialog(this, WWWNames, WWWLinks, false, true, -1);
    if (!(*WWWLinks)[0].isEmpty()) ui.GameInfoValueListEditor->setValue(5, (*WWWLinks)[0]);
}

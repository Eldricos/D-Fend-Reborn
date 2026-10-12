// C++ port of DataReaderFormUnit.pas (implementation section).
#include "DataReaderFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "InternetDataWaitFormUnit.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "DataReaderMobyUnit.h"
#include "DataReaderTheGamesDBUnit.h"

TDataReaderForm* DataReaderForm = nullptr;

static void SetBold(QWidget* W)
{
    QFont F = W->font();
    F.setBold(true);
    W->setFont(F);
}

TDataReaderForm::TDataReaderForm(QWidget* AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);
    // DFM event wiring
    connect(ui.SearchButton, &QPushButton::clicked, this, [this]() { SearchButtonClick(ui.SearchButton); });
    connect(ui.ListBox, &QListWidget::currentRowChanged, this, [this]() { ListBoxClick(ui.ListBox); });
    connect(ui.DownloadCoverCheckBox, &QCheckBox::toggled, this, [this]() { DownloadCoverCheckBoxClick(ui.DownloadCoverCheckBox); });
    connect(ui.InsertButton, &QPushButton::clicked, this, [this]() { InsertButtonClick(ui.InsertButton); });
    connect(ui.SearchTypeCheckBox, &QCheckBox::toggled, this, [this]() { SearchTypeCheckBoxClick(ui.SearchTypeCheckBox); });
    connect(ui.SourceComboBox, &QComboBox::activated, this, [this]() { SourceComboBoxChange(ui.SourceComboBox); });
    // KeyPreview = True, OnKeyDown = FormKeyDown
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        int K = Key;
        FormKeyDown(this, K, Mods);
        return (K == 0);
    });
    FormCreate(this);   // OnCreate
}

TDataReaderForm::~TDataReaderForm()
{
    FormDestroy(this);   // OnDestroy
}

void TDataReaderForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    if (!E->spontaneous()) FormShow(this);   // OnShow
}

void TDataReaderForm::FormCreate(QObject* Sender)
{
    Q_UNUSED(Sender);
    ShowCompleted = false;
    GenreSt = new TStringList;
    DeveloperSt = new TStringList;
    PublisherSt = new TStringList;
    YearSt = new TStringList;
    CoverSt = new TStringList;
    NotesSt = new TStringList;

    DataReader = new TTheGamesDBDataReader;
    ConfigOK = ShowDataReaderInternetConfigWaitDialog(parentWidget(), DataReader, false, LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError());
}

void TDataReaderForm::FormShow(QObject* Sender)
{
    QString S;
    // SetVistaFonts / Font.Charset: not needed with Qt

    SetBold(ui.NameLabel);
    SetBold(ui.GenreLabel);
    SetBold(ui.DeveloperLabel);
    SetBold(ui.PublisherLabel);
    SetBold(ui.YearLabel);
    ui.NameLabel->setText("");
    ui.GenreLabel->setText("");
    ui.DeveloperLabel->setText("");
    ui.PublisherLabel->setText("");
    ui.YearLabel->setText("");

    setWindowTitle(LanguageSetup->DataReader());
    ui.SourceLabel->setText(LanguageSetup->DataReaderSource());
    ui.GameNameEdit_EditLabel->setText(LanguageSetup->GameName());
    ui.SearchButton->setText(LanguageSetup->Search());
    ui.SearchTypeCheckBox->setText(LanguageSetup->DataReaderSearchForDOSGames());
    ui.SearchResultsLabel->setText(LanguageSetup->DataReaderSearchResults());
    ui.GameDataBox->setTitle(LanguageSetup->DataReaderNoSearchResults());
    ui.NameCheckBox->setText(LanguageSetup->GameName());
    ui.GenreCheckBox->setText(LanguageSetup->GameGenre());
    ui.DeveloperCheckBox->setText(LanguageSetup->GameDeveloper());
    ui.PublisherCheckBox->setText(LanguageSetup->GamePublisher());
    ui.YearCheckBox->setText(LanguageSetup->GameYear());
    ui.DownloadCoverCheckBox->setText(LanguageSetup->DataReaderCoverCheckbox());
    ui.DownloadCoverAllCheckBox->setText(LanguageSetup->DataReaderCoverAllCheckbox());
    ui.DescriptionCheckBox->setText(LanguageSetup->CaptureNotes());

    ui.InsertButton->setText(LanguageSetup->DataReaderInsert());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_FindFile, ui.SearchButton);
    UserIconLoader->DialogImage(DI_OK, ui.InsertButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    ui.SourceComboBox->setCurrentIndex(Min(ui.SourceComboBox->count() - 1, Max(0, PrgSetup->DataReaderSource())));
    SourceComboBoxChange(nullptr);

    ui.SearchTypeCheckBox->setChecked(!PrgSetup->DataReaderAllPlatforms());

    if (!Trim(ui.GameNameEdit->text()).isEmpty()) SearchButtonClick(Sender); else ListBoxClick(Sender);
    ShowCompleted = true;

    S = Trim(PrgSetup->DataReaderActiveSettings());
    while (Length(S) < 7) S = S + "X";
    if (Length(S) > 7) S = Copy(S, 1, 7);
    // S[8] reads behind the 7 chars in Delphi (#0, i.e. "not '-'")
    auto Ch = [&S](int I) { return (I <= Length(S)) ? S[I - 1] : QChar(0); };
    ui.NameCheckBox->setChecked(Ch(1) != '-');
    ui.GenreCheckBox->setChecked(Ch(2) != '-');
    ui.DeveloperCheckBox->setChecked(Ch(3) != '-');
    ui.PublisherCheckBox->setChecked(Ch(4) != '-');
    ui.YearCheckBox->setChecked(Ch(5) != '-');
    ui.DownloadCoverCheckBox->setChecked(Ch(6) != '-');
    ui.DownloadCoverAllCheckBox->setEnabled(ui.DownloadCoverCheckBox->isChecked());
    ui.DownloadCoverAllCheckBox->setChecked(Ch(7) != '-');
    ui.DescriptionCheckBox->setChecked(Ch(8) != '-');

    ui.SearchTypeCheckBox->setVisible(ExtUpperCase(Trim(DataReader->GetListURL(false))) != ExtUpperCase(Trim(DataReader->GetListURL(true))));
}

void TDataReaderForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    PrgSetup->SetDataReaderSource(ui.SourceComboBox->currentIndex());

    S = "";
    if (ui.NameCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.GenreCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.DeveloperCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.PublisherCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.YearCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.DownloadCoverCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.DownloadCoverAllCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.DescriptionCheckBox->isChecked()) S = S + "X"; else S = S + "-";

    PrgSetup->SetDataReaderActiveSettings(S);

    delete GenreSt; GenreSt = nullptr;
    delete DeveloperSt; DeveloperSt = nullptr;
    delete PublisherSt; PublisherSt = nullptr;
    delete YearSt; YearSt = nullptr;
    delete CoverSt; CoverSt = nullptr;
    delete NotesSt; NotesSt = nullptr;
    delete DataReader; DataReader = nullptr;
}

void TDataReaderForm::DownloadCoverCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ui.DownloadCoverAllCheckBox->setEnabled(ui.DownloadCoverCheckBox->isChecked() && ui.DownloadCoverCheckBox->isEnabled());
}

void TDataReaderForm::FormKeyDown(QObject* Sender, int& Key, TShiftState Shift)
{
    Q_UNUSED(Shift);
    if ((Key != Qt::Key_Return) && (Key != Qt::Key_Enter)) return;

    if (ui.GameNameEdit->hasFocus()) {
        SearchButtonClick(Sender);
    } else {
        setModalResult(mrOk);
        InsertButtonClick(Sender);
    }
    Key = 0;   // consumed (prevents the default button handling of QDialog)
}

void TDataReaderForm::SearchButtonClick(QObject* Sender)
{
    int I;
    QWidget* O;

    {
        QSignalBlocker Block(ui.ListBox);   // Items.Clear does not fire OnClick in VCL
        ui.ListBox->clear();
    }
    GenreSt->Clear();
    DeveloperSt->Clear();
    PublisherSt->Clear();
    YearSt->Clear();
    CoverSt->Clear();
    NotesSt->Clear();

    // Note: Delphi disables the form (Enabled:=False) while the modal wait dialog
    // is shown.  In Qt a disabled parent would also disable the child dialog; the
    // modal dialog blocks the input anyway.
    if (ShowCompleted) O = this; else O = parentWidget();
    if (ShowDataReaderInternetListWaitDialog(O, DataReader, ui.GameNameEdit->text(), LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError())) {
        QSignalBlocker Block(ui.ListBox);
        for (I = 0; I < DataReader->GameNames()->Count(); I++) {
            ui.ListBox->addItem((*DataReader->GameNames())[I]);
            GenreSt->Add("");
            DeveloperSt->Add("");
            PublisherSt->Add("");
            YearSt->Add("");
            CoverSt->Add("");
            NotesSt->Add("");
        }
    } else {
        ListBoxClick(Sender);
    }

    /* finally */
    {
        QSignalBlocker Block(ui.ListBox);
        if (ui.ListBox->count() > 0) ui.ListBox->setCurrentRow(0);
    }
    ListBoxClick(Sender);
    if (ui.ListBox->count() > 0) ui.ListBox->setFocus();
}

void TDataReaderForm::SearchTypeCheckBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    PrgSetup->SetDataReaderAllPlatforms(!ui.SearchTypeCheckBox->isChecked());
}

void TDataReaderForm::SourceComboBoxChange(QObject* Sender)
{
    bool B;
    if (DataReader != nullptr) { delete DataReader; DataReader = nullptr; }

    switch (ui.SourceComboBox->currentIndex()) {
        case 0: DataReader = new TTheGamesDBDataReader; break;
        case 1: {
            DataReader = new TMobyDataReader;
            B = ShowDataReaderInternetConfigWaitDialog(parentWidget(), DataReader, false, LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError());
            if (!B) {
                ui.SourceComboBox->setCurrentIndex(0);
                SourceComboBoxChange(nullptr);
                return;
            }
            break;
        }
    }

    if ((Sender != nullptr) && (LastSource != ui.SourceComboBox->currentIndex())) SearchButtonClick(Sender);
    LastSource = ui.SourceComboBox->currentIndex();
}

static QString ProcessGenre(const QString& S)
{
    QString result = S;
    for (int I = 1; I <= Length(result); I++) if (result[I - 1] == ',') result[I - 1] = ';';
    return result;
}

static QString BuildHint(const QString& AText)
{
    const int MaxHint = 1000;
    const int MaxLine = 70;
    int I, Count;
    QString result;
    QString Text = Trim(AText);

    Count = 0;
    for (I = 1; I <= Length(Text); I++) {
        if (Text[I - 1] == QChar(10)) continue;
        if ((Text[I - 1] == QChar(13)) || ((Count > MaxLine) && (Text[I - 1] == ' '))) {
            if (Length(result) > MaxHint) { result = result + "..."; return result; }
            result = result + "\n"; Count = 0;   // Delphi: #13 (hint line break)
            continue;
        }
        result = result + Text[I - 1];
        Count++;
    }
    return result;
}

void TDataReaderForm::ListBoxClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    TDataReaderGameDataThread* DataReaderGameDataThread;
    int Nr;
    QWidget* O;

    Nr = ui.ListBox->currentRow();
    if (Nr < 0) {
        ui.GameDataBox->setTitle(LanguageSetup->DataReaderNoSearchResults());
        ui.InsertButton->setEnabled(false);
        ui.NameCheckBox->setEnabled(false);
        ui.GenreCheckBox->setEnabled(false);
        ui.GenreLabel->setText("");
        ui.DeveloperCheckBox->setEnabled(false);
        ui.DeveloperLabel->setText("");
        ui.PublisherCheckBox->setEnabled(false);
        ui.PublisherLabel->setText("");
        ui.YearCheckBox->setEnabled(false);
        ui.YearLabel->setText("");
        ui.DownloadCoverCheckBox->setEnabled(false);
        ui.DownloadCoverAllCheckBox->setEnabled(false);
        ui.DeveloperCheckBox->setEnabled(false);
        return;
    }

    if ((Trim((*GenreSt)[Nr]).isEmpty()) && (Trim((*DeveloperSt)[Nr]).isEmpty()) && (Trim((*PublisherSt)[Nr]).isEmpty()) && (Trim((*YearSt)[Nr]).isEmpty()) && (Trim((*CoverSt)[Nr]).isEmpty()) && (Trim((*NotesSt)[Nr]).isEmpty())) {
        // Enabled:=False / True around the modal dialog: see SearchButtonClick
        if (ShowCompleted) O = this; else O = parentWidget();
        DataReaderGameDataThread = ShowDataReaderInternetDataWaitDialog(O, DataReader, Nr, false, LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError());
        if (DataReaderGameDataThread != nullptr) {
            (*GenreSt)[Nr] = GetCustomGenreName(ProcessGenre(DataReaderGameDataThread->Genre()));
            (*DeveloperSt)[Nr] = DataReaderGameDataThread->Developer();
            (*PublisherSt)[Nr] = DataReaderGameDataThread->Publisher();
            (*YearSt)[Nr] = DataReaderGameDataThread->Year();
            (*CoverSt)[Nr] = DataReaderGameDataThread->ImageURL();
            (*NotesSt)[Nr] = DecodeHTMLSymbols(DataReaderGameDataThread->Notes());
            delete DataReaderGameDataThread;
        }
    }

    ui.GameDataBox->setTitle(ui.ListBox->item(Nr)->text());
    ui.NameLabel->setText(ui.ListBox->item(Nr)->text());
    ui.NameCheckBox->setEnabled(true);
    ui.GenreCheckBox->setEnabled(!Trim((*GenreSt)[Nr]).isEmpty());
    ui.GenreLabel->setText((*GenreSt)[Nr]);
    ui.DeveloperCheckBox->setEnabled(!Trim((*DeveloperSt)[Nr]).isEmpty());
    ui.DeveloperLabel->setText((*DeveloperSt)[Nr]);
    ui.PublisherCheckBox->setEnabled(!Trim((*PublisherSt)[Nr]).isEmpty());
    ui.PublisherLabel->setText((*PublisherSt)[Nr]);
    ui.YearCheckBox->setEnabled(!Trim((*YearSt)[Nr]).isEmpty());
    ui.YearLabel->setText((*YearSt)[Nr]);
    ui.DownloadCoverCheckBox->setEnabled(!Trim((*CoverSt)[Nr]).isEmpty());
    ui.DownloadCoverAllCheckBox->setEnabled(ui.DownloadCoverCheckBox->isEnabled() && (Pos("$", (*CoverSt)[Nr]) > 0));
    ui.DescriptionCheckBox->setEnabled(!Trim((*NotesSt)[Nr]).isEmpty());
    ui.DescriptionCheckBox->setToolTip(BuildHint((*NotesSt)[Nr]));

    ui.InsertButton->setEnabled((!Trim((*GenreSt)[Nr]).isEmpty()) || (!Trim((*DeveloperSt)[Nr]).isEmpty()) || (!Trim((*PublisherSt)[Nr]).isEmpty()) || (!Trim((*YearSt)[Nr]).isEmpty()) || (!Trim((*CoverSt)[Nr]).isEmpty()) || (!Trim((*NotesSt)[Nr]).isEmpty()));
}

void TDataReaderForm::InsertButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S, T;
    int I;
    TDataReaderGameDataThread* DataReaderGameDataThread;

    Name = "";
    Genre = "";
    Developer = "";
    Publisher = "";
    Year = "";
    Notes = "";

    const int Nr = ui.ListBox->currentRow();
    if (Nr < 0) return;   // Delphi would raise an index error here

    if (ui.NameCheckBox->isEnabled() && ui.NameCheckBox->isChecked()) Name = ui.ListBox->item(Nr)->text();
    if (ui.GenreCheckBox->isEnabled() && ui.GenreCheckBox->isChecked()) Genre = ui.GenreLabel->text();
    if (ui.DeveloperCheckBox->isEnabled() && ui.DeveloperCheckBox->isChecked()) Developer = ui.DeveloperLabel->text();
    if (ui.PublisherCheckBox->isEnabled() && ui.PublisherCheckBox->isChecked()) Publisher = ui.PublisherLabel->text();
    if (ui.YearCheckBox->isEnabled() && ui.YearCheckBox->isChecked()) Year = ui.YearLabel->text();

    if (ui.DownloadCoverCheckBox->isEnabled() && ui.DownloadCoverCheckBox->isChecked() && (!CaptureDir.isEmpty())) {
        ForceDirectories(CaptureDir);
        if (ui.DownloadCoverAllCheckBox->isChecked()) {
            /* Get URLs of all images */
            DataReaderGameDataThread = ShowDataReaderInternetDataWaitDialog(this, DataReader, Nr, true, LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError());
            if (DataReaderGameDataThread != nullptr) {
                S = DataReaderGameDataThread->ImageURL();
                delete DataReaderGameDataThread;
            }
            /* Download all */
            TStringList St;
            I = Pos("$", S); while (I > 0) {
                T = Trim(Copy(S, 1, I - 1));
                S = Trim(Copy(S, I + 1, MaxInt));
                St.Add(T);
                if (!ui.DownloadCoverAllCheckBox->isChecked()) break;
                I = Pos("$", S);
            }
            St.Add(S);
            if (St.Count() > 0) ShowDataReaderInternetCoverWaitDialog(this, DataReader, &St, CaptureDir, LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError());
        } else {
            S = (*CoverSt)[Nr];
            /* Download first */
            I = Pos("$", S); if (I > 0) S = Trim(Copy(S, 1, I - 1));
            ShowDataReaderInternetCoverWaitDialog(this, DataReader, S, CaptureDir, LanguageSetup->DataReaderDownloadCaption(), LanguageSetup->DataReaderDownloadInfo(), LanguageSetup->DataReaderDownloadError());
        }
    }

    if (ui.DescriptionCheckBox->isEnabled() && ui.DescriptionCheckBox->isChecked()) Notes = (*NotesSt)[Nr];
}

/* global */

bool ShowDataReaderDialog(QWidget* AOwner, const QString& AGameName, QString& Name, QString& Genre, QString& Developer, QString& Publisher, QString& Year, QString& Internet, QString& Notes, const QString& CaptureDir)
{
    bool result;
    DataReaderForm = new TDataReaderForm(AOwner);
    if (!DataReaderForm->ConfigOK) {
        result = false;
    } else {
        DataReaderForm->ui.GameNameEdit->setText(AGameName);
        DataReaderForm->CaptureDir = CaptureDir;
        result = (DataReaderForm->ShowModal() == mrOk);
        if (result) {
            Name = DataReaderForm->Name;
            Genre = DataReaderForm->Genre;
            Developer = DataReaderForm->Developer;
            Publisher = DataReaderForm->Publisher;
            Year = DataReaderForm->Year;
            Internet = DataReaderForm->Internet;
            Notes = DataReaderForm->Notes;
        }
    }
    delete DataReaderForm;
    DataReaderForm = nullptr;
    return result;
}

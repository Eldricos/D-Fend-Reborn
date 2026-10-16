// C++ port of DataReaderFormUnit.pas (implementation section).
#include "DataReaderFormUnit.h"
#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "PrgConsts.h"
#include "IconLoaderUnit.h"
#include "GameDBToolsUnit.h"
#include "GameInfoUnit.h"

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
    // D-Fend Reborn: the game information comes from the auto setup templates
    // (offline) or from Wikidata (online, optional) - see GameInfoUnit
    ConfigOK = true;
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
    {
        const QSignalBlocker Block(ui.SourceComboBox);
        ui.SourceComboBox->clear();
        ui.SourceComboBox->addItem(LanguageSetup->DataReaderSourceTemplates());
        ui.SourceComboBox->addItem(LanguageSetup->DataReaderSourceWikidata());
    }
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
    ui.DescriptionCheckBox->setText(LanguageSetup->CaptureNotes());
    // (no covers: the templates have none, Wikidata only rarely)
    ui.DownloadCoverCheckBox->setVisible(false);
    ui.DownloadCoverAllCheckBox->setVisible(false);

    ui.InsertButton->setText(LanguageSetup->DataReaderInsert());
    ui.CancelButton->setText(LanguageSetup->Cancel());

    UserIconLoader->DialogImage(DI_FindFile, ui.SearchButton);
    UserIconLoader->DialogImage(DI_OK, ui.InsertButton);
    UserIconLoader->DialogImage(DI_Cancel, ui.CancelButton);

    ui.SourceComboBox->setCurrentIndex(Min(ui.SourceComboBox->count() - 1, Max(0, PrgSetup->GameInfoSource())));
    LastSource = ui.SourceComboBox->currentIndex();
    ui.SearchTypeCheckBox->setChecked(!PrgSetup->DataReaderAllPlatforms());
    ui.SearchTypeCheckBox->setVisible(ui.SourceComboBox->currentIndex() == 1);

    S = Trim(PrgSetup->DataReaderActiveSettings());
    while (Length(S) < 8) S = S + "X";
    auto Ch = [&S](int I) { return (I <= Length(S)) ? S[I - 1] : QChar(0); };
    ui.NameCheckBox->setChecked(Ch(1) != '-');
    ui.GenreCheckBox->setChecked(Ch(2) != '-');
    ui.DeveloperCheckBox->setChecked(Ch(3) != '-');
    ui.PublisherCheckBox->setChecked(Ch(4) != '-');
    ui.YearCheckBox->setChecked(Ch(5) != '-');
    ui.DescriptionCheckBox->setChecked(Ch(8) != '-');

    if (!Trim(ui.GameNameEdit->text()).isEmpty()) SearchButtonClick(Sender); else ListBoxClick(Sender);
    ShowCompleted = true;
}

void TDataReaderForm::FormDestroy(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    PrgSetup->SetGameInfoSource(ui.SourceComboBox->currentIndex());

    S = "";
    if (ui.NameCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.GenreCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.DeveloperCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.PublisherCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    if (ui.YearCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    S = S + "--";   // (cover settings of the old data reader)
    if (ui.DescriptionCheckBox->isChecked()) S = S + "X"; else S = S + "-";
    PrgSetup->SetDataReaderActiveSettings(S);
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
    Q_UNUSED(Sender);
    QString Error;
    bool OK = true;

    Results.clear();
    if (ui.SourceComboBox->currentIndex() == 1) {
        OK = SearchWikidataGameInfo(ui.GameNameEdit->text(), ui.SearchTypeCheckBox->isChecked(), Results, Error);
    } else {
        Results = SearchTemplateGameInfo(ui.GameNameEdit->text());
    }

    {
        QSignalBlocker Block(ui.ListBox);   // Items.Clear does not fire OnClick in VCL
        ui.ListBox->clear();
        for (const TGameInfo& Info : Results) ui.ListBox->addItem(Info.Year.isEmpty() ? Info.Name : (Info.Name + " (" + Info.Year + ")"));
        if (ui.ListBox->count() > 0) ui.ListBox->setCurrentRow(0);
    }
    if (!OK) MessageDlg(Format(LanguageSetup->DataReaderDownloadError(), {"Wikidata"}) + "\n\n" + Error, mtError, mbOK, 0);
    ListBoxClick(Sender);
    if (ui.ListBox->count() > 0) ui.ListBox->setFocus();
}

void TDataReaderForm::SearchTypeCheckBoxClick(QObject* Sender)
{
    PrgSetup->SetDataReaderAllPlatforms(!ui.SearchTypeCheckBox->isChecked());
    if ((Sender != nullptr) && ShowCompleted && (ui.SourceComboBox->currentIndex() == 1)) SearchButtonClick(Sender);
}

void TDataReaderForm::SourceComboBoxChange(QObject* Sender)
{
    ui.SearchTypeCheckBox->setVisible(ui.SourceComboBox->currentIndex() == 1);
    if ((Sender != nullptr) && (LastSource != ui.SourceComboBox->currentIndex())) SearchButtonClick(Sender);
    LastSource = ui.SourceComboBox->currentIndex();
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
    const int Nr = ui.ListBox->currentRow();
    if ((Nr < 0) || (Nr >= Results.count())) {
        ui.GameDataBox->setTitle(LanguageSetup->DataReaderNoSearchResults());
        ui.InsertButton->setEnabled(false);
        ui.NameCheckBox->setEnabled(false);
        ui.NameLabel->setText("");
        ui.GenreCheckBox->setEnabled(false);
        ui.GenreLabel->setText("");
        ui.DeveloperCheckBox->setEnabled(false);
        ui.DeveloperLabel->setText("");
        ui.PublisherCheckBox->setEnabled(false);
        ui.PublisherLabel->setText("");
        ui.YearCheckBox->setEnabled(false);
        ui.YearLabel->setText("");
        ui.DescriptionCheckBox->setEnabled(false);
        return;
    }

    TGameInfo& Info = Results[Nr];
    if (!Info.DetailsLoaded) {
        QString Error;
        if (!LoadWikidataGameInfo(Info, Error)) MessageDlg(Format(LanguageSetup->DataReaderDownloadError(), {"Wikidata"}) + "\n\n" + Error, mtError, mbOK, 0);
    }

    const QString Genre = GetCustomGenreName(Info.Genre);   // (in the program language)
    ui.GameDataBox->setTitle(Info.Name);
    ui.NameLabel->setText(Info.Name);
    ui.NameCheckBox->setEnabled(!Info.Name.isEmpty());
    ui.GenreCheckBox->setEnabled(!Genre.isEmpty());
    ui.GenreLabel->setText(Genre);
    ui.DeveloperCheckBox->setEnabled(!Info.Developer.isEmpty());
    ui.DeveloperLabel->setText(Info.Developer);
    ui.PublisherCheckBox->setEnabled(!Info.Publisher.isEmpty());
    ui.PublisherLabel->setText(Info.Publisher);
    ui.YearCheckBox->setEnabled(!Info.Year.isEmpty());
    ui.YearLabel->setText(Info.Year);
    ui.DescriptionCheckBox->setEnabled(!Info.Notes.isEmpty());
    ui.DescriptionCheckBox->setToolTip(BuildHint(Info.Notes));
    ui.GameDataBox->setToolTip(Info.Internet);

    ui.InsertButton->setEnabled(true);
}

void TDataReaderForm::InsertButtonClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    Name = ""; Genre = ""; Developer = ""; Publisher = ""; Year = ""; Internet = ""; Notes = "";

    const int Nr = ui.ListBox->currentRow();
    if ((Nr < 0) || (Nr >= Results.count())) return;
    const TGameInfo& Info = Results[Nr];

    if (ui.NameCheckBox->isEnabled() && ui.NameCheckBox->isChecked()) Name = Info.Name;
    if (ui.GenreCheckBox->isEnabled() && ui.GenreCheckBox->isChecked()) Genre = ui.GenreLabel->text();
    if (ui.DeveloperCheckBox->isEnabled() && ui.DeveloperCheckBox->isChecked()) Developer = Info.Developer;
    if (ui.PublisherCheckBox->isEnabled() && ui.PublisherCheckBox->isChecked()) Publisher = Info.Publisher;
    if (ui.YearCheckBox->isEnabled() && ui.YearCheckBox->isChecked()) Year = Info.Year;
    if (ui.DescriptionCheckBox->isEnabled() && ui.DescriptionCheckBox->isChecked()) Notes = Info.Notes;
    Internet = Info.Internet;   // (only used if the profile has no web site yet)
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

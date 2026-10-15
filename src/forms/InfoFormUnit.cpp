// C++ port of InfoFormUnit.pas (implementation section).
#include "InfoFormUnit.h"

#include "LanguageSetupUnit.h"
#include "CommonTools.h"
#include "PrgConsts.h"
#include "PrgSetupUnit.h"
#include "IconLoaderUnit.h"
#include "Process.h"

#include <QRandomGenerator>

TInfoForm* InfoForm = nullptr;

// Picture of a TImage from the .ui (set with setPixmap) or assigned at runtime (setPicture)
static QPixmap PictureOf(TImageWidget* I)
{
    if (!I->picture().isNull()) return I->picture();
    return I->pixmap();
}

// TRichEdit.Lines.LoadFromFile
static void MemoLoadFromFile(QTextEdit* Memo, const QString& FileName)
{
    bool OK = false;
    QString S = ReadTextFile(FileName, &OK);
    if (OK) Memo->setPlainText(S);
}

static double Random()
{
    return QRandomGenerator::global()->generateDouble();
}

/* TStringGrid "LanguageAuthorsTab" (ColCount=3, FixedCols=1, FixedRows=1) */

QString TInfoForm::Cells(int ACol, int ARow) const
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.LanguageAuthorsTab->horizontalHeaderItem(ACol);
        return (H != nullptr) ? H->text() : QString();
    }
    QTableWidgetItem* It = ui.LanguageAuthorsTab->item(ARow - 1, ACol);
    return (It != nullptr) ? It->text() : QString();
}

void TInfoForm::SetCells(int ACol, int ARow, const QString& Value)
{
    if (ARow == 0) {
        QTableWidgetItem* H = ui.LanguageAuthorsTab->horizontalHeaderItem(ACol);
        if (H == nullptr) { H = new QTableWidgetItem; ui.LanguageAuthorsTab->setHorizontalHeaderItem(ACol, H); }
        H->setText(Value);
        return;
    }
    QTableWidgetItem* It = ui.LanguageAuthorsTab->item(ARow - 1, ACol);
    if (It == nullptr) {
        It = new QTableWidgetItem;
        It->setFlags(It->flags() & ~Qt::ItemIsEditable);
        if (ACol == 0) It->setBackground(palette().color(QPalette::Button));   // fixed column
        ui.LanguageAuthorsTab->setItem(ARow - 1, ACol, It);
    }
    It->setText(Value);
}

TInfoForm::TInfoForm(QWidget *AOwner) : TVclForm(AOwner)
{
    ui.setupUi(this);
    VclLayout::setupForm(this);

    ui.LanguageAuthorsTab->setColumnCount(3);
    ui.LanguageAuthorsTab->setRowCount(2);
    ui.LanguageAuthorsTab->verticalHeader()->setVisible(false);
    ui.LanguageAuthorsTab->setMouseTracking(true);
    ui.LanguageAuthorsTab->viewport()->setMouseTracking(true);
    ui.LanguageAuthorsTab->viewport()->installEventFilter(this);
    ui.Label1->installEventFilter(this);
    ui.HomepageLabel->setText(DFRebornHomepage);   // (the original showed its sourceforge homepage)
    ui.HomepageLabel->installEventFilter(this);
    ui.HomepageLabel->setCursor(Qt::PointingHandCursor);

    Timer = new QTimer(this);
    Timer->setInterval(100);
    connect(Timer, &QTimer::timeout, this, [this]() { TimerTimer(Timer); });

    connect(ui.Image3, &TImageWidget::clicked, this, [this]() { Image3Click(ui.Image3); });
    connect(ui.Image4, &TImageWidget::clicked, this, [this]() { HomepageLabelClick(ui.Image4); });
    connect(ui.LicenseComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { LicenseComboBoxChange(ui.LicenseComboBox); });
    connect(ui.ChangeLogComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { ChangeLogComboBoxChange(ui.ChangeLogComboBox); });
    VclLayout::installKeyPreview(this, [this](int Key, Qt::KeyboardModifiers Mods) {
        Q_UNUSED(Mods);
        if (Key != Qt::Key_Escape) return false;
        QChar C(27);
        FormKeyPress(this, C);
        return C.unicode() == 0;
    });
}

TInfoForm::~TInfoForm() = default;

void TInfoForm::showEvent(QShowEvent* E)
{
    TVclForm::showEvent(E);
    FormShow(this);
}

bool TInfoForm::eventFilter(QObject* Obj, QEvent* E)
{
    if (Obj == ui.Label1 && E->type() == QEvent::MouseButtonDblClick) {
        Label1DblClick(ui.Label1);
    } else if (Obj == ui.HomepageLabel && E->type() == QEvent::MouseButtonRelease) {
        HomepageLabelClick(ui.HomepageLabel);
    } else if (Obj == ui.LanguageAuthorsTab->viewport() && E->type() == QEvent::MouseMove) {
        QMouseEvent* ME = static_cast<QMouseEvent*>(E);
        LanguageAuthorsTabMouseMove(ui.LanguageAuthorsTab, ME->modifiers(), ME->position().toPoint().x(), ME->position().toPoint().y());
    }
    return TVclForm::eventFilter(Obj, E);
}

void TInfoForm::FormShow(QObject* Sender)
{
    int I, J;
    TSearchRec Rec;
    TLanguageSetup* Lang;
    QString S, T;

    FImage = nullptr;

    // SetVistaFonts(self); Font.Charset:=... - not needed with Qt

    setWindowTitle(LanguageSetup->InfoForm());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.MainSheet), LanguageSetup->InfoFormMain());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.LanguageSheet), LanguageSetup->InfoFormLanguage());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.LicenseSheet), LanguageSetup->InfoFormLicense());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.CompLicenseSheet), LanguageSetup->InfoFormCompLicense());
    ui.PageControl->setTabText(ui.PageControl->indexOf(ui.ChangeLogTabSheet), LanguageSetup->InfoFormChangeLog());

    ui.OKButton->setText(LanguageSetup->OK());
    UserIconLoader->DialogImage(DI_OK, ui.OKButton);

    ui.VersionLabel->setText(GetFileVersionAsString());
    // D-Fend Reborn: "based on D-Fend Reloaded by Alexander Herzog" instead of
    // "written by Alexander Herzog" and his (old) e-mail address
    ui.WrittenByLabel->setText(Format(LanguageSetup->InfoFormBasedOn(), {QStringLiteral("Alexander Herzog")}));
    ui.eMailLabel->setVisible(false);
    SetCells(0, 0, LanguageSetup->InfoFormLanguageFile());
    SetCells(1, 0, LanguageSetup->InfoFormLanguageAuthor());
    SetCells(2, 0, LanguageSetup->InfoFormLanguageVersion());
    ui.LanguageAuthorsTab->setColumnWidth(0, 175);
    ui.LanguageAuthorsTab->setColumnWidth(2, 50);
    ui.LanguageAuthorsTab->setColumnWidth(1, ui.LanguageAuthorsTab->viewport()->width() - ui.LanguageAuthorsTab->columnWidth(0) - ui.LanguageAuthorsTab->columnWidth(2) - 25);

    {
        const QSignalBlocker Block(ui.LicenseComboBox);
        ui.LicenseComboBox->addItem("D-Fend Reborn", 0);
        ui.LicenseComboBox->addItem("DOSBox", 1);
        if ((Trim(PrgSetup->ScummVMPath()) != "") && DirectoryExists(PrgSetup->ScummVMPath())) ui.LicenseComboBox->addItem("ScummVM", 2);
        ui.LicenseComboBox->setCurrentIndex(0);
    }
    LicenseComboBoxChange(Sender);

    if (FileExists(PrgDir() + "LicenseComponents.txt")) {
        MemoLoadFromFile(ui.CompLicenseMemo, PrgDir() + "LicenseComponents.txt");
    } else {
        MemoLoadFromFile(ui.CompLicenseMemo, PrgDir() + BinFolder + PathDelim + "LicenseComponents.txt");
    }
    ui.CompLicenseMemo->setPlainText("\n" + ui.CompLicenseMemo->toPlainText());   // Lines.Insert(0,'')

    {
        const QSignalBlocker Block(ui.ChangeLogComboBox);
        ui.ChangeLogComboBox->addItem("D-Fend Reborn", 0);   // (includes the D-Fend Reloaded change log)
        ui.ChangeLogComboBox->addItem("DOSBox", 1);
        if ((Trim(PrgSetup->ScummVMPath()) != "") && DirectoryExists(PrgSetup->ScummVMPath())) ui.ChangeLogComboBox->addItem("ScummVM", 2);
        ui.ChangeLogComboBox->setCurrentIndex(0);
    }
    ChangeLogComboBoxChange(Sender);

    {
        TStringList St, StShort;
        if (DirectoryExists(PrgDir() + LanguageSubDir)) {
            I = FindFirst(PrgDir() + LanguageSubDir + PathDelim + "*.ini", faAnyFile, Rec);
            while (I == 0) {
                if ((ExtLowerCase(Copy(Rec.Name, Length(Rec.Name) - 10)) != ".custom.ini") && (StShort.IndexOf(ExtUpperCase(Rec.Name)) < 0)) {
                    St.Add(PrgDir() + LanguageSubDir + PathDelim + Rec.Name);
                    StShort.Add(ExtUpperCase(Rec.Name));
                }
                I = FindNext(Rec);
            }
            FindClose(Rec);
        }

        if (DirectoryExists(PrgDataDir() + LanguageSubDir) && (PrgDataDir() != PrgDir())) {
            I = FindFirst(PrgDataDir() + LanguageSubDir + PathDelim + "*.ini", faAnyFile, Rec);
            while (I == 0) {
                if ((ExtLowerCase(Copy(Rec.Name, Length(Rec.Name) - 10)) != ".custom.ini") && (StShort.IndexOf(ExtUpperCase(Rec.Name)) < 0)) {
                    St.Add(PrgDataDir() + LanguageSubDir + PathDelim + Rec.Name);
                    StShort.Add(ExtUpperCase(Rec.Name));
                }
                I = FindNext(Rec);
            }
            FindClose(Rec);
        }

        ui.LanguageAuthorsTab->setRowCount(St.Count());   // RowCount:=St.Count+1 (incl. fixed row)
        for (I = 0; I < St.Count(); I++) {
            Lang = new TLanguageSetup(St[I], true);
            S = ChangeFileExt(ExtractFileName(St[I]), "");
            T = Lang->LocalLanguageName();
            if ((T != S) && (T != "")) S = T + " (" + S + ")";
            SetCells(0, I + 1, S);
            SetCells(1, I + 1, Lang->Author());
            S = Lang->MaxVersion();
            J = VersionToInt(S);
            if ((J >= 10000) || (Copy(S, 1, 2) == "0.")) S = IntToStr(J / 10000) + "." + IntToStr((J % 10000) / 100) + ".x";
            SetCells(2, I + 1, S);
            delete Lang;
        }
    }

    TStringList* St = StringToStringList(LanguageSetup->InfoFormDFend());
    ui.DFendInfoLabel->setText(St->Text());
    delete St;
}

void TInfoForm::FormKeyPress(QObject* Sender, QChar& Key)
{
    Q_UNUSED(Sender);
    if (Key == QChar(27)) { Close(); Key = QChar(0); }
}

void TInfoForm::HomepageLabelClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    ShellOpen(ui.HomepageLabel->text());
}

void TInfoForm::Image3Click(QObject* Sender)
{
    Q_UNUSED(Sender);
    ShellOpen(QString("http:/") + "/gplv3.fsf.org/");
}

void TInfoForm::Label1DblClick(QObject* Sender)
{
    Q_UNUSED(Sender);
    if (!(QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier)) return;
    if (FImage != nullptr) return;
    FImage = new TImageWidget(ui.MainSheet); Dir = 3;
    FImage->setAttribute(Qt::WA_TranslucentBackground);   // Transparent:=True
    FImage->setPicture(PictureOf(ui.EasterImage1));
    FImage->resize(PictureOf(ui.EasterImage1).size());    // AutoSize:=True
    FImage->move(-100, 0);
    FImage->show();
    Timer->start();
}

void TInfoForm::LanguageAuthorsTabMouseMove(QObject* Sender, TShiftState Shift, int X, int Y)
{
    Q_UNUSED(Sender); Q_UNUSED(Shift); Q_UNUSED(X);
    int Nr;

    // Find the row under the mouse (Y is relative to the grid viewport, the fixed row is the header)
    Nr = ui.LanguageAuthorsTab->rowAt(Y);
    if (Nr >= 0) Nr = Nr + 1;

    QString H;
    if (Nr <= 0) H = ""; else {
        H = LanguageSetup->InfoFormLanguageFile() + ": " + Cells(0, Nr) + "\n" +
            LanguageSetup->InfoFormLanguageAuthor() + ": " + Cells(1, Nr) + "\n" +
            LanguageSetup->InfoFormLanguageVersion() + ": " + Cells(2, Nr);
    }
    if (H != ui.LanguageAuthorsTab->toolTip()) {
        QToolTip::hideText();   // ShowHint:=False; ShowHint:=True
        ui.LanguageAuthorsTab->setToolTip(H);
        if (H != "") QToolTip::showText(QCursor::pos(), H, ui.LanguageAuthorsTab);
    }
}

void TInfoForm::LicenseComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    ui.LicenseMemo->clear();
    switch (ui.LicenseComboBox->itemData(ui.LicenseComboBox->currentIndex()).toInt()) {
        case 0: if (FileExists(PrgDir() + BinFolder + PathDelim + "License.txt")) S = PrgDir() + BinFolder + PathDelim + "License.txt"; else S = PrgDir() + "License.txt"; break;
        case 1:
            S = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + "Documentation" + PathDelim + "COPYING.txt";
            if (!FileExists(S)) S = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + "COPYING.txt";
            break;
        case 2: S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + "COPYING.txt"; break;
    }
    if (S != "") {
        if (FileExists(S)) MemoLoadFromFile(ui.LicenseMemo, S);
    }
}

void TInfoForm::ChangeLogComboBoxChange(QObject* Sender)
{
    Q_UNUSED(Sender);
    QString S;
    ui.ChangeLogMemo->clear();
    switch (ui.ChangeLogComboBox->itemData(ui.ChangeLogComboBox->currentIndex()).toInt()) {
        case 0:
            // (the file is named "Changelog.txt"; case matters on Linux)
            for (const QString& Name : {QStringLiteral("Changelog.txt"), QStringLiteral("ChangeLog.txt")}) {
                if (FileExists(PrgDir() + BinFolder + PathDelim + Name)) { S = PrgDir() + BinFolder + PathDelim + Name; break; }
                if (FileExists(PrgDir() + Name)) { S = PrgDir() + Name; break; }
            }
            break;
        case 1:
            if (FileExists(IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + "Documentation" + PathDelim + "NEWS.txt"))
                S = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + "Documentation" + PathDelim + "NEWS.txt";
            else
                S = IncludeTrailingPathDelimiter(PrgSetup->DOSBoxSettings(0)->DosBoxDir()) + "NEWS.txt";
            break;
        case 2: S = IncludeTrailingPathDelimiter(PrgSetup->ScummVMPath()) + "NEWS.txt"; break;
    }
    if (S != "") MemoLoadFromFile(ui.ChangeLogMemo, S);
    QFont F("Courier New");
    F.setStyleHint(QFont::Monospace);
    ui.ChangeLogMemo->setFont(F);
}

void TInfoForm::TimerTimer(QObject* Sender)
{
    Q_UNUSED(Sender);
    const int MoveStep = 3;
    if (FImage == nullptr) return;
    if (Random() > 0.99) switch (Dir) { case 1: Dir = 30; break; case 3: Dir = 10; break; case 7: Dir = 90; break; case 9: Dir = 70; break; }
    if (Random() > 0.95) switch (Dir) { case 1: Dir = 7; break; case 3: Dir = 9; break; case 7: Dir = 1; break; case 9: Dir = 3; break; }
    if (FImage->x() <= 0) switch (Dir) { case 1: Dir = 30; break; case 7: Dir = 90; break; }
    if (FImage->x() + FImage->width() >= ui.MainSheet->width()) switch (Dir) { case 3: Dir = 10; break; case 9: Dir = 70; break; }
    if (FImage->y() <= 0) switch (Dir) { case 7: Dir = 1; break; case 9: Dir = 3; break; }
    if (FImage->y() + FImage->height() >= ui.MainSheet->height()) switch (Dir) { case 1: Dir = 7; break; case 3: Dir = 9; break; }
    if (Dir % 10 == 0) FImage->setPicture(PictureOf(ui.EasterImage3));
    if ((Dir >= 10) && (Dir % 10 < 9)) Dir++;
    switch (Dir) {
        case 1: FImage->move(FImage->x() - MoveStep, FImage->y() + 1); break;
        case 3: FImage->move(FImage->x() + MoveStep, FImage->y() + 1); break;
        case 7: FImage->move(FImage->x() - MoveStep, FImage->y() - 1); break;
        case 9: FImage->move(FImage->x() + MoveStep, FImage->y() - 1); break;
        case 39: case 99: FImage->setPicture(PictureOf(ui.EasterImage1)); Dir = Dir / 10; break;
        case 19: case 79: FImage->setPicture(PictureOf(ui.EasterImage2)); Dir = Dir / 10; break;
    }
}

/* global */

void ShowInfoDialog(QWidget* AOwner)
{
    InfoForm = new TInfoForm(AOwner);
    InfoForm->ShowModal();
    delete InfoForm;
    InfoForm = nullptr;
}

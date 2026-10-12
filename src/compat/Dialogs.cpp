#include "Dialogs.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>

TMsgDlgCaptions MsgDlgCaptions;

static QWidget *defaultParent(QWidget *Parent)
{
    if (Parent) return Parent;
    QWidget *W = QApplication::activeModalWidget();
    if (!W) W = QApplication::activeWindow();
    return W;
}

int MessageDlgCaption(const QString &Caption, const QString &Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons, QWidget *Parent)
{
    QMessageBox Box(defaultParent(Parent));
    Box.setWindowTitle(Caption);
    Box.setText(Msg);
    switch (DlgType) {
    case mtWarning: Box.setIcon(QMessageBox::Warning); break;
    case mtError: Box.setIcon(QMessageBox::Critical); break;
    case mtInformation: Box.setIcon(QMessageBox::Information); break;
    case mtConfirmation: Box.setIcon(QMessageBox::Question); break;
    default: Box.setIcon(QMessageBox::NoIcon); break;
    }
    struct B { int Flag; QString *Text; int Result; QMessageBox::ButtonRole Role; };
    const B Defs[] = {
        {mbYes, &MsgDlgCaptions.Yes, mrYes, QMessageBox::YesRole},
        {mbNo, &MsgDlgCaptions.No, mrNo, QMessageBox::NoRole},
        {mbOK, &MsgDlgCaptions.OK, mrOk, QMessageBox::AcceptRole},
        {mbCancel, &MsgDlgCaptions.Cancel, mrCancel, QMessageBox::RejectRole},
        {mbAbort, &MsgDlgCaptions.Abort, mrAbort, QMessageBox::RejectRole},
        {mbRetry, &MsgDlgCaptions.Retry, mrRetry, QMessageBox::AcceptRole},
        {mbIgnore, &MsgDlgCaptions.Ignore, mrIgnore, QMessageBox::AcceptRole},
        {mbAll, &MsgDlgCaptions.All, mrAll, QMessageBox::AcceptRole},
        {mbNoToAll, &MsgDlgCaptions.NoToAll, mrNoToAll, QMessageBox::NoRole},
        {mbYesToAll, &MsgDlgCaptions.YesToAll, mrYesToAll, QMessageBox::YesRole},
        {mbHelp, &MsgDlgCaptions.Help, mrNone, QMessageBox::HelpRole},
        {mbClose, &MsgDlgCaptions.Close, mrClose, QMessageBox::RejectRole},
    };
    if (Buttons == 0) Buttons = mbOK;
    QList<QPair<QAbstractButton *, int>> Map;
    QAbstractButton *Escape = nullptr;
    for (const B &D : Defs) {
        if (!(Buttons & D.Flag)) continue;
        QPushButton *P = Box.addButton(*D.Text, D.Role);
        Map.append({P, D.Result});
        if (!Box.defaultButton()) Box.setDefaultButton(P);
        if (D.Result == mrCancel || D.Result == mrNo || D.Result == mrAbort || D.Result == mrClose) Escape = P;
    }
    if (Escape) Box.setEscapeButton(Escape);
    else if (Map.size() == 1) Box.setEscapeButton(Map[0].first);
    Box.exec();
    QAbstractButton *Clicked = Box.clickedButton();
    for (auto &P : Map) if (P.first == Clicked) return P.second;
    return mrCancel;
}

int MessageDlg(const QString &Msg, TMsgDlgType DlgType, TMsgDlgButtons Buttons, int HelpCtx, QWidget *Parent)
{
    Q_UNUSED(HelpCtx);
    QString Caption;
    switch (DlgType) {
    case mtWarning: Caption = MsgDlgCaptions.Warning; break;
    case mtError: Caption = MsgDlgCaptions.Error; break;
    case mtInformation: Caption = MsgDlgCaptions.Information; break;
    case mtConfirmation: Caption = MsgDlgCaptions.Confirmation; break;
    default: Caption = QApplication::applicationDisplayName(); break;
    }
    return MessageDlgCaption(Caption, Msg, DlgType, Buttons, Parent);
}

void ShowMessage(const QString &Msg, QWidget *Parent)
{
    MessageDlgCaption(QApplication::applicationDisplayName(), Msg, mtCustom, mbOK, Parent);
}

bool InputQuery(const QString &Caption, const QString &Prompt, QString &Value, QWidget *Parent)
{
    bool Ok = false;
    QString R = QInputDialog::getText(defaultParent(Parent), Caption, Prompt, QLineEdit::Normal, Value, &Ok);
    if (Ok) Value = R;
    return Ok;
}

QString InputBox(const QString &Caption, const QString &Prompt, const QString &Default, QWidget *Parent)
{
    QString V = Default;
    if (!InputQuery(Caption, Prompt, V, Parent)) return Default;
    return V;
}

bool SelectDirectory(const QString &Caption, const QString &Root, QString &Directory, QWidget *Parent)
{
    QString Start = Directory.isEmpty() ? Root : Directory;
    QString R = QFileDialog::getExistingDirectory(defaultParent(Parent), Caption, ToNativePath(Start));
    if (R.isEmpty()) return false;
    Directory = QDir::toNativeSeparators(R);
    return true;
}

QString DelphiFilterToQt(const QString &Filter)
{
    if (Filter.isEmpty()) return QString();
    QStringList Parts = Filter.split('|');
    QStringList Out;
    for (int i = 0; i + 1 < Parts.size(); i += 2) {
        QString Name = Parts[i];
        QString Masks = Parts[i + 1];
        Masks.replace(';', ' ');
        Masks.replace("*.*", "*");
        // Delphi descriptions often already contain the mask: "Text (*.txt)"
        if (Name.contains('(')) Name = Name.left(Name.indexOf('(')).trimmed();
        Out << QString("%1 (%2)").arg(Name, Masks);
    }
    return Out.join(";;");
}

static QString selectedFilterFor(const QString &QtFilter, int FilterIndex)
{
    QStringList L = QtFilter.split(";;");
    if (FilterIndex >= 1 && FilterIndex <= L.size()) return L[FilterIndex - 1];
    return QString();
}

bool TOpenDialog::Execute()
{
    QString QtFilter = DelphiFilterToQt(Filter);
    QString Sel = selectedFilterFor(QtFilter, FilterIndex);
    QString Start = FileName.isEmpty() ? InitialDir : (ExtractFilePath(FileName).isEmpty() ? IncludeTrailingPathDelimiter(InitialDir) + FileName : FileName);
    Files.Clear();
    if (AllowMultiSelect) {
        QStringList R = QFileDialog::getOpenFileNames(defaultParent(FParent), Title, ToNativePath(Start), QtFilter, &Sel);
        if (R.isEmpty()) return false;
        for (QString &F : R) Files.Add(QDir::toNativeSeparators(F));
        FileName = Files[0];
    } else {
        QString R = QFileDialog::getOpenFileName(defaultParent(FParent), Title, ToNativePath(Start), QtFilter, &Sel);
        if (R.isEmpty()) return false;
        FileName = QDir::toNativeSeparators(R);
        Files.Add(FileName);
    }
    QStringList L = QtFilter.split(";;");
    FilterIndex = L.indexOf(Sel) + 1;
    return true;
}

bool TSaveDialog::Execute()
{
    QString QtFilter = DelphiFilterToQt(Filter);
    QString Sel = selectedFilterFor(QtFilter, FilterIndex);
    QString Start = FileName.isEmpty() ? InitialDir : (ExtractFilePath(FileName).isEmpty() ? IncludeTrailingPathDelimiter(InitialDir) + FileName : FileName);
    QFileDialog::Options Opt;
    if (!OverwritePrompt) Opt |= QFileDialog::DontConfirmOverwrite;
    QString R = QFileDialog::getSaveFileName(defaultParent(FParent), Title, ToNativePath(Start), QtFilter, &Sel, Opt);
    if (R.isEmpty()) return false;
    if (!DefaultExt.isEmpty() && ExtractFileExt(R).isEmpty()) {
        QString Ext = DefaultExt.startsWith('.') ? DefaultExt : "." + DefaultExt;
        R += Ext;
    }
    FileName = QDir::toNativeSeparators(R);
    QStringList L = QtFilter.split(";;");
    FilterIndex = L.indexOf(Sel) + 1;
    Files.Clear();
    Files.Add(FileName);
    return true;
}

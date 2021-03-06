#include "commit_dialog.h"
#include "ui_commit_dialog.h"

#include <QProcess>
#include <QByteArray>
#include <QSettings>
#include <QMenu>
#include <QFileDialog>

// TODO:
// - Remember selection on refresh.
// - Add support for conflict detection and resolution.
// - Edit context menu actions:
//      - Edit file (using external tool).
//      - Visual diff (using external tool).
// - Add support for change lists. Use a tree instead of a list?
//      - Add default and ignore-on-commit list.
//      - Check list, checks all items.
//      - Add context menu action to move to list.
// - Accept dropped files and file paths. Check that they are in the same svn repository and add to list.
// - Remember previous commit messages.
// - Add some support for editing properties?
// - Authentication.

inline static QString filePathFromStatus(QString line) {
    return line.right(line.size() - 4).trimmed();
}
inline static QString changeListFromStatus(QString line) {
    QRegExp regexp("'(.*)'");
    if (regexp.indexIn(line) != -1) {
        return regexp.cap(1);
    }
    return QString::null;
}


CommitDialog::CommitDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);

    QApplication::instance()->installEventFilter(this);
}

CommitDialog::~CommitDialog()
{
    delete ui;
}

void CommitDialog::reject()
{
    // Make sure close event is sent.
    close();
    QDialog::reject();
}

void CommitDialog::closeEvent(QCloseEvent * e)
{
    QSettings settings("Thekla", "svn-commit-tool");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("message", ui->plainTextEdit->toPlainText());
    settings.setValue("show_unversioned", ui->showUnversionedCheckBox->checkState());
    QDialog::closeEvent(e);
}

void CommitDialog::showEvent(QShowEvent * e)
{
    QSettings settings("Thekla", "svn-commit-tool");
    restoreGeometry(settings.value("geometry").toByteArray());
    ui->plainTextEdit->setPlainText(settings.value("message").toString());
    ui->showUnversionedCheckBox->setCheckState(settings.value("show_unversioned").toBool() ? Qt::Checked : Qt::Unchecked);
    QDialog::showEvent(e);
}

static const Qt::Key action_modifier_key = Qt::Key_Alt;
static const Qt::KeyboardModifiers action_modifier = Qt::AltModifier;

bool CommitDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent * e = static_cast<QKeyEvent *>(event);
        if (e->key() == action_modifier_key) {
            ui->commitButton->setText(tr("Create patch..."));
        }
    }
    else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent * e = static_cast<QKeyEvent *>(event);
        if (e->key() == action_modifier_key) {
            ui->commitButton->setText(tr("Commit"));
        }
    }

    return QDialog::eventFilter(obj, event);
}

void CommitDialog::addChangeList(const char * line)
{
    QString name = changeListFromStatus(line);

    if (name != QString::null) {
        changeLists.append(name);

        QListWidgetItem* item = new QListWidgetItem(line);
        item->setFlags(0);
        //item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        //item->setCheckState(Qt::PartiallyChecked);

        ui->listWidget->addItem(item);
    }
}

void CommitDialog::addFile(const char * filepath)
{
    QListWidgetItem* item = new QListWidgetItem(filepath);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    item->setCheckState(Qt::Unchecked);

    ui->listWidget->addItem(item);
}

void CommitDialog::updateStatus()
{
    int count = checkedItems.count();
    if (count == 0) {
        ui->statusLabel->setText("No files selected.");
    }
    else {
        ui->statusLabel->setText(QString("%1 files selected.").arg(count));
    }

    ui->commitButton->setEnabled(messageAvailable && checkedItems.count());
}


void CommitDialog::watch(const QString & path)
{
    paths.append(path);
}


bool CommitDialog::status()
{
    ui->listWidget->clear();
    checkedItems.clear();
    changeLists.clear();

    QProcess svn;
    svn.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    svn.setProgram("svn");
    svn.setArguments(QStringList("st"));

    svn.start("svn", QStringList("st"), QProcess::ReadOnly);
    svn.waitForFinished(-1);
    if (svn.exitStatus() != QProcess::NormalExit || svn.exitCode() != 0) {
        // @@ Detect non-working copies correctly.
        return false;
    }

    QByteArray output = svn.readAllStandardOutput();
    QList<QByteArray> lines = output.split('\n');
    const bool showUnversioned = ui->showUnversionedCheckBox->checkState() == Qt::Checked;

    foreach(const QByteArray & line, lines) {
        if (line.startsWith("M") || line.startsWith("A") || line.startsWith("D") || line.startsWith(" M") || (showUnversioned && line.startsWith("?"))) {
            if (changeLists.count() == 0) {
                addChangeList("--- Changelist 'default':");
            }
            addFile(line);
        }
        else if (line.startsWith("--- Changelist")) {
            addChangeList(line);
        }
    }

    // Add hidden changelist 'ignore-on-commit'
    if (!changeLists.contains("ignore-on-commit")) {
        changeLists.append("ignore-on-commit");
    }

    updateStatus();

    return true;
}

void CommitDialog::commit()
{
    QStringList arguments;

    // Add commit message.
    arguments.append("commit");
    arguments.append("-N");     // Non recursive.
    arguments.append("-m");
    arguments.append(ui->plainTextEdit->toPlainText());

    int count = ui->listWidget->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem * item = ui->listWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            QString filePath = filePathFromStatus(item->text());
            arguments.append(filePath);
        }
    }

    QProcess svn;
    svn.setProgram("svn");
    //svn.setProgram("echo");
    svn.setProcessChannelMode(QProcess::ForwardedChannels);
    svn.setArguments(arguments);

    svn.start();
    hide();

    svn.waitForFinished(-1);
    if (svn.exitStatus() == QProcess::NormalExit && svn.exitCode() == 0) {
        ui->plainTextEdit->clear();
    }
    else {
        // @@ Print error.
    }

    close();
}

void CommitDialog::createPatch(const QString & fileName)
{
    QStringList arguments;

    // Add commit message.
    arguments.append("diff");
    arguments.append("-N");     // Non recursive.
    arguments.append("-x");
    arguments.append("--ignore-eol-style");

    int count = ui->listWidget->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem * item = ui->listWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            QString filePath = filePathFromStatus(item->text());
            arguments.append(filePath);
        }
    }

    QProcess svn;
    svn.setProgram("svn");
    //svn.setProgram("echo");
    svn.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    svn.setStandardOutputFile(fileName);
    svn.setArguments(arguments);

    svn.start();
    hide();

    svn.waitForFinished(-1);
    if (svn.exitStatus() == QProcess::NormalExit && svn.exitCode() == 0) {
        ui->plainTextEdit->clear();
    }

    close();
}

void CommitDialog::diff(const QString & filePath)
{
    QProcess svn;
    svn.setProgram("svn");
    svn.setProcessChannelMode(QProcess::ForwardedChannels);
    svn.setArguments({"diff", "-N", "-x", "--ignore-eol-style", filePath});

    svn.start();
    svn.waitForFinished(-1);
}

void CommitDialog::revert(const QString & filePath)
{
    QProcess svn;
    svn.setProgram("svn");
    svn.setProcessChannelMode(QProcess::ForwardedChannels);
    svn.setArguments({"revert", filePath});

    svn.start();
    svn.waitForFinished(-1);

    // Refresh status after changes.
    status();
}

void CommitDialog::add(const QString & filePath)
{
    QProcess svn;
    svn.setProgram("svn");
    svn.setProcessChannelMode(QProcess::ForwardedChannels);
    svn.setArguments({"add", filePath});

    svn.start();
    svn.waitForFinished(-1);

    // Refresh status after changes.
    status();
}

void CommitDialog::remove(const QString & filePath)
{
    QFile::remove(filePath);

    // Refresh status after changes.
    status();
}

void CommitDialog::onDiffAction() {
    QListWidgetItem * item = ui->listWidget->currentItem();
    if (item) {
        diff(filePathFromStatus(item->text()));
    }
}

void CommitDialog::onRevertAction() {
    QListWidgetItem * item = ui->listWidget->currentItem();
    if (item) {
        revert(filePathFromStatus(item->text()));
    }
}

void CommitDialog::onAddAction() {
    QListWidgetItem * item = ui->listWidget->currentItem();
    if (item) {
        add(filePathFromStatus(item->text()));
    }
}

void CommitDialog::onRemoveAction() {
    QListWidgetItem * item = ui->listWidget->currentItem();
    if (item) {
        remove(filePathFromStatus(item->text()));
    }
}

void CommitDialog::moveToChangelist() {}
void CommitDialog::moveToNewChangelist() {}

void CommitDialog::refresh()
{
    status();
}

void CommitDialog::on_plainTextEdit_textChanged()
{
    messageAvailable = (ui->plainTextEdit->toPlainText().length() > 0);
    updateStatus();
}


void CommitDialog::on_commitButton_pressed()
{
    if (QApplication::keyboardModifiers() & action_modifier) {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save patch file..."), "changes.patch", tr("Patch files (*.patch)"));
        if (!fileName.isNull()) {
            createPatch(fileName);
        }
    }
    else {
        commit();
    }
}

void CommitDialog::on_listWidget_itemChanged(QListWidgetItem *item)
{
    if (item->checkState() == Qt::Checked) {
        if (!checkedItems.contains(item)) {
            checkedItems.append(item);
            updateStatus();
        }
    }
    else {
        if (checkedItems.contains(item)) {
            checkedItems.removeOne(item);
            updateStatus();
        }
    }
}

static bool isChangeList(QListWidgetItem *item) {
    return item->text().startsWith("---");
}

void CommitDialog::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
    // @@ Changelists are disabled, so they don't get this message.
    if (isChangeList(item)) {
        // @@ Toggle instead of set checked? Use tri-state checkbox instead.
        int i = ui->listWidget->row(item);
        for (i++; i < ui->listWidget->count(); i++) {
            item = ui->listWidget->item(i);
            if (isChangeList(item)) {
                break;
            }
            else {
                item->setCheckState(Qt::Checked);
            }
        }
    }
    else {
        QString filePath = filePathFromStatus(item->text());
        diff(filePath);
    }
}

void CommitDialog::on_listWidget_customContextMenuRequested(const QPoint &pos)
{
    QMenu contextMenu;
    QMenu changelistMenu(tr("Move to changelist"));

    QAction * action;

    QListWidgetItem * item = ui->listWidget->itemAt(pos);
    if (item != NULL) {

        if (isChangeList(item)) {
            // @@ Add option to remove changelist.
        }
        else {
            QString line = item->text();
            if (line.startsWith("M") || line.startsWith("A") || line.startsWith("D")) {
                action = contextMenu.addAction("Diff", this, SLOT(onDiffAction()));
                //action->setIcon(diffIcon);
                //QFont font = action->font();
                //font.setBold(true);
                //action->setFont(font);

                action = contextMenu.addAction("Revert", this, SLOT(onRevertAction()));
                //action->setIcon(revertIcon);

                // @@ Only show if item is a file (not a folder).
                action = contextMenu.addAction("Edit", this, SLOT(onEditAction()));
                //action->setIcon(editIcon);

                foreach(const QString & str, changeLists) {
                    changelistMenu.addAction(str, this, SLOT(moveToChangelist()));
                }
                action = changelistMenu.addAction("New changelist", this, SLOT(moveToNewChangelist()));
                changelistMenu.insertSeparator(action);


                contextMenu.addMenu(&changelistMenu);
            }
            else if (line.startsWith("?")) {
                action = contextMenu.addAction("Add", this, SLOT(onAddAction()));
                //action->setIcon(addIcon);

                action = contextMenu.addAction("Remove", this, SLOT(onRemoveAction()));

                // @@ Only show if item is a file (not a folder).
                action = contextMenu.addAction("Edit", this, SLOT(onEditAction()));
                //action->setIcon(editIcon);
            }

        }
    }

    action = contextMenu.addAction("Refresh", this, SLOT(refresh()));
    //action->setIcon(QIcon("images/refresh.png"));
    contextMenu.insertSeparator(action);

    //contextMenu.setStyleSheet("background-color: #AACCCCCC");

    contextMenu.exec(ui->listWidget->viewport()->mapToGlobal(pos));
}

void CommitDialog::on_showUnversionedCheckBox_stateChanged(int arg1)
{
    Q_UNUSED(arg1);
    refresh();
}

#include "commit_dialog.h"
#include "ui_commit_dialog.h"

#include <QProcess>
#include <QByteArray>
#include <QSettings>
#include <QMenu>

// TODO:
// - Edit context menu actions:
//      - Revert changes.
//      - Edit file (using external tool).
//      - Visual diff (using external tool).
//      - Show unversioned files. Add support for Add context menu action.
// - Add support for change lists. Use a tree instead of a list?
//      - Add default and ignore-on-commit list.
//      - Check list, checks all items.
//      - Add context menu action to move to list.
// - Add support for conflict detection and resolution.
// - Accept dropped files and file paths. Check that they are in the same svn repository and add to list.
// - Remember previous commit messages.
// - Add some support for editing properties?


CommitDialog::CommitDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
}

CommitDialog::~CommitDialog()
{
    delete ui;
}

void CommitDialog::closeEvent(QCloseEvent * e)
{
    QSettings settings("Thekla", "svn-commit-tool");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("message", ui->plainTextEdit->toPlainText());
    QDialog::closeEvent(e);
}

void CommitDialog::showEvent(QShowEvent * e)
{
    QSettings settings("Thekla", "svn-commit-tool");
    restoreGeometry(settings.value("geometry").toByteArray());
    ui->plainTextEdit->setPlainText(settings.value("message").toString());
    QDialog::showEvent(e);
}

void CommitDialog::addChangeList(const char * name)
{
    QListWidgetItem* item = new QListWidgetItem(name);
    //item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    item->setFlags(0);
    //item->setCheckState(Qt::PartiallyChecked);

    ui->listWidget->addItem(item);

    // @@ Add to list of change lists.
    //changeLists.append();
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

bool CommitDialog::status()
{
    //addChangeList("--- Changelist 'default':");

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

    foreach(const QByteArray & line, lines) {
        if (line.startsWith("M") || line.startsWith("A") || line.startsWith("D") || line.startsWith(" M")) {
            addFile(line);
        }
        else if (line.startsWith("--- Changelist")) {
            addChangeList(line);
        }
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
            QString line = item->text();
            QString filePath = line.right(line.size() - 4).trimmed();
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

    close();
}

void CommitDialog::diff(QString & filePath)
{
    QProcess svn;
    svn.setProgram("svn");
    svn.setProcessChannelMode(QProcess::ForwardedChannels);
    svn.setArguments({"diff", "-N", filePath});

    svn.start();
    svn.waitForFinished(-1);
}


void CommitDialog::on_plainTextEdit_textChanged()
{
    messageAvailable = (ui->plainTextEdit->toPlainText().length() > 0);
    updateStatus();
}

void CommitDialog::on_commitButton_pressed()
{
    commit();
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

void CommitDialog::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
    QString line = item->text();
    QString filePath = line.right(line.size() - 4).trimmed();
    diff(filePath);
}

void CommitDialog::on_listWidget_customContextMenuRequested(const QPoint &pos)
{
    QMenu contextMenu;

    QListWidgetItem * item = ui->listWidget->itemAt(pos);
    if (item) {
        QString line = item->text();
        QString filePath = line.right(line.size() - 4).trimmed();

        if (line.startsWith("M") || line.startsWith("A") || line.startsWith("D")) {
            QAction revertAction("Revert", this);
            //connect(&revertAction, SIGNAL(triggered()), this, SLOT(removeDataPoint()));

            //contextMenu.addAction(&revertAction);
            contextMenu.addAction(&revertAction);
        }
        else if (line.startsWith("?")) {
            QAction addAction("Add", this);
            contextMenu.addAction(&addAction);
        }

        // @@ Only show if item is a file (not a folder).
        QAction editAction("Edit", this);
        contextMenu.addAction(&editAction);

        contextMenu.insertSeparator(&editAction);
    }

    contextMenu.addAction("Foo");

    QAction * refreshAction = new QAction("Refresh", this);
    contextMenu.insertSeparator(refreshAction);
    //contextMenu.addAction(refreshAction);

    // @@ Not enabled yet.
    //contextMenu.exec(ui->listWidget->viewport()->mapToGlobal(pos));
}

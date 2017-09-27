#ifndef COMMIT_DIALOG_H
#define COMMIT_DIALOG_H

#include <QDialog>
#include <QFileSystemWatcher>

namespace Ui {
class Dialog;
}
class QListWidgetItem;
class QMenu;

class CommitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommitDialog(QWidget *parent = 0);
    ~CommitDialog();

    virtual void reject() override;
    virtual void closeEvent(QCloseEvent *) override;
    virtual void showEvent(QShowEvent *) override;
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

    void addChangeList(const char * name);
    void addFile(const char * filepath);
    void updateStatus();

    void watch(const QString & path);
    bool status();
    void commit();
    void createPatch(const QString & fileName);
    void diff(const QString & filePath);
    void revert(const QString & filePath);
    void add(const QString & filePath);
    void remove(const QString & filePath);

    QList<QString> paths;
    bool messageAvailable = false;
    QList<QListWidgetItem*> checkedItems;
    QList<QString> changeLists;
    //QFileSystemWatcher fileSystemWatcher;


private slots:
    void onDiffAction();
    void onRevertAction();
    void onAddAction();
    void onRemoveAction();
    void moveToChangelist();
    void moveToNewChangelist();
    void refresh();

    void on_plainTextEdit_textChanged();

    void on_commitButton_pressed();

    void on_listWidget_itemChanged(QListWidgetItem *item);

    void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_listWidget_customContextMenuRequested(const QPoint &pos);

    void on_showUnversionedCheckBox_stateChanged(int arg1);

private:
    Ui::Dialog *ui;
};

#endif // COMMIT_DIALOG_H

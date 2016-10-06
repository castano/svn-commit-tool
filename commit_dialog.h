#ifndef COMMIT_DIALOG_H
#define COMMIT_DIALOG_H

#include <QDialog>

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

    void closeEvent(QCloseEvent *);
    void showEvent(QShowEvent *);

    void addChangeList(const char * name);
    void addFile(const char * filepath);
    void updateStatus();

    bool status();
    void commit();
    void diff(QString & filePath);

    bool messageAvailable = false;
    QList<QListWidgetItem*> checkedItems;
    QList<QString> changeLists;


private slots:
    void on_plainTextEdit_textChanged();

    void on_commitButton_pressed();

    void on_listWidget_itemChanged(QListWidgetItem *item);

    void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_listWidget_customContextMenuRequested(const QPoint &pos);

private:
    Ui::Dialog *ui;
};

#endif // COMMIT_DIALOG_H

#include "commit_dialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CommitDialog w;

    if (!w.status()) {
        return 1;
    }

    w.show();

    return a.exec();
}

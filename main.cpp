#include "commit_dialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CommitDialog w;

    if (a.arguments().size() == 0) {
        w.watch(".");
    }
    else {
        foreach(const QString & arg, a.arguments()) {
            if (arg.startsWith('-')) {
                // @@ Print unknown option.
            }
            else {
                // Add path to watch list.
                w.watch(arg);
            }
        }
    }

    if (!w.status()) {
        return 1;
    }

    w.show();

    return a.exec();
}

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "DbManager.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if (argc < 2) {
        qCritical() << "USAGE:" << argv[0] << "-p\nto print all known applications on this system";
        return 1;
    }

    QString path = argv[1];
    QString canonicalPath = QDir(path).canonicalPath();

    // For speed reasons, exit as fast as possible if we are not working on an
    // application
    if (!(path == "-p" || canonicalPath.endsWith(".app") || canonicalPath.endsWith(".AppDir")
          || canonicalPath.endsWith(".AppImage")))
        return 0;

    DbManager db;

    if (path == "-p") {
        const QStringList allApps = db.allApplications();
        for (QString app : allApps) {
            qWarning() << app;
        }
        return 0;
    }

    db.handleApplication(canonicalPath);

    return 0;
}

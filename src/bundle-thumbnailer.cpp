#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

#include "dbmanager.h"


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if(argc < 2) {
        qCritical() << "USAGE:" << argv[0] << "-p\nto print all known applications on this system";
        return 1;
    }
    
    QString path = argv[1];
    QString canonicalPath = QDir(path).canonicalPath();

    // For speed reasons, exit as fast as possible if we are not working on an application
    if (! (path == "-p" || canonicalPath.endsWith(".app") || canonicalPath.endsWith(".AppDir") || canonicalPath.endsWith(".AppImage")))
        return 0;

    DbManager db;
    if (! db.isOpen()) {
        qCritical() << "Database is not open!";
        return 1;
    }

    if(path == "-p") {
        for (QString app : db.allApplications()) {
            qWarning() << app;
        }
        return 0;
    }

    db.handleApplication(canonicalPath);

    return 0;
}

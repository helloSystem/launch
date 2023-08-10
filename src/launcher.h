#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QApplication>
#include <QProcess>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDirIterator>
#include <QTime>
#include <QElapsedTimer>
#include <QRegularExpressionValidator>
#include <QIcon>
#include <QStyle>
#include <QTime>
#include <QUrl>
#include <QTableWidget>
#include <QLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QSizePolicy>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QtDBus/QtDBus>

#include <KF5/KWindowSystem/KWindowSystem>

#include "DbManager.h"
#include "ApplicationInfo.h"
#include "AppDiscovery.h"
#include "extattrs.h"

class QDetachableProcess : public QProcess

{
public:
    QDetachableProcess(QObject *parent = 0) : QProcess(parent) { }
    void detach()
    {
        this->waitForStarted();
        setProcessState(QProcess::NotRunning);
        // qDebug() << "Detaching process";
    }
};

class Launcher
{
public:
    Launcher();
    ~Launcher();

    void discoverApplications();
    int launch(QStringList args);
    int open(const QStringList args);

private:
    DbManager *db;
    void handleError(QDetachableProcess *p, QString errorString);
    QString getPackageUpdateCommand(QString pathToInstalledFile);
    QStringList executableForBundleOrExecutablePath(QString bundleOrExecutablePath);
    QString pathWithoutBundleSuffix(QString path);
};

#endif // LAUNCHER_H

#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QString>

class DbManager
{
public:
    DbManager();
    ~DbManager();
    void handleApplication(QString canonicalPath);
    bool isOpen() const;
    QStringList allApplications() const;
    bool removeAllApplications();
    bool handleNonExistingApplicationSymlink(const QString &symlinkPath) const;
    bool applicationExists(const QString &name) const;
    QString getCanOpenFromFile(QString canonicalPath);
    bool filesystemSupportsExtattr;
    static const QString localShareLaunchApplicationsPath;
    static const QString localShareLaunchMimePath;

private:
    bool _createTable();
    bool _addApplication(const QString &name);
    bool _removeApplication(const QString &name);

    unsigned int _numberOfApplications() const;
};

#endif // DBMANAGER_H

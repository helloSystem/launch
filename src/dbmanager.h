#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>

class DbManager
{
public:
    DbManager();
    ~DbManager();
    void handleApplication(QString canonicalPath);
    bool isOpen() const;
    QStringList allApplications() const;
    bool removeAllApplications();
    bool applicationExists(const QString &name) const;
    QString getCanOpenFromFile(QString canonicalPath);
    bool filesystemSupportsExtattr;

private:
    QSqlDatabase m_db;
    bool _createTable();
    bool _addApplication(const QString &name);
    bool _removeApplication(const QString &name);

    unsigned int _numberOfApplications() const;
    static const QString _databasePath;
};

#endif // DBMANAGER_H

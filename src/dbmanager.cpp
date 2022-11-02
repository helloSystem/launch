#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

DbManager::DbManager()
{
    static const QString _databasePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/launch/launch.db");

    QDir dir(QFileInfo(_databasePath).dir());
    if (!dir.exists())
        dir.mkpath(QFileInfo(_databasePath).dir().absolutePath());

    m_db = QSqlDatabase::addDatabase("QSQLITE");

    m_db.setDatabaseName(_databasePath);

    if (!m_db.open())
    {
        qDebug() << QString("Error: connection with database %1 fail").arg(_databasePath);
    }
    else
    {
        // qDebug() << QString("Database %1: connection ok").arg(_databasePath);
        _createTable(); // Creates a table if it doesn't exist. Otherwise, it will use existing table.
    }
}

DbManager::~DbManager()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
}

bool DbManager::isOpen() const
{
    return m_db.isOpen();
}

bool DbManager::_createTable()
{
    bool success = false;

    QSqlQuery query;
    query.prepare("CREATE TABLE applications(path TEXT PRIMARY KEY);");

    if (!query.exec())
    {
        // qDebug() << "Couldn't create the table 'applications': one might already exist.";
        success = false;
    }

    return success;
}

void DbManager::handleApplication(QString path)
{

    QString canonicalPath = QDir(path).canonicalPath();

    if(! (QFileInfo(canonicalPath).isDir() || QFileInfo(canonicalPath).isFile()))
    {
        qDebug() << canonicalPath << "does not exist, removing from launch.db";
        _removeApplication(canonicalPath);
    } else {
        // qDebug() << "Adding" << canonicalPath << "to launch.db";
        _addApplication(canonicalPath);
    }
}

bool DbManager::_addApplication(const QString& path)
{
    bool success = false;

    if (!path.isEmpty())
    {
        QSqlQuery queryAdd;
        queryAdd.prepare("INSERT INTO applications (path) VALUES (:path)");
        queryAdd.bindValue(":path", path);

        if(queryAdd.exec())
        {
            success = true;
        }
    }
    else
    {
        qDebug() << "add application failed: path cannot be empty";
    }

    return success;
}

bool DbManager::_removeApplication(const QString& path)
{
    bool success = false;

    if (_applicationExists(path))
    {
        QSqlQuery queryDelete;
        queryDelete.prepare("DELETE FROM applications WHERE path = (:path)");
        queryDelete.bindValue(":path", path);
        success = queryDelete.exec();

        if(!success)
        {
            qDebug() << "remove application failed: " << queryDelete.lastError();
        }
    }

    return success;
}

QStringList DbManager::allApplications() const
{
    QStringList results;
    QTextStream cout(stdout);
    QSqlQuery query("SELECT * FROM applications");
    int idName = query.record().indexOf("path");
    while (query.next())
    {
        results.append(query.value(idName).toString());
    }
    return results;
}

unsigned int DbManager::_numberOfApplications() const
{
    QSqlQuery query("SELECT COUNT(*) as cnt FROM applications");
    if (query.next()) {
        return query.value(0).toInt();
    } else {
        return 0;
    }
}

bool DbManager::_applicationExists(const QString& path) const
{
    bool exists = false;

    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT path FROM applications WHERE path = (:path)");
    checkQuery.bindValue(":path", path);

    if (checkQuery.exec())
    {
        if (checkQuery.next())
        {
            exists = true;
        }
    }
    else
    {
        qDebug() << "application exists failed: " << checkQuery.lastError();
    }

    return exists;
}

bool DbManager::removeAllApplications()
{
    bool success = false;

    QSqlQuery removeQuery;
    removeQuery.prepare("DELETE FROM applications");

    if (removeQuery.exec())
    {
        success = true;
    }
    else
    {
        qDebug() << "remove all applications failed: " << removeQuery.lastError();
    }

    return success;
}

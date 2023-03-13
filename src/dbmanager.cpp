#include "dbmanager.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

#include "extattrs.h"

// Make localShareLaunchApplicationsPath available to other classes
const QString DbManager::localShareLaunchApplicationsPath =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/launch/Applications/";

// Make localShareLaunchMimePath available to other classes
const QString DbManager::localShareLaunchMimePath =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/launch/MIME/";

DbManager::DbManager() : filesystemSupportsExtattr(false)
{

    qDebug() << "DbManager::DbManager()";

    // In order to find out whether it is worth doing costly operations regarding
    // extattrs we check whether the filesystem supports them and only use them if
    // it does. This should help speed up things on Live ISOs where extattrs don't
    // seem to be supported.
    bool ok = false;
    ok = Fm::setAttributeValueInt("/usr", "filesystemSupportsExtattr", true);
    if (ok) {
        filesystemSupportsExtattr = true;
        qDebug() << "Extended attributes are supported on /usr; using them";
    } else {
        qDebug() << "Extended attributes are not supported on /usr\n"
                    "or the command to set them needs 'chmod +s'; system will be slower";
    }

    // Create localShareLaunchMimePath and localShareLaunchApplicationsPath
    QDir dir;
    dir.mkpath(localShareLaunchMimePath);
    dir.mkpath(localShareLaunchApplicationsPath);

    // Check all symlinks in ~/.local/share/launch/ and remove any
    // that point to non-existent files.
    // TODO: Move to a location where it is
    // only run periodically, e.g., when the application starts, or run delayed
    // after an application is added
    QDirIterator it(localShareLaunchApplicationsPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        QString symlinkPath = it.next();
        if (QFileInfo(symlinkPath).isSymLink()
            && !QFileInfo(QFileInfo(symlinkPath).symLinkTarget()).exists()) {
            handleNonExistingApplicationSymlink(symlinkPath);
        }
    }

    // Check all symlinks in ~/.local/share/launch/MIME/ and remove any
    // that point to non-existent files.
    // after an application is added
    QDirIterator it2(localShareLaunchMimePath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it2.hasNext()) {
        QString symlinkPath = it2.next();
        if (QFileInfo(symlinkPath).isSymLink()
            && !QFileInfo(QFileInfo(symlinkPath).symLinkTarget()).exists()) {
            handleNonExistingApplicationSymlink(symlinkPath);
        }
    }
}

DbManager::~DbManager()
{
    qDebug() << "DbManager::~DbManager()";
}

// Read "can-open" file and return its contents as a QString;
// this is used e.g., when the system encounters application bundles
// for the first time, or when the "open" command wants to open
// documents but the filesystem doesn't support extended attributes
// Returns nullptr if no "can-open" file is found in the application bundle
QString DbManager::getCanOpenFromFile(QString canonicalPath)
{
    if (canonicalPath.endsWith(".app")) {
        QString canOpenFilePath = canonicalPath + "/Resources/can-open";
        if (!QFileInfo(canOpenFilePath).isFile())
            return QString();
        QFile f(canOpenFilePath);
        if (!f.open(QFile::ReadOnly | QFile::Text))
            return QString();
        QTextStream in(&f);
        return in.readAll();
    } else if (canonicalPath.endsWith(".desktop")) {
        bool ok = false;
        QString canOpenFromExtAttr = Fm::getAttributeValueQString(canonicalPath, "can-open", ok);
        if (ok)
            return canOpenFromExtAttr; // extattr is already set
        // The following removes everything after a ';' which XDG loves to use
        // even though they are comments in .ini files...
        // QSettings desktopFile(canonicalPath, QSettings::IniFormat);
        // QString mime = desktopFile.value("Desktop Entry/MimeType").toString();
        // Hence we have to do it the hard way. Yet another example of why XDG is
        // too complex
        QFile f(canonicalPath);
        QString mime = "";
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        if (f.isOpen()) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                QString getLine = in.readLine().trimmed();
                if (getLine.startsWith("MimeType=")) {
                    mime = getLine.remove(0, 9);
                    mime = mime.trimmed();
                    break;
                }
            }
        }
        f.close();

        return mime;
    } else {
        // TODO: AppDir
        return QString();
    }
}

void DbManager::handleApplication(QString path)
{
    QString canonicalPath = QDir(path).canonicalPath();

    if (!(QFileInfo(canonicalPath).isDir() || QFileInfo(canonicalPath).isFile())) {
        qDebug() << canonicalPath << "does not exist, removing from launch.db";
        _removeApplication(canonicalPath);
    } else {
        // qDebug() << "Adding" << canonicalPath << "to launch.db";
        _addApplication(canonicalPath);

        QString mime = getCanOpenFromFile(canonicalPath);
        if (mime.isEmpty()) {
            qDebug() << "No MIME types found in" << canonicalPath;
            return;
        } else if (mime == "") {
            qDebug() << "Empty MIME types found in" << canonicalPath;
            return;
        }

        // Split mime types into a QStringList
        QStringList mimeList = mime.split(";");
        // Remove entries that consist of only whitespace
        mimeList.removeAll("");
        // Trim whitespace from each entry; this is needed because
        // otherwise we get, e.g., "text/plain\n" instead of "text/plain"
        for (int i = 0; i < mimeList.size(); ++i) {
            mimeList[i] = mimeList[i].trimmed();
        }

        for (QString mime : mimeList) {

            if (mime.isEmpty()) {
                continue;
            }

            QString mimeDir = localShareLaunchMimePath + "/" + mime.replace("/", "_");
            if (!QFileInfo(mimeDir).isDir()) {
                QDir dir;
                dir.mkpath(mimeDir);
            }

            QString link = mimeDir + "/" + QFileInfo(canonicalPath).fileName();

            if (QFileInfo(link).isSymLink()) {
                // qDebug() << "Not creating symlink for" << mime << "because it already"
                //          << "exists";
                continue;
            }
            bool ok = QFile::link(canonicalPath, link);
            if (ok) {
                qDebug() << "Created symlink for" << mime << "in" << localShareLaunchMimePath;
            } else {
                qDebug() << "Cannot create symlink for" << mime << "in" << localShareLaunchMimePath;
            }
        }

        // If extended attributes are not supported, there is nothing else to be
        // done here
        if (!filesystemSupportsExtattr) {
            return;
        }

        // Set 'can-open' extattr if 'can-open' extattr doesn't already exist but
        // 'can-open' file exists
        bool ok = false;
        Fm::getAttributeValueQString(canonicalPath, "can-open", ok);
        if (ok)
            return; // extattr is already set

        // Set 'can-open' extattr on the application
        ok = Fm::setAttributeValueQString(canonicalPath, "can-open", mime);
        if (ok) {
            qDebug() << "Set xattr 'can-open' on" << canonicalPath;
        } else {
            qDebug() << "Cannot set xattr 'can-open' on" << canonicalPath;
        }
    }
}

bool DbManager::_addApplication(const QString &path)
{

    bool success = false;

    if (path.isEmpty())
        return false;

    // Prevent recursion by checking if the path starts with the
    // localShareLaunchPath
    if (path.startsWith(localShareLaunchApplicationsPath)
        || path.startsWith(localShareLaunchMimePath)) {
        qDebug() << "Not creating symlink for" << path << "because it is in"
                 << localShareLaunchApplicationsPath << "or" << localShareLaunchMimePath;
        return success;
    }

    // Check if a symlink to the target already exists in the directory
    // ~/.local/share/launch/Applications under any name that starts
    // with the name of the target sans extension
    // If it does, do nothing
    // If it doesn't, create a symlink to the target in the directory

    // Get name of target sans extension
    QString targetName = QFileInfo(path).fileName();
    targetName = targetName.left(targetName.lastIndexOf("."));
    QString targetCompleteSuffix = QFileInfo(path).completeSuffix();

    // Check for existing symlinks to the target

    bool found = false;

    // Check for symlinks that start with the name of the target sans extension
    // to also catch -2, -3, etc.
    QDirIterator it2(localShareLaunchApplicationsPath,
                     QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it2.hasNext()) {
        QString symlinkPath = it2.next();
        if (QFileInfo(symlinkPath).symLinkTarget() == path) {
            found = true;
            break;
        }
    }

    if (!found) {
        QString linkPath = localShareLaunchApplicationsPath + QFileInfo(path).fileName();
        int i = 2;
        while (QFileInfo(linkPath).exists()) {
            linkPath = localShareLaunchApplicationsPath + targetName + "-" + QString::number(i)
                    + "." + targetCompleteSuffix;
            i++;
        }
        if (QFile::link(path, linkPath)) {
            qDebug() << "Created symlink:" << linkPath;
            success = true;
        } else {
            qDebug() << "Failed to create symlink:" << linkPath;
        }
    }

    return success;
}

bool DbManager::_removeApplication(const QString &path)
{
    bool success = false;

    // Remove all symlinks from ~/.local/share/launch/Applications that point to
    // the target
    QDirIterator it(localShareLaunchApplicationsPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        QString symlinkPath = it.next();
        if (QFileInfo(symlinkPath).isSymLink()
            && QFileInfo(QFileInfo(symlinkPath).symLinkTarget()).canonicalFilePath()
                    == QFileInfo(path).canonicalFilePath()) {
            if (QFile::remove(symlinkPath)) {
                qDebug() << "Removed symlink:" << symlinkPath;
                success = true;
            } else {
                qDebug() << "Failed to remove symlink:" << symlinkPath;
            }
        }
    }

    return success;
}

QStringList DbManager::allApplications() const
{

    QStringList results;

    // Check all symlinks in ~/.local/share/launch/Applications and get their
    // targets if they exist, delete them otherwise
    QDirIterator it(localShareLaunchApplicationsPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        QString symlinkPath = it.next();
        if (QFileInfo(symlinkPath).isSymLink()) {
            QString target = QFileInfo(symlinkPath).symLinkTarget();
            if (QFileInfo(target).exists()) {
                results.append(target);
            } else {
                handleNonExistingApplicationSymlink(symlinkPath);
            }
        }
    }

    // Sort the results so that they are in alphabetical order and .desktop files
    // are at the end
    std::sort(results.begin(), results.end(), [](const QString &a, const QString &b) {
        if (a.endsWith(".desktop") && !b.endsWith(".desktop")) {
            return false;
        } else if (!a.endsWith(".desktop") && b.endsWith(".desktop")) {
            return true;
        } else {
            return a < b;
        }
    });

    return results;
}

unsigned int DbManager::_numberOfApplications() const
{
    // Count the number of valid symlinks in
    // ~/.local/share/launch/Applications
    unsigned int count = 0;
    QDirIterator it(localShareLaunchApplicationsPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        QString symlinkPath = it.next();
        if (QFileInfo(symlinkPath).isSymLink()) {
            QString target = QFileInfo(symlinkPath).symLinkTarget();
            if (QFileInfo(target).exists()) {
                count++;
            } else {
                handleNonExistingApplicationSymlink(symlinkPath);
            }
        }
    }
    return count;
}

bool DbManager::handleNonExistingApplicationSymlink(const QString &symlinkPath) const
{
    // Exit if symlinkPath is not a symlink
    if (!QFileInfo(symlinkPath).isSymLink()) {
        return false;
    }
    qDebug() << "Removing symlink to non-existent file:" << symlinkPath;
    // TODO: We could get fancy here and check whether similar applications exist
    // at the target path (e.g., newer versions) and if so, ask the user whether
    // they want to create a new symlink to the new location
    QFile::remove(symlinkPath);
    return true;
}

bool DbManager::applicationExists(const QString &path) const
{
    bool exists = false;
    // Check all symlinks in ~/.local/share/launch/Applications and get their
    // targets if they exist, delete them otherwise. If the target of a symlink
    // matches the path, the application exists
    QDirIterator it(localShareLaunchApplicationsPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        QString symlinkPath = it.next();
        if (QFileInfo(symlinkPath).isSymLink()) {
            QString target = QFileInfo(symlinkPath).symLinkTarget();
            if (QFileInfo(target).exists()) {
                if (target == path) {
                    exists = true;
                    break;
                }
            } else {
                handleNonExistingApplicationSymlink(symlinkPath);
            }
        }
    }
    return exists;
}

bool DbManager::removeAllApplications()
{
    bool success = false;

    // Delete all symlinks in ~/.local/share/launch/Applications
    QDirIterator it(localShareLaunchApplicationsPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        QString symlinkPath = it.next();
        if (QFileInfo(symlinkPath).isSymLink()) {
            qDebug() << "Removing symlink:" << symlinkPath;
            QFile::remove(symlinkPath);
        }
    }
    return success;
}

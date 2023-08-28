#include "Executable.h"
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMimeDatabase>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>

bool Executable::isExecutable(const QString& path) {
    QFileInfo fileInfo(path);
    return fileInfo.isExecutable();
}

bool Executable::hasShebang(const QString& path) {
    QFile file(path);
    QFileInfo fileInfo(path);

    // If it is a directory, it cannot have a shebang, so return false
    if (fileInfo.isDir()) {
        qDebug() << "File is a directory, so it cannot have a shebang.";
        return false;
    }

    // If it is a symlink, we need to check the target
    if (fileInfo.isSymLink()) {
        qDebug() << "File is a symlink, so we need to check the target.";
        QString target = fileInfo.symLinkTarget();
        qDebug() << "Target:" << target;
        return hasShebang(target);
    }

    qDebug() << "Checking file:" << path;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << path;
        return false;
    }

    // Check if the file starts with the shebang sequence
    QByteArray firstTwoBytes = file.read(2);
    qDebug() << "First two bytes:" << firstTwoBytes;
    if (firstTwoBytes == "#!") {
        qDebug() << "File has a shebang.";
        // Exception: If the MIME type is e.g., "application/x-raw-disk-image",
        // then we ignore the shebang
        if (QMimeDatabase().mimeTypeForFile(path).name().contains("disk-image")) {
            qDebug() << "File is a disk image, so we ignore the shebang.";
            return false;
        }
        return true;
    } else {
        qDebug() << "File does not have a shebang.";
        return false;
    }
}

bool Executable::isElf(const QString& path) {
    QMimeDatabase mimeDatabase;
    QString mimeType = mimeDatabase.mimeTypeForFile(path).name();
    // NOTE: Not all "application/..." mime types are ELF executables, e.g., disk images
    // have "application/..." mime types, too.
    if (mimeType == "application/x-executable" || \
        mimeType == "application/x-pie-executable" || \
        mimeType == "application/vnd-appimage") {
        qDebug() << "File is an ELF executable.";
        return true;
    } else {
        qDebug() << "File is not an ELF executable.";
        return false;
    }
}

bool Executable::askUserToMakeExecutable(const QString& path) {
    if (!isExecutable(path)) {
        QString message = tr("The file is not executable:\n%1\n\nDo you want to make it executable?\n\nYou should only do this if you trust this file.")
                          .arg(path);
        QMessageBox::StandardButton response = QMessageBox::question(nullptr, tr("Make Executable"), message,
                                                                   QMessageBox::Yes | QMessageBox::No);

        if (response == QMessageBox::Yes) {

            QProcess process;
            QStringList arguments;
            arguments << "+x" << path;

            process.setProgram("chmod");
            process.setArguments(arguments);

            process.start();
            if (process.waitForFinished() && process.exitCode() == 0) {
                // QMessageBox::information(nullptr, tr("Success"), tr("File is now executable."));
                return true;
            } else {
                QMessageBox::warning(nullptr, tr("Error"), tr("Failed to make the file executable."));
                return false;
            }
        } else {
            // QMessageBox::information(nullptr, tr("Info"), tr("File was not made executable."));
            return false;
        }
    } else {
        // QMessageBox::information(nullptr, tr("Info"), tr("The file is already executable."));
        return true;
    }
}

bool Executable::hasShebangOrIsElf(const QString& path) {
    if (hasShebang(path)) {
        qDebug() << tr("File has a shebang.");
        return true;
    } else if (isElf(path)) {
        qDebug() << tr("File is an ELF.");
        return true;
    } else {
        qDebug() << tr("File does not have a shebang or is not an ELF.");
        return false;
    }
}

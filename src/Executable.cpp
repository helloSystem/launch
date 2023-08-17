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
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << path;
        return false;
    }

    QTextStream in(&file);
    QString firstLine = in.readLine();
    file.close();

    return firstLine.startsWith("#!");
}

bool Executable::isElf(const QString& path) {
    QMimeDatabase mimeDatabase;
    QMimeType mimeType = mimeDatabase.mimeTypeForFile(path);

    QStringList allMimeTypeNames = {mimeType.name()};
    allMimeTypeNames.append(mimeType.allAncestors());
    // qDebug() << "All MIME types for file:" << allMimeTypeNames;

    if (allMimeTypeNames.contains("application/x-executable")) {
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

#include "finder.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QSettings>

QFileInfoList Finder::findAppsInside(QStringList locationsContainingApps, QFileInfoList candidates, QString firstArg)
{
     foreach (QString directory, locationsContainingApps) {
        QDirIterator it(directory, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            QString filename = it.next();
            // qDebug() << "probono: Processing" << filename;
            QString nameWithoutSuffix = QFileInfo(QDir(filename).canonicalPath()).baseName();
            QFileInfo file(filename);
                        
            if (file.fileName() == firstArg + ".app"){
                QString AppCand = filename + "/" + nameWithoutSuffix;
                qDebug() << "################### Checking" << AppCand;
                if(QFileInfo(AppCand).exists() == true){
                    qDebug() << "# Found" << AppCand;
                    candidates.append(AppCand);
                }
            }
            else if (file.fileName() == firstArg + ".AppDir"){
                QString AppCand = filename + "/" + "AppRun";
                
                qDebug() << "################### Checking" << AppCand;
                if(QFileInfo(AppCand).exists() == true){
                    qDebug() << "# Found" << AppCand;
                    candidates.append(AppCand);
                }
            }
            else if (file.fileName() == firstArg + ".AppImage" || file.fileName() == firstArg.replace(" ", "_") + ".AppImage" || file.fileName().endsWith(".AppName") &  file.fileName().startsWith(firstArg + "-") || file.fileName().startsWith(firstArg.replace(" ", "_") + "-")) {
                QString AppCand = getExecutable(filename);
                candidates.append(AppCand);
            }
            else if (file.fileName() == firstArg + ".desktop") {
                // load the .desktop file for parsing - QSettings::IniFormat returns values as strings by default
                // see https://doc.qt.io/qt-5/qsettings.html
                QSettings desktopFile(filename, QSettings::IniFormat);
                QString AppCand = desktopFile.value("Desktop Entry/Exec").toString();
                
                // null safety check
                if (AppCand != NULL) {
                    qDebug() << "# Found" << AppCand;
                    candidates.append(AppCand);
                }
            }
            else if (locationsContainingApps.contains(filename) == false && file.isDir() && filename.endsWith("/..") == false && filename.endsWith("/.") == false && filename.endsWith(".app") == false && filename.endsWith(".AppDir") == false && filename.endsWith(".AppImage") == false) {
                // Now we have a directory that is not an .app bundle nor an .AppDir
                // Shall we descend into it? Only if it contains at least one application, to optimize for speed
                // by not descending into directory trees that do not contain any applications at all. Can make
                // a big difference.
                QStringList nameFilter({"*.app", "*.AppDir", "*.desktop", "*.AppImage"});
                QDir directory(filename);
                int numberOfAppsInDirectory = directory.entryList(nameFilter).length();
                if(numberOfAppsInDirectory > 0) {
                    qDebug() << "# Descending into" << filename;
                    QStringList locationsToBeChecked = {filename};
                    candidates = findAppsInside(locationsToBeChecked, candidates, firstArg);
                }
            }
        }
    }
    return candidates;
}

QString Finder::getExecutable(QString &firstArg)
{
    
    QString executable = nullptr;
    
    // check if the file exists
    /*if (!QFile::exists(firstArg)) {
        // try replacing space with _
        firstArg = firstArg.replace(" ", "_");
    }*/
        
    if (QFile::exists(firstArg)) {
        // get the file info
        QFileInfo info = QFileInfo(firstArg);
        
        if (firstArg.endsWith(".AppDir") || firstArg.endsWith(".app") || firstArg.endsWith(".AppImage")) {
            qDebug() << "# Found" << firstArg;
            
            // The potential candidate
            QString candidate;
            
            if(firstArg.endsWith(".AppDir")) {
                candidate = firstArg + "/AppRun";
            }
            else if (firstArg.endsWith(".AppImage")) {
                // this is a .AppImage file, we have nothing else to do here, so just make it a candidate
                candidate = firstArg;
            }
            else {
                // The .app could be a symlink, so we need to determine the nameWithoutSuffix from its target
                QFileInfo fileInfo = QFileInfo(QDir(firstArg).canonicalPath());
                QString nameWithoutSuffix = QFileInfo(fileInfo.completeBaseName()).fileName();
                candidate = firstArg + "/" + nameWithoutSuffix;
            }

            QFileInfo candinfo = QFileInfo(candidate);
            if (candinfo.isExecutable()) {
                executable = candidate;
            }
            
        }
        else if (info.isExecutable()){
            qDebug() << "# Found executable" << firstArg;
            executable = firstArg;
        }
    }
    
    return executable;
}




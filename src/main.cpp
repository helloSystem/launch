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
#include <QRegExpValidator>
#include <QApplication>
#include <QSettings>
#include <QIcon>
#include <QStyle>
#include <QVersionNumber>
#include "utils/finder.h"

/*
 * This tool handles four types of applications:
 * 1. Executables on the $PATH
 * 2. Simplified .app bundles (Convention: AppName.app/AppName is an executable) in well-known locations
 * 3. Simplified .AppDir directories (Convention: AppName[.AppDir]/AppRun is an executable) in well-known locations
 * 4. AppName.desktop files in well-known locations (TODO: Not implemented yet)
 *
 * Usage:
 * launch <application to be launched> [<arguments>]    Launch the specified application
 * launch -l                                            Print well-known locations for applications

Similar to:
https://github.com/probonopd/appwrapper
and:
GNUstep openapp

TODO: Make the behavior resemble /usr/local/GNUstep/System/Tools/openapp (a bash script)

user@FreeBSD$ /usr/local/GNUstep/System/Tools/openapp --help
usage: openapp [--find] [--debug] application [arguments...]

application is the complete or relative name of the application
program with or without the .app extension, like Ink.app.

[arguments...] are the arguments to the application.

If --find is used, openapp prints out the full path of the application
executable which would be executed, without actually executing it.  It
will also list all paths that are attempted.

*/

class QDetachableProcess : public QProcess
{
public:
    QDetachableProcess(QObject *parent = 0) : QProcess(parent){}
    void detach()
    {
        this->waitForStarted();
        setProcessState(QProcess::NotRunning);
        // qDebug() << "Detaching process";
    }
};

// If a package needs to be updated, tell the user how to do this,
// or even offer to do it
QString getPackageUpdateCommand(QString pathToInstalledFile){
    QString candidate = QStandardPaths::findExecutable("pkg");
    if (candidate != "") {
        // We are on FreeBSD
        QProcess p;
        QString exe = "pkg which " + pathToInstalledFile;
        p.start(exe);
        p.waitForFinished();
        QString output(p.readAllStandardOutput());
        if (output != "") {
            qDebug() << output;
            QRegExp rx(".* was installed by package (.*)-(.*)\n");
            if (rx.indexIn(output) == 0) {
                return QString("sudo pkg install %1").arg(rx.cap(1));
            }
        }
    }
    // TODO: Implement the same for deb and rpm...
    // In all other cases, return a blank string
    return "";
}

// Translate cryptic errors into clear text, and possibly even offer buttons to take action
void handleError(QDetachableProcess *p, QString errorString){
    QMessageBox qmesg;

    // Make this error message not appear in the Dock // FIXME: Does not work, why?
    qmesg.setWindowFlag(Qt::SubWindow);

    // If we can't make the icon go away in the Dock, at least make it a non-placeholder icon
    // Not sure if the Dock is already reflecting the WindowIcon correctly
    // FIXME: Does not work, why?
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    qmesg.setWindowIcon(icon);

    QRegExp rx(".*ld-elf.so.1: (.*): version (.*) required by (.*) not found.*");
    QRegExp rxPy(".*ModuleNotFoundError: No module named '(.*)'.*");
    QFileInfo fi(p->program());
    QString title = fi.completeBaseName(); // https://doc.qt.io/qt-5/qfileinfo.html#completeBaseName
    if(errorString.contains("FATAL: kernel too old")) {
        QString cleartextString = "The Linux compatibility layer reports an older kernel version than what is required to run this application.\n\n" \
                                  "Please run\nsudo sysctl compat.linux.osrelease=5.0.0\nand try again.";
        qmesg.warning( nullptr, title, cleartextString );
    } else if (rx.indexIn(errorString) == 0) {
        QString outdatedLib = rx.cap(1);
        QString versionNeeded = rx.cap(2);
        QFile f(outdatedLib);
        QFileInfo fileInfo(f.fileName());
        QString outdatedLibShort(fileInfo.fileName());
        QString requiredBy = rx.cap(3);
        QString cleartextString = QString("This application requires at least version %2 of %1 to run.").arg(outdatedLibShort).arg(versionNeeded);
        if (getPackageUpdateCommand(outdatedLib) != "") {
            cleartextString.append(QString("\n\nPlease update it with\n%1\nand try again.").arg(getPackageUpdateCommand(outdatedLib)));
        } else {
            cleartextString.append(QString("\n\nPlease update it and try again.").arg(getPackageUpdateCommand(outdatedLib)));
        }
        qmesg.warning( nullptr, title, cleartextString );
    } else if (rxPy.indexIn(errorString) == 0) {
        QString missingPyModule = rxPy.cap(1);
        QString cleartextString = QString("This application requires the Python module %1 to run.\n\nPlease install it and try again.").arg(missingPyModule);
        qmesg.warning( nullptr, title, cleartextString );
    } else {
        qmesg.warning( nullptr, title, errorString );
    }
}

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);

    // Setting a busy cursor in this way seems only to affect the own application's windows
    // rather than the full screen, which is why it is not suitable for this tool
    // QApplication::setOverrideCursor(Qt::WaitCursor);

    // Launch an application but initially watch for errors and display a Qt error message
    // if needed. After some timeout, detach the process of the application, and exit this helper

    QStringList args = app.arguments();

    args.pop_front();

    QStringList locationsContainingApps = {};

    // Add some location in $HOME
    locationsContainingApps.append(QDir::homePath() + "/Applications");
    locationsContainingApps.append(QDir::homePath() + "/bin");
    locationsContainingApps.append(QDir::homePath() + "/.bin");

    // Add system-wide locations
    // TODO: Find a better and more complete way to specify the GNUstep ones
    locationsContainingApps.append({"/Applications", "/System", "/Library",
                                    "/usr/local/GNUstep/Local/Applications",
                                    "/usr/local/GNUstep/System/Applications",
                                    "/usr/GNUstep/Local/Applications",
                                    "/usr/GNUstep/System/Applications"
                                   });

    // Add legacy locations for XDG compatibility
    // On FreeBSD: "/home/user/.local/share/applications", "/usr/local/share/applications", "/usr/share/applications"
    locationsContainingApps.append(QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation));

    locationsContainingApps.removeDuplicates(); // Make unique

    if(args.isEmpty()){
        qDebug() << "USAGE:" << argv[0] << "<application to be launched> [<arguments>]" ;
        exit(1);
    }

    if(args[0] == "-l") {
        args.pop_front();
        qDebug() << locationsContainingApps;
        exit(0);
    }

    QDetachableProcess p;
    Finder finder;
    
    // Check whether the first argument exists or is on the $PATH

    QString executable = nullptr;
    QString firstArg = nullptr;

    // First, try to find something we can launch at the path,
    // either an executable or an .AppDir or an .app bundle
    firstArg = args.first();
    
    executable = finder.getExecutable(firstArg);    
    
    // Second, try to find an executable file on the $PATH
    if(executable == nullptr) {
        QString candidate = QStandardPaths::findExecutable(firstArg);
        if (candidate != "") {
            qDebug() << "Found" << candidate << "on the $PATH";
            executable = candidate; // Returns the absolute file path to the executable, or an empty string if not found
        }
    }

    // Third, if still not found, then try other means of location
    // the preferred instance of the requested application, e.g., from a list of
    // directories that hold .desktop files, .app bundles, .AppDir directories, or from a database.
    // What follows is a very simplistic implementation, one could become much fancier for this.
    // NOTE: For performance reasons, try to limit the launchable strings to
    // what can be derived from file/directory names, so that we do not have to parse
    // the contents of many desktop files, plist files, etc. here. If we were using a database,
    // this restriction could be removed.
    if(executable == nullptr) {

        // Measure the time it takes to look up candidates
        QElapsedTimer timer;
        timer.start();

        // Iterate recursively through locationsContainingApps searching for AppRun files in matchingly named AppDirs

        QFileInfoList candidates;
        QString firstArgWithoutWellKnownSuffix = firstArg.replace(".AppDir", "").replace(".app", "").replace(".desktop" ,"").replace(".AppImage", "");

        candidates = finder.findAppsInside(locationsContainingApps, candidates, firstArgWithoutWellKnownSuffix);

        qDebug() << "Took" << timer.elapsed() << "milliseconds to find candidates via the filesystem";
        qDebug() << "Candidates:" << candidates;
        
        QFileInfo candidate = candidates.first().absoluteFilePath();
        QFileInfoList::iterator it;
        
        qDebug() << candidate;
                
        // Attempt version detection
        if (candidates.size() > 1) {
            
            // todo: loop through and compare versions
            
            for (int i = 0; i < candidates.size(); i++)
            {
                if (!candidate.fileName().contains("-")) {
                        candidate = candidates[i].absoluteFilePath();
                        continue;
                }
                
                try {
                    QStringList previousVersion = candidate.fileName().split("-");
                    QStringList curVersion = candidates[i].fileName().split("-");
                
                    QVersionNumber previousVer = QVersionNumber::fromString(previousVersion[1]);
                    QVersionNumber newVer = QVersionNumber::fromString(curVersion[1]);
                
                    int compare = QVersionNumber::compare(previousVer, newVer);
                    qDebug() << compare;
                    if (compare == -1) {
                        // previous one is older, use newer one
                        candidate = candidates[i].absoluteFilePath();
                    }
                } catch(std::exception &e) {
                    // catch any exeption that may occure
                    qDebug() << "Failed to compare application versions";
                }
            }
        } 
        
        qDebug() << "Selected candidate:" << candidate.absoluteFilePath();
        executable = candidate.absoluteFilePath();
    }

    p.setProgram(executable);
    args.pop_front();
    p.setArguments(args);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // env.insert("QT_SCALE_FACTOR", "2");
    p.setProcessEnvironment(env);

    p.setProcessChannelMode(QProcess::ForwardedOutputChannel); // Forward stdout onto the main process

    p.start();

    // Blocks until process has started
    if (!p.waitForStarted()) {
        QMessageBox::warning( nullptr, firstArg, "Could not launch\n" + firstArg );
        return(1);
    }

    // TODO: Now would be a good time to signal the Dock to start showing a bouncing the icon of the
    // application that is being launched, until the application has its own icon in the Dock

    p.waitForFinished(3 * 1000); // Blocks until process has finished or timeout occured (x seconds)
    // Errors occuring thereafter will not be reported to the user in a message box anymore.
    // This should cover most errors like missing libraries, missing interpreters, etc.

    if (p.state() == 0 and p.exitCode() != 0) {
        qDebug("Process is not running anymore and exit code was not 0");
        const QString error = p.readAllStandardError();
        if (!error.isEmpty()) {
            qDebug() << error;
            handleError(&p, error);
        }
        return(p.exitCode());
    }

    // Crude workaround for https://github.com/helloSystem/launch/issues/4
    // FIXME: Find a way to p.detach(); and return(0); without crashing the payload application
    // when it writes to stderr

    p.waitForFinished(-1);

}


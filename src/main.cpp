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
#include <QIcon>
#include <QStyle>

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


QFileInfoList findAppsInside(QStringList locationsContainingApps, QFileInfoList candidates, QString firstArg)
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
            else if (file.fileName() == firstArg + ".desktop") {
                // .desktop file
                qDebug() << "# Found" << file.fileName() << "TODO: Parse it for Exec=";
            }
            else if (locationsContainingApps.contains(filename) == false && file.isDir() && filename.endsWith("/..") == false && filename.endsWith("/.") == false && filename.endsWith(".app") == false && filename.endsWith(".AppDir") == false) {
                // Now we have a directory that is not an .app bundle nor an .AppDir
                // Shall we descend into it? Only if it contains at least one application, to optimize for speed
                // by not descending into directory trees that do not contain any applications at all. Can make
                // a big difference.
                QStringList nameFilter({"*.app", "*.AppDir", "*.desktop"});
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

    // Check whether the first argument exists or is on the $PATH

    QString executable = nullptr;
    QString firstArg = nullptr;

    // First, try to find something we can launch at the path,
    // either an executable or an .AppDir or an .app bundle
    firstArg = args.first();
    if (QFile::exists(firstArg)){
        QFileInfo info = QFileInfo(firstArg);
        if ( firstArg.endsWith(".AppDir") || firstArg.endsWith(".app") ){
            qDebug() << "# Found" << firstArg;
            QString candidate;
            if(firstArg.endsWith(".AppDir")) {
                candidate = firstArg + "/AppRun";
            }
            else {
                // The .app could be a symlink, so we need to determine the nameWithoutSuffix from its target
                QFileInfo fileInfo = QFileInfo(QDir(firstArg).canonicalPath());
                QString nameWithoutSuffix = QFileInfo(fileInfo.completeBaseName()).fileName();
                candidate = firstArg + "/" + nameWithoutSuffix;
            }

            QFileInfo candinfo = QFileInfo(candidate);
            if(candinfo.isExecutable()) {
                executable = candidate;
            }

        }
        else if (info.isExecutable()){
            qDebug() << "# Found executable" << firstArg;
            executable = args.first();
        }
    }

    // Second, try to find an executable file on the $PATH
    if(executable == nullptr){
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
        QString firstArgWithoutWellKnownSuffix = firstArg.replace(".AppDir", "").replace(".app", "").replace(".desktop" ,"");

        candidates = findAppsInside(locationsContainingApps, candidates, firstArgWithoutWellKnownSuffix);

        qDebug() << "Took" << timer.elapsed() << "milliseconds to find candidates via the filesystem";
        qDebug() << "Candidates:" << candidates;

        foreach (QFileInfo candidate, candidates) {
            // Now that we may have collected different candidates, decide on which one to use
            // e.g., the one with the highest self-declared version number. Again, a database might come in handy here
            // For now, just use the first one
            qDebug() << "Selected candidate:" << candidate.absoluteFilePath();
            executable = candidate.absoluteFilePath();
            break;
        }
    }

    p.setProgram(executable);
    args.pop_front();
    p.setArguments(args);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // env.insert("QT_SCALE_FACTOR", "2");

    // Set LAUNCHED_EXECUTABLE and LAUNCHED_BUNDLE environment variables
    // On FreeBSD, can use
    // procstat -e $(xprop | grep PID | cut -d " " -f 3)
    // to get these for any given Xorg window
    env.insert("LAUNCHED_EXECUTABLE", executable);
    QFileInfo info = QFileInfo(executable);
    if ( info.dir().absolutePath().endsWith(".AppDir") || info.dir().absolutePath().endsWith(".app") ){
        // qDebug() << "# Bundle" << info.dir().absolutePath();
        env.insert("LAUNCHED_BUNDLE", info.dir().absolutePath());
    }
    // qDebug() << "# env" << env.toStringList();
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


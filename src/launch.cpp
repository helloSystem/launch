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
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <QTime>
#include <QUrl>
#include <QtDBus/QtDBus>

#include <KF5/KWindowSystem/KWindowSystem>

#include "dbmanager.h"
#include "applicationinfo.h"
#include "appdiscovery.h"
#include "extattrs.h"

/*
 * All documents shall be opened through this tool on helloDesktop
 *
 * This tool handles four types of applications:
 * 1. Executables
 * 2. Simplified .app bundles (Convention: AppName.app/AppName is an executable)
 * 3. Simplified .AppDir directories (Convention: AppName[.AppDir]/AppRun is an executable)
 * 4. AppName.desktop files
 *
 * The applications are searched
 * 1. At the path given as the first argument
 * 2. On the $PATH
 * 3. In launch.db
 * 4. As a fallback, via Baloo? (not implemented yet)
 *
 * launch.db is populated
 * 1. By this tool (currently each time it is invoked; could be optimized to: only if application is not found)
 * 2. By the file manager when one looks at applications (can be implemented natively or using bundle-thumbnailer)
 *
 * If an application bundle is being launched that has existing windows and no arguments are passed in,
 * the existing windows are brought to the front instead of starting a new process.
 *
 * The environment variable LAUNCHED_EXECUTABLE is populated, and for bundles, the LAUNCHED_BUNDLE
 * environment variable is also polulated.
 *
 * Usage:
 * launch <application to be launched> [<arguments>]    Launch the specified application

Similar to https://github.com/probonopd/appwrapper and GNUstep openapp

TODO:
* Possibly optimize for launch speed by only populating launch.db if the application is not found
* Make the behavior resemble /usr/local/GNUstep/System/Tools/openapp (a bash script)

user@FreeBSD$ /usr/local/GNUstep/System/Tools/openapp --help
usage: openapp [--find] [--debug] application [arguments...]

application is the complete or relative name of the application
program with or without the .app extension, like Ink.app.

[arguments...] are the arguments to the application.

If --find is used, openapp prints out the full path of the application
executable which would be executed, without actually executing it.  It
will also list all paths that are attempted.

*/

DbManager *db = new DbManager();

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

// Find apps on well-known paths and put them into launch.db
void discoverApplications()
{
    // Measure the time it takes to look up candidates
    QElapsedTimer timer;
    timer.start();
    AppDiscovery *ad = new AppDiscovery(db);
    QStringList wellKnownLocs = ad->wellKnownApplicationLocations();
    ad->findAppsInside(wellKnownLocs);
    qDebug() << "Took" << timer.elapsed() << "milliseconds to discover applications and add them to launch.db, part of which was logging";
    // ad->~AppDiscovery(); // FIXME: Doing this here would lead to a crash; why?
}

QStringList executableForBundleOrExecutablePath(QString bundleOrExecutablePath)
{
    QStringList executableAndArgs = {};
    if (QFile::exists(bundleOrExecutablePath)){
        QFileInfo info = QFileInfo(bundleOrExecutablePath);
        if ( bundleOrExecutablePath.endsWith(".AppDir") || bundleOrExecutablePath.endsWith(".app") ){
            qDebug() << "# Found" << bundleOrExecutablePath;
            QString executable_candidate;
            if(bundleOrExecutablePath.endsWith(".AppDir")) {
                executable_candidate = bundleOrExecutablePath + "/AppRun";
            }
            else {
                // The .app could be a symlink, so we need to determine the nameWithoutSuffix from its target
                QString nameWithoutSuffix = QFileInfo(bundleOrExecutablePath).completeBaseName();
                executable_candidate = bundleOrExecutablePath + "/" + nameWithoutSuffix;
            }
            QFileInfo candinfo = QFileInfo(executable_candidate);
            if(candinfo.isExecutable()) {
                executableAndArgs = QStringList({executable_candidate});
            }

        }
        else if (bundleOrExecutablePath.endsWith(".AppImage") || bundleOrExecutablePath.endsWith(".appimage")) {
            qDebug() << "# Found non-executable AppImage" << bundleOrExecutablePath;
            executableAndArgs = QStringList({bundleOrExecutablePath});
        }
        else if (bundleOrExecutablePath.endsWith(".desktop")) {
            qDebug() << "# Found .desktop file" << bundleOrExecutablePath;
            QSettings desktopFile(bundleOrExecutablePath, QSettings::IniFormat);
            QString s = desktopFile.value("Desktop Entry/Exec").toString();
            QStringList execStringAndArgs = QProcess::splitCommand(s); // This should hopefully treat quoted strings halfway correctly
            if(execStringAndArgs.first().count(QLatin1Char('\\')) > 0) {
                QMessageBox::warning(nullptr, " ",
                                     "Launching such complex .desktop files is not supported yet.\n" + bundleOrExecutablePath );
                exit(1);
            }
            else {
                executableAndArgs = execStringAndArgs;
            }
        }
        else if (info.isExecutable() && ! info.isDir()) {
            qDebug() << "# Found executable" << bundleOrExecutablePath;
            executableAndArgs =  QStringList({bundleOrExecutablePath});
        }
    }
    return executableAndArgs;
}

QString pathWithoutBundleSuffix(QString path)
{
    // FIXME: This is very lazy; TODO: Do it properly
    return path.replace(".AppDir", "").replace(".app", "").replace(".desktop" ,"").replace(".AppImage", "").replace(".appimage", "");
}


int launch(QStringList args)
{
    QDetachableProcess p;

    QString executable = nullptr;
    QString firstArg = args.first();
    qDebug() << "launch firstArg:" << firstArg;

    QFileInfo fileInfo = QFileInfo(firstArg);
    QString nameWithoutSuffix = QFileInfo(fileInfo.completeBaseName()).fileName();

    // Remove trailing slashes
    while( firstArg.endsWith("/") ){
        firstArg.remove(firstArg.length()-1,1);
    }

    args.pop_front();

    // First, try to find something we can launch at the path that was supplied as an argument,
    // Examples:
    //    /Applications/LibreOffice.app
    //    /Applications/LibreOffice.AppDir
    //    /Applications/LibreOffice.AppImage
    //    /Applications/libreoffice

    QStringList e = executableForBundleOrExecutablePath(firstArg);
    if(e.length() > 0)
        executable = e.first();

    // Second, try to find an executable file on the $PATH
    if(executable == nullptr){
        QString candidate = QStandardPaths::findExecutable(firstArg);
        if (candidate != "") {
            qDebug() << "Found" << candidate << "on the $PATH";
            executable = candidate; // Returns the absolute file path to the executable, or an empty string if not found
        }
    }

    // Third, try to find an executable from the applications in launch.db

    QString selectedBundle = "";
    if(executable == nullptr) {

        // Measure the time it takes to look up candidates
        QElapsedTimer timer;
        timer.start();

        // Iterate recursively through locationsContainingApps searching for AppRun files in matchingly named AppDirs

        QFileInfoList candidates;

        QStringList allAppsFromDb = db->allApplications();

        for (QString appBundleCandidate : allAppsFromDb) {
            // Now that we may have collected different candidates, decide on which one to use
            // e.g., the one with the highest self-declared version number.
            // Also we need to check whether the appBundleCandidate exist
            // For now, just use the first one
            if ( pathWithoutBundleSuffix(appBundleCandidate).endsWith(firstArg)) {
                if(QFileInfo(appBundleCandidate).exists()) {
                    qDebug() << "Selected from launch.db:" << appBundleCandidate;
                    selectedBundle = appBundleCandidate;
                    break;
                } else {
                    db->handleApplication(appBundleCandidate); // Remove from launch.db it if it does not exist
                }
            }
        }

        // For the selectedBundle, get the launchable executable
        if(selectedBundle == ""){
            QMessageBox::warning(nullptr, " ", "Cannot find\n" + firstArg );
            exit(1);
        } else {
            QStringList e = executableForBundleOrExecutablePath(selectedBundle);
            if(e.length() > 0)
                executable = e.first();
        }
    }

    // .desktop files can have arguments in them, and we need to insert the arguments given
    // to launch on the command line into the arguments coming from the desktop file.
    // So we have to construct arguments from the desktop file and from the command line
    // Things like this make XDG overly complex!
    QStringList constructedArgs = {};

    QStringList execLinePartsFromDesktopFile = executableForBundleOrExecutablePath(firstArg);
    if(execLinePartsFromDesktopFile.length() > 1) {
        execLinePartsFromDesktopFile.pop_front();
        for(const QString execLinePartFromDesktopFile : execLinePartsFromDesktopFile) {
            if(execLinePartFromDesktopFile == "%f"  || execLinePartFromDesktopFile == "%u")
                constructedArgs.append(args[0]);
            else if (execLinePartFromDesktopFile == "%F"  || execLinePartFromDesktopFile == "%U")
                constructedArgs.append(args);
            else
                constructedArgs.append(execLinePartFromDesktopFile);
        }
        args = constructedArgs;
    }

    // Proceed to launch application
    p.setProgram(executable);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    qDebug() << "# Setting LAUNCHED_EXECUTABLE environment variable to" << executable;
    env.insert("LAUNCHED_EXECUTABLE", executable);
    QFileInfo info = QFileInfo(executable);

    // Hackish workaround; TODO: Replace by something cleaner
    if (executable.toLower().endsWith(".appimage")){
        if (! info.isExecutable()) {
            p.setProgram("runappimage");
            args.insert(0, executable);
        }
    }

    p.setArguments(args);

    // Hint: LAUNCHED_EXECUTABLE and LAUNCHED_BUNDLE environment variables
    // can be gotten from X11 windows on FreeBSD with
    // procstat -e $(xprop | grep PID | cut -d " " -f 3)
    qDebug() << "info.canonicalFilePath():" << info.canonicalFilePath();
    qDebug() << "executable:" << executable;
    env.remove("LAUNCHED_BUNDLE"); // So that nested launches won't leak LAUNCHED_BUNDLE from parent to child application; works
    if (info.dir().absolutePath().toLower().endsWith(".appdir") || info.dir().absolutePath().toLower().endsWith(".app")){
        qDebug() << "# Bundle directory (.app, .AppDir)" << info.dir().canonicalPath();
        qDebug() << "# Setting LAUNCHED_BUNDLE environment variable to it";
        env.insert("LAUNCHED_BUNDLE", info.dir().canonicalPath()); // Resolve symlinks so as to show the real location
    } else if (fileInfo.canonicalFilePath().toLower().endsWith(".appimage")){
        qDebug() << "# Bundle file (.AppImage)" << fileInfo.canonicalFilePath();
        qDebug() << "# Setting LAUNCHED_BUNDLE environment variable to it";
        env.insert("LAUNCHED_BUNDLE", fileInfo.canonicalFilePath()); // Resolve symlinks so as to show the real location
    } else if (fileInfo.canonicalFilePath().endsWith(".desktop")){
        qDebug() << "# Bundle file (.desktop)" << fileInfo.canonicalFilePath();
        qDebug() << "# Setting LAUNCHED_BUNDLE environment variable to it";
        env.insert("LAUNCHED_BUNDLE", fileInfo.canonicalFilePath()); // Resolve symlinks so as to show the real location
    }
    // qDebug() << "# env" << env.toStringList();
    p.setProcessEnvironment(env);
    qDebug() << "#  p.processEnvironment():" << p.processEnvironment().toStringList();

    p.setProcessChannelMode(QProcess::ForwardedOutputChannel); // Forward stdout onto the main process
    qDebug() << "# program:" << p.program();
    qDebug() << "# args:" << args;

    // If user launches an application bundle that has existing windows, bring those to the front
    // instead of launching a second instance.
    // FIXME: Currently we are doing this only if launch has been invoked without parameters
    // for the application to be launched, so as to not break opening documents with applications;
    // we can only hope that such applications are smart enough to open the supplied document
    // using an already-running process instance. This is clumsy; how to do it better?
    // TODO: Remove Menu special case here as soon as we can bring up its Search box with D-Bus
    if(args.length() < 1 && env.contains("LAUNCHED_BUNDLE") && (firstArg != "Menu")) {
        qDebug() << "# Checking for existing windows";
        const auto &windows = KWindowSystem::windows();
        ApplicationInfo *ai = new ApplicationInfo();
        bool foundExistingWindow = false;
        for (WId wid : windows) {
            QString runningBundle = ai->bundlePathForWId(wid);
            if(runningBundle == env.value("LAUNCHED_BUNDLE")) {
                foundExistingWindow = true;
                KWindowSystem::forceActiveWindow(wid);
            }
        }
        if(foundExistingWindow) {
            qDebug() << "# Activated existing windows instead of launching a new instance";

            // We can't exit immediately or else te windows won't become active; FIXME: Do this properly
            QTime dieTime= QTime::currentTime().addSecs(1);
            while (QTime::currentTime() < dieTime)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

            exit(0);
        } else {
            qDebug() << "# Did not find existing windows for LAUNCHED_BUNDLE" << env.value("LAUNCHED_BUNDLE");
        }
        ai->~ApplicationInfo();
    } else if(args.length() > 1) {
        qDebug() << "# Not checking for existing windows because arguments were passed to the application";
    } else {
        qDebug() << "# Not checking for existing windows";
    }

    // Start new process
    p.start();

    // Tell Menu that an application is being launched
    ApplicationInfo *ai = new ApplicationInfo();
    QString bPath = ai->bundlePath(p.program());
    ai->~ApplicationInfo();

    // Blocks until process has started
    if (!p.waitForStarted()) {
        QMessageBox::warning(nullptr, "", "Cannot launch\n" + firstArg );
        exit(1);
    }

    if(env.value("LAUNCHED_BUNDLE") != ""){
        QString stringToBeDisplayed = QFileInfo(env.value("LAUNCHED_BUNDLE")).completeBaseName();
        // For desktop files, we need to parse them...
        if(env.value("LAUNCHED_BUNDLE").endsWith(".desktop")) {
            QSettings desktopFile(env.value("LAUNCHED_BUNDLE"), QSettings::IniFormat);
            stringToBeDisplayed = desktopFile.value("Desktop Entry/Name").toString();
        }

        if (QDBusConnection::sessionBus().isConnected()) {
            QDBusInterface iface("local.Menu", "/", "", QDBusConnection::sessionBus());
            if (! iface.isValid()) {
                qDebug() << "D-Bus interface not valid";
            } else {
                QDBusReply<QString> reply = iface.call("showApplicationName", stringToBeDisplayed);
                if (! reply.isValid()) {
                    qDebug() << "D-Bus reply not valid";
                } else {
                    qDebug() << QString("D-Bus reply: %1\n").arg(reply.value());
                }
            }
        }
    }

    p.waitForFinished(3 * 1000); // Blocks until process has finished or timeout occured (x seconds)
    // Errors occuring thereafter will not be reported to the user in a message box anymore.
    // This should cover most errors like missing libraries, missing interpreters, etc.

    if (p.state() == 0 and p.exitCode() != 0) {
        qDebug("Process is not running anymore and exit code was not 0");
        QString error = p.readAllStandardError();
        if (error.isEmpty()) {
            error = QString("%1 exited unexpectedly\nwith exit code %2").arg(nameWithoutSuffix).arg(p.exitCode());
        }

        qDebug() << error;
        handleError(&p, error);

        // Tell Menu that an application is no more being launched
        if (QDBusConnection::sessionBus().isConnected()) {
            QDBusInterface iface("local.Menu", "/", "", QDBusConnection::sessionBus());
            if (! iface.isValid()) {
                qDebug() << "D-Bus interface not valid";
            } else {
                QDBusReply<QString> reply = iface.call("hideApplicationName");
                if (! reply.isValid()) {
                    qDebug() << "D-Bus reply not valid";
                } else {
                    qDebug() << QString("D-Bus reply: %1\n").arg(reply.value());
                }
            }
        }

        exit(p.exitCode());
    }

    // When we have made it all the way to here, add our application to the launch.db
    // TODO: Similarly, when we are trying to launch the bundle but it is not there anymore, then remove it from the launch.db
    if(env.contains("LAUNCHED_BUNDLE")) {
        db->handleApplication(env.value("LAUNCHED_BUNDLE"));
    }

    // Crude workaround for https://github.com/helloSystem/launch/issues/4
    // FIXME: Find a way to p.detach(); and return(0); without crashing the payload application
    // when it writes to stderr
    db->~DbManager();
    p.waitForFinished(-1);

    return(0);
}


int open(const QStringList args)
{
    QString firstArg = args.first();
    qDebug() << "open firstArg:" << firstArg;

    if (! db->isOpen()) {
        qCritical() << "Database is not open!";
        return 1;
    }

    QString appToBeLaunched = nullptr;
    QStringList removalCandidates = {}; // For applications that possibly don't exist on disk anymore

    if((! QFileInfo::exists(firstArg)) && (! firstArg.contains(":/"))) {
        QMessageBox::warning(nullptr, " ", "Cannot find\n" + firstArg );
        exit(1);
    }

    // Check whether the file to be opened specifies an application it wants to be opened with
    bool ok = false;
    QString openWith = Fm::getAttributeValueQString(firstArg, "open-with", ok);
    if(ok) {
        // NOTE: For security reasons, the application must be known to the system
        // so that totally random commands won't get executed.
        // Currently open-with needs to contain an absolute path
        // contained in launch.db. This could
        // possibly be made more sophisticated by allowing the open-with
        // value to any kind of string that 'launch' knows to open;
        // to be decided. Behavior might change in the future.
        if(db->applicationExists(openWith)) {
            appToBeLaunched = openWith;
        }
    }

    QString mimeType;
    if (appToBeLaunched.isNull()) {
        // Get MIME type of file to be opened
        mimeType = QMimeDatabase().mimeTypeForFile(firstArg).name();
        qDebug() << "File to be opened has MIME type:" << mimeType;

        // Stop stealing applications (like code-oss) from claiming folders
        if(mimeType.startsWith("inode/")){
            appToBeLaunched = "Filer";
        }

        // Empty files are reported as 'application/x-zerosize' here, but as "inode/x-empty" by 'file'
        // so treat them as empty text files; TODO: Better ideas, anyone?
        if(mimeType == "application/x-zerosize" || mimeType == "inode/x-empty") {
            mimeType = "text/plain";
        }

        // Handle legacy XDG style "file:///..." URIs
        // by converting them to sane "/...". Example: Falkon downloads being double-clicked
        if(firstArg.startsWith("file://")) {
            firstArg = QUrl::fromEncoded(firstArg.toUtf8()).toString().replace("file://", "");
        }

        // Handle legacy XDG style "computer:///LIVE.mount" mount points
        // by converting them to sane "/media/LIVE". TODO: Get rid of them in Filer
        if((firstArg.startsWith("computer://")) && (firstArg.endsWith(".mount"))) {
            appToBeLaunched = "Filer";
            firstArg.replace("computer://", "").replace(".mount", "");
            firstArg = "/media" + firstArg;
        }

        // Do not attempt to open file types which are known to have no useful applications;
        // please let us know if you have better ideas for what to do with those
        QStringList blacklistedMimeTypes = {"application/octet-stream"};
        for(const QString blacklistedMimeType : blacklistedMimeTypes) {
            if((mimeType == blacklistedMimeType) && (! firstArg.contains(":/"))){
                QMessageBox::warning(nullptr, " ", QString("Cannot open %1\nof MIME type '%2'").arg(firstArg, mimeType));
                exit(1);
            }
        }


        if(firstArg.contains(":/")){
            QUrl url = QUrl(firstArg);
            qDebug() << "Protocol" << url.scheme();
            mimeType = "x-scheme-handler/" + url.scheme();
        }

        if (appToBeLaunched.isNull()) {
            QStringList appCandidates;
            QStringList fallbackAppCandidates; // Those where only the first part of the MIME type before the "/" matches
            const QStringList allApps = db->allApplications();
            for (const QString &app : allApps) {

                QStringList canOpens;
                if(db->filesystemSupportsExtattr) {
                    bool ok = false;
                    canOpens = Fm::getAttributeValueQString(app, "can-open", ok).split(";");
                    if(! ok) {
                        removalCandidates.append(app);
                        continue;
                    }
                } else {
                    canOpens = db->getCanOpenFromFile(app).split(";");
                }

                for (const QString &canOpen : canOpens) {
                    if (canOpen == mimeType) {
                        qDebug() << app << "can open" << canOpen;
                        appCandidates.append(app);
                    }
                    if (canOpen.split("/").first() == mimeType.split("/").first()) {
                        qDebug() << app << "can open" << canOpen.split("/").first();
                        fallbackAppCandidates.append(app);
                    }
                }

            }

            qDebug() << "appCandidates:" << appCandidates;

            // Falling back to applications where only the first part of the MIME type before the "/" matches;
            // this does not make sense for x-scheme-handler though
            if((appCandidates.length() < 1) && (mimeType.split("/").first() != "x-scheme-handler")) {
                qDebug() << "fallbackAppCandidates:" << fallbackAppCandidates;
                appCandidates = fallbackAppCandidates;
            }

            if(appCandidates.length() < 1) {
                QString fileOrProtocol = QFileInfo(firstArg).fileName();
                if(firstArg.contains(":/")){
                    QUrl url = QUrl(firstArg);
                    fileOrProtocol = url.scheme() + "://";
                }
                QMessageBox::warning(nullptr, " ",
                                     QString("Found no application that can open\n%1\nof type %2" ).arg(fileOrProtocol).arg(mimeType)); // TODO: Show "Open With" dialog?
                return 1;
            } else {
                appToBeLaunched = appCandidates[0];
            }
        }

        // Garbage collect launch.db: Remove applications that are no longer on the filesystem
        for (const QString removalCandidate : removalCandidates) {
            db->handleApplication(removalCandidate);
        }

        // TODO: Prioritize which of the applications that can handle this
        // file should get to open it. For now we ust just the first one we find
        // const QStringList arguments = QStringList({appCandidates[0], path});
        launch({appToBeLaunched, firstArg});
    }
    db->~DbManager();
    return 0;
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

    discoverApplications();

    if(QFileInfo(argv[0]).fileName() == "launch") {
        if(args.isEmpty()){
            qCritical() << "USAGE:" << argv[0] << "<application to be launched> [<arguments>]" ;
            exit(1);
        }
        launch(args);
    }

    if(QFileInfo(argv[0]).fileName().endsWith("open")) {
        if(args.isEmpty()){
            qCritical() << "USAGE:" << argv[0] << "<document to be opened>" ;
            exit(1);
        }
        open(args);
    }

    return 0;

}

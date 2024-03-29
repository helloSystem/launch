#include "launcher.h"
#include "ApplicationSelectionDialog.h"
#include <unistd.h>
#include <QApplication>
#include <NETWM>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "Executable.h"
#include <QMessageBox>

Launcher::Launcher() : db(new DbManager()) { }

Launcher::~Launcher()
{
    db->~DbManager();
}

// If a package needs to be updated, tell the user how to do this,
// or even offer to do it
QString Launcher::getPackageUpdateCommand(QString pathToInstalledFile)
{
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

// Translate cryptic errors into clear text, and possibly even offer buttons to
// take action
void Launcher::handleError(QDetachableProcess *p, QString errorString)
{
    QMessageBox qmesg;

    QFileInfo fi(p->program());
    QString title = fi.completeBaseName(); // https://doc.qt.io/qt-5/qfileinfo.html#completeBaseName

    // Make this error message not appear in the Dock // FIXME: Does not work,
    // why?
    qmesg.setWindowFlag(Qt::SubWindow);

    // If we can't make the icon go away in the Dock, at least make it a
    // non-placeholder icon Not sure if the Dock is already reflecting the
    // WindowIcon correctly
    // FIXME: Does not work, why?
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    qmesg.setWindowIcon(icon);

    QRegExp rx(".*ld-elf.so.1: (.*): version (.*) required by (.*) not found.*");
    QRegExp rxPy(".*ModuleNotFoundError: No module named '(.*)'.*");

    if (errorString.contains("FATAL: kernel too old")) {
        QString cleartextString =
                "The Linux compatibility layer reports an older kernel version than "
                "what is required to run this application.\n\n"
                "Please run\nsudo sysctl compat.linux.osrelease=5.0.0\nand try again.";
        qmesg.warning(nullptr, title, cleartextString);
    } else if (errorString.contains("setuid_sandbox_host.cc")) {
        QString cleartextString = "Cannot run Chromium-based applications with a sandbox.\n"
                                  "Please try running it with the --no-sandbox argument.";
        qmesg.warning(nullptr, title, cleartextString);
    } else if (rx.indexIn(errorString) == 0) {
        QString outdatedLib = rx.cap(1);
        QString versionNeeded = rx.cap(2);
        QFile f(outdatedLib);
        QFileInfo fileInfo(f.fileName());
        QString outdatedLibShort(fileInfo.fileName());
        QString cleartextString =
                QString("%1 application requires at least version %2 of %3 to run.")
                        .arg(title)
                        .arg(versionNeeded)
                        .arg(outdatedLibShort);
        if (getPackageUpdateCommand(outdatedLib) != "") {
            cleartextString.append(QString("\n\nPlease update it with\n%1\nand try again.")
                                           .arg(getPackageUpdateCommand(outdatedLib)));
        } else {
            cleartextString.append(QString("\n\nPlease update it and try again.")
                                           .arg(getPackageUpdateCommand(outdatedLib)));
        }
        qmesg.warning(nullptr, title, cleartextString);
    } else if (rxPy.indexIn(errorString) == 0) {
        QString missingPyModule = rxPy.cap(1);
        QString cleartextString = QString("%1 requires the Python module %2 to "
                                          "run.\n\nPlease install it and try again.")
                                          .arg(title)
                                          .arg(missingPyModule);
        qmesg.warning(nullptr, title, cleartextString);
    } else {

        QStringList lines = errorString.split("\n");

        // Remove all lines from QStringList lines that contain "from LD_PRELOAD
        // cannot be preloaded" or "no version information available as these most
        // likely are not relevant to the user
        for (QStringList::iterator it = lines.begin(); it != lines.end();) {
            if ((*it).contains("from LD_PRELOAD cannot be preloaded")
                || (*it).contains("no version information available (required by")) {
                it = lines.erase(it);
            } else {
                ++it;
            }
        }

        QString cleartextString = lines.join("\n");
        if (lines.length() > 10) {

            QString text = QObject::tr(QString("%1 has quit unexpectedly.\n\n\n").arg(title).toUtf8());
            // Append non-breaking spaces to the text to increase width
            text.append(QString(100, QChar::Nbsp));
            qmesg.setText(text);
            QString informativeText = QObject::tr("Error message:");
            qmesg.setWindowTitle(title);
            qmesg.setDetailedText(cleartextString);
            qmesg.setIcon(QMessageBox::Warning);
            qmesg.setSizeGripEnabled(true);
            qmesg.exec();
        } else {
            qmesg.warning(nullptr, title, cleartextString);
        }

    }
}

// Find apps on well-known paths and put them into launch.db
void Launcher::discoverApplications()
{
    // Measure the time it takes to look up candidates
    QElapsedTimer timer;
    timer.start();
    AppDiscovery *ad = new AppDiscovery(db);
    QStringList wellKnownLocs = ad->wellKnownApplicationLocations();
    ad->findAppsInside(wellKnownLocs);
    // Print to stdout how long it took to discover applications
    qDebug() << "Took" << timer.elapsed()
             << "milliseconds to discover applications and add them to "
                "launch.db, part of which was logging";
    // ad->~AppDiscovery(); // FIXME: Doing this here would lead to a crash; why?
}

QStringList Launcher::executableForBundleOrExecutablePath(QString bundleOrExecutablePath)
{
    QStringList executableAndArgs = {};
    if (QFile::exists(bundleOrExecutablePath)) {
        QFileInfo info = QFileInfo(bundleOrExecutablePath);
        if (bundleOrExecutablePath.endsWith(".AppDir") || bundleOrExecutablePath.endsWith(".app")) {
            qDebug() << "# Found" << bundleOrExecutablePath;
            QString executable_candidate;
            if (bundleOrExecutablePath.endsWith(".AppDir")) {
                executable_candidate = bundleOrExecutablePath + "/AppRun";
            } else {
                // The .app could be a symlink, so we need to determine the
                // nameWithoutSuffix from its target
                QString nameWithoutSuffix = QFileInfo(bundleOrExecutablePath).completeBaseName();
                executable_candidate = bundleOrExecutablePath + "/" + nameWithoutSuffix;
            }
            QFileInfo candinfo = QFileInfo(executable_candidate);
            if (candinfo.isExecutable()) {
                executableAndArgs = QStringList({ executable_candidate });
            }

        } else if (bundleOrExecutablePath.endsWith(".AppImage")
                   || bundleOrExecutablePath.endsWith(".appimage")) {
            qDebug() << "# Found non-executable AppImage" << bundleOrExecutablePath;
            executableAndArgs = QStringList({ bundleOrExecutablePath });
        } else if (bundleOrExecutablePath.endsWith(".desktop")) {
            qDebug() << "# Found .desktop file" << bundleOrExecutablePath;
            QSettings desktopFile(bundleOrExecutablePath, QSettings::IniFormat);
            QString s = desktopFile.value("Desktop Entry/Exec").toString();
            QStringList execStringAndArgs = QProcess::splitCommand(
                    s); // This should hopefully treat quoted strings halfway correctly
            if (execStringAndArgs.first().count(QLatin1Char('\\')) > 0) {
                QMessageBox::warning(nullptr, " ",
                                     "Launching such complex .desktop files is not supported yet.\n"
                                             + bundleOrExecutablePath);
                exit(1);
            } else {
                // Get the first element of the list, which is the executable, and look it up on the $PATH
                QString executable = execStringAndArgs.first();
                if (! executable.contains("/")) {
                    QString executablePath = QStandardPaths::findExecutable(executable);
                    if (executablePath == "") {
                        QMessageBox::warning(nullptr,
                                             QApplication::tr("Executable not found"),
                                             QApplication::tr("Could not find executable %1 on $PATH.\n%2")
                                                 .arg(executable, bundleOrExecutablePath));

                        exit(1);
                    }
                    // Replace the first element of the list with the full path to the executable
                    execStringAndArgs.replace(0, executablePath);
                }
                executableAndArgs = execStringAndArgs;
            }
        } else if (!info.isDir()) {
            if(Executable::hasShebangOrIsElf(bundleOrExecutablePath)) {
                if(info.isExecutable()) {
                    qDebug() << "# Found executable" << bundleOrExecutablePath;
                    executableAndArgs = QStringList({ bundleOrExecutablePath });
                } else {
                    qDebug() << "# Found non-executable" << bundleOrExecutablePath;
                    bool success = Executable::askUserToMakeExecutable(bundleOrExecutablePath);
                    if (!success) {
                        exit(1);
                    } else {
                        executableAndArgs = QStringList({ bundleOrExecutablePath });
                    }
                }
            }
        }
    }
    return executableAndArgs;
}

QString Launcher::pathWithoutBundleSuffix(QString path)
{
    // List of common bundle suffixes to remove
    QStringList bundleSuffixes = { ".AppDir", ".app", ".desktop", ".AppImage", ".appimage" };

    QString cleanedPath = path;
    for (const QString &suffix : bundleSuffixes) {
        cleanedPath = cleanedPath.remove(QRegularExpression(suffix, QRegularExpression::CaseInsensitiveOption));
    }

    return cleanedPath;
}

int Launcher::launch(QStringList args)
{
    QDetachableProcess p;

    QString executable = nullptr;
    QString firstArg = args.first();
    qDebug() << "launch firstArg:" << firstArg;

    QFileInfo fileInfo = QFileInfo(firstArg);
    QString nameWithoutSuffix = QFileInfo(fileInfo.completeBaseName()).fileName();

    // Remove trailing slashes
    while (firstArg.endsWith("/")) {
        firstArg.remove(firstArg.length() - 1, 1);
    }

    args.pop_front();

    // First, try to find something we can launch at the path that was supplied as
    // an argument, Examples:
    //    /Applications/LibreOffice.app
    //    /Applications/LibreOffice.AppDir
    //    /Applications/LibreOffice.AppImage
    //    /Applications/libreoffice

    QStringList e = executableForBundleOrExecutablePath(firstArg);
    if (e.length() > 0) {
        executable = e.first();

        // Non-executable files should be handled by the open command, not the launch command.
        // But just in case, we check whether the file is lacking the executable bit.
        if(Executable::hasShebangOrIsElf(executable)) {
            QFileInfo info = QFileInfo(executable);
            if(! info.isExecutable()) {
                qDebug() << "# Found non-executable" << executable;
                bool success = Executable::askUserToMakeExecutable(executable);
                if (!success) {
                    exit(1);
                }
            }
        }
    }

    // Second, try to find an executable file on the $PATH
    if (executable == nullptr) {
        QString candidate = QStandardPaths::findExecutable(firstArg);
        if (candidate != "") {
            qDebug() << "Found" << candidate << "on the $PATH";
            executable = candidate; // Returns the absolute file path to the
                                    // executable, or an empty string if not found
        }
    }

    // Third, try to find an executable from the applications in launch.db

    QString selectedBundle = "";
    if (executable == nullptr) {

        // Measure the time it takes to look up candidates
        QElapsedTimer timer;
        timer.start();

        // Iterate recursively through locationsContainingApps searching for AppRun
        // files in matchingly named AppDirs

        QFileInfoList candidates;

        QStringList allAppsFromDb = db->allApplications();

        for (QString appBundleCandidate : allAppsFromDb) {
            // Now that we may have collected different candidates, decide on which
            // one to use e.g., the one with the highest self-declared version number.
            // Also we need to check whether the appBundleCandidate exist
            // For now, just use the first one
            if (pathWithoutBundleSuffix(appBundleCandidate).endsWith(firstArg)) {
                if (QFileInfo(appBundleCandidate).exists()) {
                    qDebug() << "Selected from launch.db:" << appBundleCandidate;
                    selectedBundle = appBundleCandidate;
                    break;
                } else {
                    db->handleApplication(appBundleCandidate); // Remove from launch.db it
                                                               // if it does not exist
                }
            }
        }

        // For the selectedBundle, get the launchable executable
        if (selectedBundle == "") {
            QMessageBox::warning(nullptr, " ",
                                 QString("The application '%1'\ncan't be launched "
                                         "because it can't be found.")
                                         .arg(firstArg));
            // Remove the application from launch.db if the symlink points to a non-existing file
            db->handleApplication(firstArg);

            exit(1);
        } else {
            QStringList e = executableForBundleOrExecutablePath(selectedBundle);
            if (e.length() > 0)
                executable = e.first();
        }
    }

    // .desktop files can have arguments in them, and we need to insert the
    // arguments given to launch on the command line into the arguments coming
    // from the desktop file. So we have to construct arguments from the desktop
    // file and from the command line Things like this make XDG overly complex!
    QStringList constructedArgs = {};

    QStringList execLinePartsFromDesktopFile = executableForBundleOrExecutablePath(firstArg);
    if (execLinePartsFromDesktopFile.length() > 1) {
        execLinePartsFromDesktopFile.pop_front();
        for (const QString &execLinePartFromDesktopFile : execLinePartsFromDesktopFile) {
            if (execLinePartFromDesktopFile == "%f" || execLinePartFromDesktopFile == "%u") {
                if (args.length() > 0) {
                    constructedArgs.append(args[0]);
                }
            } else if (execLinePartFromDesktopFile == "%F" || execLinePartFromDesktopFile == "%U") {
                if (args.length() > 0) {
                    constructedArgs.append(args);
                }
            } else {
                constructedArgs.append(execLinePartFromDesktopFile);
            }
        }
        args = constructedArgs;
    }

    // Proceed to launch application
    p.setProgram(executable);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    qDebug() << "# Setting LAUNCHED_EXECUTABLE environment variable to" << executable;
    env.insert("LAUNCHED_EXECUTABLE", executable);
    QFileInfo info = QFileInfo(executable);

    p.setArguments(args);

    // Hint: LAUNCHED_EXECUTABLE and LAUNCHED_BUNDLE environment variables
    // can be gotten from X11 windows on FreeBSD with
    // procstat -e $(xprop | grep PID | cut -d " " -f 3)
    qDebug() << "info.canonicalFilePath():" << info.canonicalFilePath();
    qDebug() << "executable:" << executable;
    env.remove("LAUNCHED_BUNDLE"); // So that nested launches won't leak LAUNCHED_BUNDLE
                                   // from parent to child application; works
    if (info.dir().absolutePath().toLower().endsWith(".appdir")
        || info.dir().absolutePath().toLower().endsWith(".app")) {
        qDebug() << "# Bundle directory (.app, .AppDir)" << info.dir().canonicalPath();
        qDebug() << "# Setting LAUNCHED_BUNDLE environment variable to it";
        env.insert("LAUNCHED_BUNDLE",
                   info.dir().canonicalPath()); // Resolve symlinks so as to show
                                                // the real location
    } else if (fileInfo.canonicalFilePath().toLower().endsWith(".appimage")) {
        qDebug() << "# Bundle file (.AppImage)" << fileInfo.canonicalFilePath();
        qDebug() << "# Setting LAUNCHED_BUNDLE environment variable to it";
        env.insert("LAUNCHED_BUNDLE",
                   fileInfo.canonicalFilePath()); // Resolve symlinks so as to show
                                                  // the real location
    } else if (fileInfo.canonicalFilePath().endsWith(".desktop")) {
        qDebug() << "# Bundle file (.desktop)" << fileInfo.canonicalFilePath();
        qDebug() << "# Setting LAUNCHED_BUNDLE environment variable to it";
        env.insert("LAUNCHED_BUNDLE",
                   fileInfo.canonicalFilePath()); // Resolve symlinks so as to show
                                                  // the real location
    }
    // qDebug() << "# env" << env.toStringList();
    p.setProcessEnvironment(env);
    qDebug() << "#  p.processEnvironment():" << p.processEnvironment().toStringList();

    p.setProcessChannelMode(
            QProcess::ForwardedOutputChannel); // Forward stdout onto the main process
    qDebug() << "# program:" << p.program();
    qDebug() << "# args:" << args;

    // If user launches an application bundle that has existing windows, bring
    // those to the front instead of launching a second instance.
    // FIXME: Currently we are doing this only if launch has been invoked without
    // parameters for the application to be launched, so as to not break opening
    // documents with applications; we can only hope that such applications are
    // smart enough to open the supplied document using an already-running process
    // instance. This is clumsy; how to do it better? We could check whethe the
    // same application we are about to launch has windows open with the name/path
    // of the file about to be opened in _NET_WM_NAME (e.g., FeatherPad) but that
    // does not work reliably for all applictions, e.g.,"launch.html - Falkon"
    // TODO: Remove Menu special case here as soon as we can bring up its Search
    // box with D-Bus
    if (args.length() < 1 && env.contains("LAUNCHED_BUNDLE") && (firstArg != "Menu")) {
        qDebug() << "# Checking for existing windows";
        const auto &windows = KWindowSystem::windows();
        bool foundExistingWindow = false;
        for (WId wid : windows) {

            QString runningBundle = ApplicationInfo::bundlePathForWId(wid);
            if (runningBundle == env.value("LAUNCHED_BUNDLE")) {
                // Check if the user ID which the application is running under is the same user ID as is the current user
                // This is to avoid bringing to the front windows of other users (e.g., if we want to run as root)
                // FIXME: Find a way that works on all platforms and takes ~3 lines of code instead of ~20
                int pid = 0;
                Display *display = XOpenDisplay(nullptr);
                Atom type;
                int format;
                unsigned long nitems, bytes_after;
                unsigned char *prop;
                int status = XGetWindowProperty(display, wid, XInternAtom(display, "_NET_WM_PID", False), 0, 1, False, XA_CARDINAL, &type, &format, &nitems, &bytes_after, &prop);
                if (status == Success && prop) {
                    pid = *(unsigned long *)prop;
                    XFree(prop);
                }
                XCloseDisplay(display);
                qDebug() << "# _NET_WM_PID:" << pid;
                QProcess process;
                process.start("ps", QStringList() << "-p" << QString::number(pid) << "-o" << "user");
                process.waitForFinished(-1);
                QString processOutput = process.readAllStandardOutput();
                if (processOutput.split("\n").at(1) != qgetenv("USER")) {
                    qDebug() << "# Not activating window" << wid << "because it is running under a different user ID";
                    continue;
                }
            
                foundExistingWindow = true;
                KWindowSystem::forceActiveWindow(wid);
            }
        }
        if (foundExistingWindow) {
            qDebug() << "# Activated existing windows instead of launching a new instance";

            // We can't exit immediately or else te windows won't become active;
            // FIXME: Do this properly
            QTime dieTime = QTime::currentTime().addSecs(1);
            while (QTime::currentTime() < dieTime)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

            exit(0);
        } else {
            qDebug() << "# Did not find existing windows for LAUNCHED_BUNDLE"
                     << env.value("LAUNCHED_BUNDLE");
        }
    } else if (args.length() > 1) {
        qDebug() << "# Not checking for existing windows because arguments were "
                    "passed to the application";
    } else {
        qDebug() << "# Not checking for existing windows";
    }

    // Start new process
    p.start();

    // Tell Menu that an application is being launched
    QString bPath = ApplicationInfo::bundlePath(p.program());

    // Blocks until process has started
    if (!p.waitForStarted()) {
        // The reason we ended up here may well be that the file has executable permissions despite
        // it not being an executable file, hence we can't launch it. So we try to open it with its
        // default application
        qDebug() << "# Failed to start process; trying to open it with its default application";
        QStringList completeArgs = { firstArg };
        completeArgs.append(args);
        open(completeArgs);
        exit(0);
    }

    if (env.value("LAUNCHED_BUNDLE") != "") {
        QString stringToBeDisplayed = QFileInfo(env.value("LAUNCHED_BUNDLE")).completeBaseName();
        // For desktop files, we need to parse them...
        if (env.value("LAUNCHED_BUNDLE").endsWith(".desktop")) {
            QSettings desktopFile(env.value("LAUNCHED_BUNDLE"), QSettings::IniFormat);
            stringToBeDisplayed = desktopFile.value("Desktop Entry/Name").toString();
        }

        if (QDBusConnection::sessionBus().isConnected()) {
            QDBusInterface iface("local.Menu", "/", "", QDBusConnection::sessionBus());
            if (!iface.isValid()) {
                qDebug() << "D-Bus interface not valid";
            } else {
                QDBusReply<QString> reply = iface.call("showApplicationName", stringToBeDisplayed);
                if (!reply.isValid()) {
                    qDebug() << "D-Bus reply not valid";
                } else {
                    qDebug() << QString("D-Bus reply: %1\n").arg(reply.value());
                }
            }
        }
    }

    p.waitForFinished(10
                      * 1000); // Blocks until process has finished or timeout occured (x seconds)
    // Errors occuring thereafter will not be reported to the user in a message
    // box anymore. This should cover most errors like missing libraries, missing
    // interpreters, etc.

    if (p.state() == 0 and p.exitCode() != 0) {
        qDebug("Process is not running anymore and exit code was not 0");
        QString error = p.readAllStandardError();
        if (error.isEmpty()) {
            error = QString("%1 exited unexpectedly\nwith exit code %2")
                            .arg(nameWithoutSuffix)
                            .arg(p.exitCode());
        }

        qDebug() << error;
        handleError(&p, error);

        // Tell Menu that an application is no more being launched
        if (QDBusConnection::sessionBus().isConnected()) {
            QDBusInterface iface("local.Menu", "/", "", QDBusConnection::sessionBus());
            if (!iface.isValid()) {
                qDebug() << "D-Bus interface not valid";
            } else {
                QDBusReply<QString> reply = iface.call("hideApplicationName");
                if (!reply.isValid()) {
                    qDebug() << "D-Bus reply not valid";
                } else {
                    qDebug() << QString("D-Bus reply: %1\n").arg(reply.value());
                }
            }
        }

        exit(p.exitCode());
    }

    // When we have made it all the way to here, add our application to the
    // launch.db
    // TODO: Similarly, when we are trying to launch the bundle but it is not
    // there anymore, then remove it from the launch.db
    if (env.contains("LAUNCHED_BUNDLE")) {
        db->handleApplication(env.value("LAUNCHED_BUNDLE"));
    }

    db->~DbManager();

    p.waitForFinished(-1);

    // Is this a way to p.detach(); and return(0)
    // without crashing the payload application
    // when it writes to stderr?
    // https://github.com/helloSystem/launch/issues/4
    // TODO: Make these picked up by a crash reporter; maybe name them with the
    // pid? p.setStandardErrorFile("/tmp/launch_error");
    // p.setStandardOutputFile("/tmp/launch_output");
    // p.setInputChannelMode(QProcess::ManagedInputChannel); // "Unforward"

    // NOTE: 'daemon launch ...' will lead to '<defunct>' in 'ps ax' output
    // after the following runs; does this have any negative impact other than
    // cosmetics?
    // p.detach();
    return (0);
}

int Launcher::open(QStringList args)
{

    bool showChooserRequested = false;
    if (args.first() == "--chooser") {
        args.pop_front();
        showChooserRequested = true;
    }

    QString firstArg = args.first();
    qDebug() << "open firstArg:" << firstArg;

    // Workaround for FreeBSD not being able to properly mount all AppImages
    // by using the "runappimage" helper that mounts the AppImage and then
    // executes its payload.
    // FIXME: This should go away as soon as possible.

    if (QSysInfo::kernelType() == "freebsd") {
        // Check if we have the "runappimage" helper
        QString runappimage = QStandardPaths::findExecutable("runappimage");
        qDebug() << "runappimage:" << runappimage;
        if (! runappimage.isEmpty()) {
            if (firstArg.toLower().endsWith(".appimage")) {
                QFileInfo info = QFileInfo(firstArg);
                if (!info.isExecutable()) {
                    args.insert(0, runappimage);
                    firstArg = args.first();
                }
            }
        }
    }

    QString appToBeLaunched = nullptr;
    QStringList
            removalCandidates = {}; // For applications that possibly don't exist on disk anymore

    // NOTE: magnet:?xt=urn:btih:... URLs do not contain ":/"
    if (!showChooserRequested && (!QFileInfo::exists(firstArg)) && (!firstArg.contains(":/"))
        && (!firstArg.contains(":?"))) {
        if (QFileInfo(firstArg).isSymLink()) {
            // Broken symlink
            // TODO: Offer to delete or fix broken symlinks
            QMessageBox::warning(nullptr, " ",
                                 QString("The symlink '%1'\ncan't be opened "
                                         "because\nthe target '%2'\ncan't be found.")
                                         .arg(firstArg)
                                         .arg(QFileInfo(firstArg).symLinkTarget()));
        } else {
            // File not found
            QMessageBox::warning(
                    nullptr, " ",
                    QString("'%1'\ncan't be opened because it can't be found.").arg(firstArg));
        }
        exit(1);
    }

    // Check whether the file to be opened is an ELF executable or a script missing the executable bit
    if(!showChooserRequested && Executable::hasShebangOrIsElf(firstArg)) {
        QStringList executableAndArgs;
        QFileInfo info = QFileInfo(firstArg);
        if(info.isExecutable()) {
            qDebug() << "# Found executable" << firstArg;
            exit(launch(args));
        } else {
            qDebug() << "# Found non-executable" << firstArg;
            bool success = Executable::askUserToMakeExecutable(firstArg);
            if (!success) {
                exit(1);
            } else {
                exit(launch(args));
            }
        }
    }

    // Check whether the file to be opened specifies an application it wants to be
    // opened with
    bool ok = false;
    QString openWith = Fm::getAttributeValueQString(firstArg, "open-with", ok);
    if (ok && !showChooserRequested) {
        // NOTE: For security reasons, the application must be known to the system
        // so that totally random commands won't get executed.
        // This could
        // possibly be made more sophisticated by allowing the open-with
        // value to any kind of string that 'launch' knows to open;
        // to be decided. Behavior might change in the future.
        if (db->applicationExists(openWith)) {
            appToBeLaunched = openWith;
        }
    }

    QString mimeType;
    if (appToBeLaunched.isNull()) {
        // Get MIME type of file to be opened
        mimeType = QMimeDatabase().mimeTypeForFile(firstArg).name();

        // Handle legacy XDG style "file:///..." URIs
        // by converting them to sane "/...". Example: Falkon downloads being
        // double-clicked
        if (firstArg.startsWith("file://")) {
            firstArg = QUrl::fromEncoded(firstArg.toUtf8()).toLocalFile();
        }

        // Handle legacy XDG style "computer:///LIVE.mount" mount points
        // by converting them to sane "/media/LIVE". For legacy compatibility
        // reasons only
        if ((firstArg.startsWith("computer://")) && (firstArg.endsWith(".mount"))) {
            appToBeLaunched = "Filer";
            firstArg.replace("computer://", "").replace(".mount", "");
            firstArg = "/media" + firstArg;
        }

        // Do this AFTER the special cases like "file://" and "computer://" have
        // already been handled
        // NOTE: magnet:?xt=urn:btih:... URLs do not contain ":/"
        if (firstArg.contains(":/") || firstArg.contains(":?")) {
            QUrl url = QUrl(firstArg);
            qDebug() << "Protocol" << url.scheme();
            mimeType = "x-scheme-handler/" + url.scheme();
        }

        // Stop stealing applications (like code-oss) from claiming folders
        if (mimeType.startsWith("inode/")) {
            appToBeLaunched = "Filer";
        }

        // Empty files are reported as 'application/x-zerosize' here, but as
        // "inode/x-empty" by 'file' so treat them as empty text files; TODO: Better
        // ideas, anyone?
        if (mimeType == "application/x-zerosize" || mimeType == "inode/x-empty") {
            mimeType = "text/plain";
        }

        qDebug() << "File to be opened has MIME type:" << mimeType;

        // Do not attempt to open file types which are known to have no useful
        // applications; please let us know if you have better ideas for what to do
        // with those
        QStringList blacklistedMimeTypes = { "application/octet-stream" };
        for (const QString blacklistedMimeType : blacklistedMimeTypes) {
            if ((mimeType == blacklistedMimeType) && (!firstArg.contains(":/"))) {
                QMessageBox::warning(
                        nullptr, " ",
                        QString("Cannot open %1\nof MIME type '%2'.").arg(firstArg, mimeType));
                exit(1);
            }
        }

        // Do not open .desktop files; instead, launch them
        if (mimeType == "application/x-desktop") {
            QStringList argsForLaunch = { firstArg };
            argsForLaunch.append(args);
            return launch(argsForLaunch);
        }

        // Check whether there is a symlink in ~/.local/share/launch/MIME/<...>/Default
        // pointing to an application that exists on disk; if yes, then use that
        if (!showChooserRequested) {
            QString mimePath = QString("%1/%2")
                                       .arg(db->localShareLaunchMimePath)
                                       .arg(mimeType.replace("/", "_"));
            QString defaultPath = QString("%1/Default").arg(mimePath);
            if (QFileInfo::exists(defaultPath)) {
                QString defaultApp = QFileInfo(defaultPath).symLinkTarget();
                if (QFileInfo::exists(defaultApp)) {
                    appToBeLaunched = defaultApp;
                } else {
                    // The symlink is broken
                    removalCandidates.append(defaultApp);
                }
            }
            // If there is no Default symlink, check how many symlinks there are in
            // the directory and if there is only one, use that
            else {
                QDirIterator it(mimePath, QDir::Files | QDir::NoDotAndDotDot);
                int count = 0;
                while (it.hasNext()) {
                    it.next();
                    count++;
                    if (count > 1)
                        break;
                }
                if (count == 1) {
                    // Check whether the only file in the directory is a symlink and whether it
                    // points to an application that exists on disk
                    QString onlyFile = it.filePath();
                    if (QFileInfo(onlyFile).isSymLink()) {
                        QString onlyApp = QFileInfo(onlyFile).symLinkTarget();
                        if (QFileInfo::exists(onlyApp)) {
                            appToBeLaunched = onlyApp;
                        } else {
                            // The symlink is broken
                            removalCandidates.append(onlyApp);
                        }
                    }
                }
            }
        }

        if (appToBeLaunched.isNull()) {
            QStringList appCandidates;
            QStringList fallbackAppCandidates; // Those where only the first part of
                                               // the MIME type before the "/" matches
            const QStringList allApps = db->allApplications();
            for (const QString &app : allApps) {

                QStringList canOpens;
                if (db->filesystemSupportsExtattr) {
                    bool ok = false;
                    canOpens = Fm::getAttributeValueQString(app, "can-open", ok).split(";");
                    if (!ok) {
                        if (!removalCandidates.contains(app))
                            removalCandidates.append(app);
                        continue;
                    }
                } else {
                    canOpens = db->getCanOpenFromFile(app).split(";");
                }

                for (const QString &canOpen : canOpens) {
                    if (canOpen == mimeType) {
                        qDebug() << app << "can open" << canOpen;
                        if (!appCandidates.contains(app))
                            appCandidates.append(app);
                    }
                    if (canOpen.split("/").first() == mimeType.split("/").first()) {
                        qDebug() << app << "can open" << canOpen.split("/").first();
                        if (!fallbackAppCandidates.contains(app))
                            fallbackAppCandidates.append(app);
                    }
                }
            }

            qDebug() << "appCandidates:" << appCandidates;

            // Falling back to applications where only the first part of the MIME type
            // before the "/" matches; this does not make sense for x-scheme-handler
            // though
            if ((appCandidates.length() < 1)
                && (mimeType.split("/").first() != "x-scheme-handler")) {
                qDebug() << "fallbackAppCandidates:" << fallbackAppCandidates;
                appCandidates = fallbackAppCandidates;
            }

            QString fileOrProtocol = QFileInfo(firstArg).canonicalFilePath();
            if (firstArg.contains(":/")) {
                fileOrProtocol = firstArg;
            }

            if (showChooserRequested || appCandidates.length() < 1) {
                ApplicationSelectionDialog *dlg =
                        new ApplicationSelectionDialog(&fileOrProtocol, &mimeType, true, false, nullptr);
                auto result = dlg->exec();
                if (result == QDialog::Accepted)
                    appToBeLaunched = dlg->getSelectedApplication();
                else
                    exit(0);
            } else {
                appToBeLaunched = appCandidates[0];
            }
            qDebug() << "appToBeLaunched" << appToBeLaunched;
        }
    }
    // Garbage collect launch.db: Remove applications that are no longer on the
    // filesystem
    for (const QString removalCandidate : removalCandidates) {
        db->handleApplication(removalCandidate);
    }

    // TODO: Prioritize which of the applications that can handle this
    // file should get to open it. For now we ust just the first one we find
    // const QStringList arguments = QStringList({appCandidates[0], path});
    return launch({ appToBeLaunched, firstArg });
}
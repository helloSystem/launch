#include <QApplication>
#include <QProcess>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDirIterator>
#include <QCommandLineParser>

/*

Similar to:
https://github.com/probonopd/appwrapper
and:
GNUstep openapp

TODO: Make the behavior resemble:

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

int main(int argc, char *argv[])
{

    // Launch an application but initially watch for errors and display a Qt error message
    // if needed. After some timeout, detach the process of the application, and exit this helper

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Launch an application and show a graphical error dialog in case the launch did not succeed");
    parser.addHelpOption();

    parser.addPositionalArgument("application", QCoreApplication::translate("main", "Application to be launched. Can be an absolute or relative path to an executable, the name of an executable on the $PATH, or the name of an .AppDir"));

    parser.process(app);

    QStringList args = app.arguments();

    args.pop_front();

    if(args.isEmpty()){
        qDebug() << "No arguments were supplied";
        exit(1);
    }

    QDetachableProcess p;

    // Check whether the first argument exists or is on the $PATH

    // First, try to find an executable file at the path in the first argument
    QString executable = nullptr;
    QString firstArg = args.first();
    if (QFile::exists(firstArg)){
        QFileInfo info = QFileInfo(firstArg);
        if (info.isExecutable()){
            qDebug() << "Found" << args.first();
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
        QStringList locationsContainingApps = { "/Applications", "/System", "/Library",
                                               "/usr/local/GNUstep/Local/Applications",
                                               "/usr/local/GNUstep/System/Applications" // TODO: Find a better and more complete way to specify these
                                              };
        // TODO: Also search for executables in the other types of supported files, e.g.,
        // QStandardPaths::ApplicationsLocation = "~/.local/share/applications", "/usr/local/share/applications", "/usr/share/applications"
        // Iterate recursively through locationsContainingApps searching for AppRun files in matchingly named AppDirs
        QFileInfoList candidates;
        foreach (QString directory, locationsContainingApps) {
            QDirIterator it(directory, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString filename = it.next();
                QFileInfo file(filename);
                if (file.isDir()) {
                    continue;
                }
                if (file.fileName() == "AppRun") {
                    // .AppDir
                    QString AppDirPath = QFileInfo(file).dir().path();
                    // Note: baseName() returns the name without the leading path and without suffixes
                    if (QFileInfo(AppDirPath).baseName() == firstArg.replace(".AppDir", "")) {
                        qDebug() << "Found" << file;
                        candidates.append(file);
                    }
                } else if (file.fileName() == firstArg.replace(".app", "")) {
                    // .app bundle
                    QString AppBundlePath = QFileInfo(file).dir().path();
                    // Note: baseName() returns the name without the leading path and without suffixes
                    if (QFileInfo(AppBundlePath).baseName() == firstArg.replace(".app", "")) {
                        qDebug() << "Found" << file;
                        candidates.append(file);
                    }
                }
                else if (file.fileName() == firstArg + ".desktop") {
                    // .desktop file
                    qDebug() << "Found" << file.fileName() << "TODO: Parse it for Exec=";
                }
            }
        }
        foreach (QFileInfo candidate, candidates) {
            // Now that we may have collected different candidates, decide on which one to use
            // e.g., the one with the highest self-declared version number. Again, a database might come in handy here
            // For now, just use the first one
            qDebug() << "Candidate:" << candidate.absoluteFilePath();
            executable = candidate.absoluteFilePath();
            break;
        }
    }

    p.setProgram(executable);
    args.pop_front();
    p.setArguments(args);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // env.insert("QT_SCALE_FACTOR", "2");
    p.setProcessEnvironment(env);

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
            QMessageBox::warning( nullptr, p.program(), error );
            return(p.exitCode());
        }
    } else {
        p.detach();
        return(0);
    }

    return(1);
}

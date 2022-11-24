#include <QApplication>

#include "launcher.h"

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


int main(int argc, char *argv[])
{

    QApplication app(argc, argv);

    Launcher *launcher = new Launcher();

    // Setting a busy cursor in this way seems only to affect the own application's windows
    // rather than the full screen, which is why it is not suitable for this tool
    // QApplication::setOverrideCursor(Qt::WaitCursor);

    // Launch an application but initially watch for errors and display a Qt error message
    // if needed. After some timeout, detach the process of the application, and exit this helper

    QStringList args = app.arguments();

    args.pop_front();

    launcher->discoverApplications();

    if(QFileInfo(argv[0]).fileName() == "launch") {
        if(args.isEmpty()){
            qCritical() << "USAGE:" << argv[0] << "<application to be launched> [<arguments>]" ;
            exit(1);
        }
        return launcher->launch(args);
    }

    if(QFileInfo(argv[0]).fileName().endsWith("open")) {
        if(args.isEmpty()){
            qCritical() << "USAGE:" << argv[0] << "<document to be opened>" ;
            exit(1);
        }
        return launcher->open(args);
    }

    return 1;

}

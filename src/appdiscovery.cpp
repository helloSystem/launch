#include "appdiscovery.h"

#include <QDir>
#include <QStringList>
#include <QDebug>
#include <QStandardPaths>

#include "dbmanager.h"

AppDiscovery::AppDiscovery()
{
    DbManager *db = new DbManager();
}


AppDiscovery::~AppDiscovery()
{
    db->~DbManager();
}

QStringList AppDiscovery::wellKnownApplicationLocations()
{
    QStringList wellKnownApplicationLocations = {};

    // Add some location in $HOME
    wellKnownApplicationLocations.append(QDir::homePath() + "/Applications");
    wellKnownApplicationLocations.append(QDir::homePath() + "/bin");
    wellKnownApplicationLocations.append(QDir::homePath() + "/.bin");

    // Add system-wide locations
    // TODO: Find a better and more complete way to specify the GNUstep ones
    wellKnownApplicationLocations.append({"/Applications", "/System", "/Library",
                                          "/usr/local/GNUstep/Local/Applications",
                                          "/usr/local/GNUstep/System/Applications",
                                          "/usr/GNUstep/Local/Applications",
                                          "/usr/GNUstep/System/Applications"
                                         });

    // Add legacy locations for XDG compatibility
    // On FreeBSD: "/home/user/.local/share/applications", "/usr/local/share/applications", "/usr/share/applications"
    wellKnownApplicationLocations.append(QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation));

    wellKnownApplicationLocations.removeDuplicates();

    return wellKnownApplicationLocations;
}

void AppDiscovery::findAppsInside(QStringList locationsContainingApps)
// probono: Check locationsContainingApps for applications and add them to the m_systemMenu.
// TODO: Nested submenus rather than flat ones with '→'
// This code is similar to the code in the 'launch' command
{
    QStringList nameFilter({"*.app", "*.AppDir", "*.desktop", "*.AppImage", "*.appimage"});
    foreach (QString directory, locationsContainingApps) {
        // Shall we process this directory? Only if it contains at least one application, to optimize for speed
        // by not descending into directory trees that do not contain any applications at all. Can make
        // a big difference.

        QDir dir(directory);
        int numberOfAppsInDirectory = dir.entryList(nameFilter).length();

        if(directory.endsWith(".app") == false && directory.endsWith(".AppDir") == false && numberOfAppsInDirectory > 0) {
        } else {
            continue;
        }

        // Use QDir::entryList() insted of QDirIterator because it supports sorting
        QStringList candidates = dir.entryList();
        for(QString candidate : candidates ) {
            candidate = dir.path() + "/" + candidate;
            // Do not show Autostart directories (or should we?)
            if (candidate.endsWith("/Autostart") == true) {
                continue;
            }
            // qDebug() << "probono: Processing" << candidate;

            if (candidate.endsWith(".app")){
                db->handleApplication(candidate);
            }
            else if (candidate.endsWith(".AppDir")) {
                db->handleApplication(candidate);
            }
            else if (candidate.endsWith(".desktop")) {
                db->handleApplication(candidate);
            }
            else if (candidate.endsWith(".AppImage") || candidate.endsWith(".appimage")) {
                db->handleApplication(candidate);
            }
            else if (locationsContainingApps.contains(candidate) == false && QFileInfo(candidate).isDir() && candidate.endsWith("/..") == false && candidate.endsWith("/.") == false && candidate.endsWith(".app") == false && candidate.endsWith(".AppDir") == false) {
                // qDebug() << "# Found" << file.fileName() << ", a directory that is not an .app bundle nor an .AppDir";
                QStringList locationsToBeChecked({candidate});
                findAppsInside(locationsToBeChecked);
            }
        }
    }
}

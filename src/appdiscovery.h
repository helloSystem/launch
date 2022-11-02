#ifndef APPDISCOVERY_H
#define APPDISCOVERY_H

#include <QStringList>

#include "dbmanager.h"


class AppDiscovery
{
public:
    AppDiscovery();
    ~AppDiscovery();
    QStringList wellKnownApplicationLocations();
    void findAppsInside(QStringList locationsContainingApps);

private:
    DbManager *db;
};

#endif // APPDISCOVERY_H

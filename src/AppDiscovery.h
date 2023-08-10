#ifndef APPDISCOVERY_H
#define APPDISCOVERY_H

#include <QStringList>

#include "DbManager.h"

/**
 * @file AppDiscovery.h
 * @class AppDiscovery
 * @brief A class for discovering and handling application locations.
 *
 * This class is responsible for discovering well-known application locations and
 * finding applications within those locations.
 */
class AppDiscovery
{
public:
    /**
     * Constructor.
     *
     * @param db A pointer to the DbManager instance for database handling.
     */
    AppDiscovery(DbManager *db);

    /**
     * Destructor.
     */
    ~AppDiscovery();

    /**
     * Retrieve a list of well-known application locations.
     *
     * @return A QStringList containing well-known application locations.
     */
    QStringList wellKnownApplicationLocations();

    /**
     * Find and process applications within specified locations.
     *
     * This function searches for applications within the provided locations and
     * handles each discovered application using the associated DbManager instance.
     *
     * @param locationsContainingApps A list of locations to search for applications.
     */
    void findAppsInside(QStringList locationsContainingApps);

private:
    DbManager *dbman; /**< A pointer to the DbManager instance. */
};

#endif // APPDISCOVERY_H

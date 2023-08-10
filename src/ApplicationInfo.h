#ifndef APPLICATIONINFO_H
#define APPLICATIONINFO_H

#include <QString>

/*
 * https://en.wikipedia.org/wiki/Rule_of_three_(computer_programming)
 * Currently being used in:
 * Menu (master)
 * launch (copy)
 */

class ApplicationInfo
{
public:
    /**
     * Constructor.
     *
     * Creates an instance of the ApplicationInfo class.
     */
    explicit ApplicationInfo();

    /**
     * Destructor.
     *
     * Cleans up resources associated with the ApplicationInfo instance.
     */
    ~ApplicationInfo();

    /**
     * Get the most nested bundle path of a file.
     *
     * This function returns the name of the most nested bundle a file is in,
     * or an empty string if the file is not in a bundle.
     *
     * @param path The path of the file to check.
     * @return The bundle path or an empty string.
     */
    static QString bundlePath(const QString &path);

    /**
     * Get a human-readable application name for a given path.
     *
     * This function returns a nice name for the application based on its path.
     *
     * @param path The path of the application.
     * @return The application nice name.
     */
    static QString applicationNiceNameForPath(const QString &path);

    /**
     * Get the bundle path for a given process ID.
     *
     * This function returns the bundle path for a process ID, based on the
     * LAUNCHED_BUNDLE environment variable set by the 'launch' command.
     *
     * @param pid The process ID.
     * @return The bundle path.
     */
    static QString bundlePathForPId(unsigned int pid);

    /**
     * Get the bundle path for a given window ID.
     *
     * This function returns the bundle path associated with a window ID.
     *
     * @param id The window ID.
     * @return The bundle path.
     */
    static QString bundlePathForWId(unsigned long long id);

    /**
     * Get the path for a given window ID.
     *
     * This function returns the path associated with a window ID.
     *
     * @param id The window ID.
     * @return The path.
     */
    static QString pathForWId(unsigned long long id);

    /**
     * Get a human-readable application name for a given window ID.
     *
     * This function returns a nice name for the application associated with
     * a window ID.
     *
     * @param id The window ID.
     * @return The application nice name.
     */
    static QString applicationNiceNameForWId(unsigned long long id);
};

#endif // APPLICATIONINFO_H

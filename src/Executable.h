#ifndef EXECUTABLE_H
#define EXECUTABLE_H

#include <QString>
#include <QObject>

/**
 * @file Executable.h
 * @class Executable
 * @brief A class to provide utility methods related to ELF executables and interpreted scripts.
 */
class Executable : public QObject {
    Q_OBJECT
public:
    /**
     * Check if a file is executable.
     *
     * @param path The path to the file.
     * @return True if the file is executable, false otherwise.
     */
    static bool isExecutable(const QString& path);

    /**
     * Check if a file has a shebang line.
     *
     * @param path The path to the file.
     * @return True if the file has a shebang, false otherwise.
     */
    static bool hasShebang(const QString& path);

    /**
     * Check if a file has a shebang line or is an ELF executable.
     *
     * @param path The path to the file.
     * @return True if the file has a shebang or is an ELF, false otherwise.
     */
    static bool hasShebangOrIsElf(const QString& path);

    /**
     * Check if a file is an ELF executable.
     *
     * @param path The path to the file.
     * @return True if the file is an ELF, false otherwise.
     */
    static bool isElf(const QString& path);

    /**
     * @brief Ask the user if they want to make a file executable and perform the action if requested.
     *
     * This method displays a dialog asking the user if they want to make the specified file executable.
     * If the user agrees, the file's permissions are modified accordingly.
     *
     * @param path The path to the file.
     * @return True if the file is now executable or already executable, false otherwise.
     *         If an error occurs during permission modification, false is returned as well.
     */
    static bool askUserToMakeExecutable(const QString& path);
};

#endif // EXECUTABLE_H

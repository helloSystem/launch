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
    explicit ApplicationInfo();
    ~ApplicationInfo();
    QString bundlePath(QString path);
    QString bundleName(unsigned long long id);
    QString applicationNiceNameForPath(QString path);
    QString bundlePathForPId(unsigned int pid);
    QString bundlePathForWId(unsigned long long id);
    QString pathForWId(unsigned long long id);
    QString applicationNiceNameForWId(unsigned long long id);


};

#endif // APPLICATIONINFO_H

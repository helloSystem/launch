#ifndef FINDER_H
#define FINDER_H

#include <QFileInfo>

class Finder 
{
public:
    QFileInfoList findAppsInside(QStringList locationsContainingApps, QFileInfoList candidates, QString firstArg);
    QString getExecutable(QString &firstArg);
};

#endif

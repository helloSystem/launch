// File added by probono

#ifndef EXTATTRS_H
#define EXTATTRS_H

#include <QString>

namespace Fm {
int getAttributeValueInt(const QString &path, const QString &attribute, bool &ok);
bool setAttributeValueInt(const QString &path, const QString &attribute, int value);
bool removeAttribute(const QString &path, const QString &attribute);
QString getAttributeValueQString(const QString &path, const QString &attribute, bool &ok);
bool setAttributeValueQString(const QString &path, const QString &attribute, const QString &value);
bool hasAttribute(const QString &path, const QString &attribute);
} // namespace Fm

#endif // EXTATTRS_H

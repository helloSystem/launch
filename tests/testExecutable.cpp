#include <QCoreApplication>
#include <QtTest>

#include "Executable.h"

class TestExecutable : public QObject {
    Q_OBJECT

private slots:
    void testIsExecutable() {
        QVERIFY(Executable::isExecutable("/usr/bin/env"));
        QVERIFY(!Executable::isExecutable("/etc/os-release"));
    }

    void testHasShebang() {
        QVERIFY(Executable::hasShebang("/usr/bin/bg"));
        QVERIFY(!Executable::hasShebang("/etc/os-release"));
    }

    void testHasShebangOrIsElf() {
        QVERIFY(Executable::hasShebangOrIsElf("/usr/bin/bg"));
        QVERIFY(Executable::hasShebangOrIsElf("/usr/bin/env"));
        QVERIFY(!Executable::hasShebangOrIsElf("/etc/os-release"));
    }

 };

QTEST_APPLESS_MAIN(TestExecutable)

#include "testExecutable.moc"

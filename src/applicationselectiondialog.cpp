#include "applicationselectiondialog.h"
#include "ui_applicationselectiondialog.h"
#include <QDebug>

#include "extattrs.h"
#include <QDir>
#include <QMessageBox>

ApplicationSelectionDialog::ApplicationSelectionDialog(QString *fileOrProtocol, QString *mimeType,
                                                       bool showAlsoLegacyCandidates,
                                                       QWidget *parent)
    : QDialog(parent), ui(new Ui::ApplicationSelectionDialog)
{

    this->fileOrProtocol = fileOrProtocol;
    this->mimeType = mimeType;
    this->showAlsoLegacyCandidates = showAlsoLegacyCandidates;

    QString selectedApplication;

    ui->setupUi(this);
    ui->label->setText(QString("Please choose an application to open \n'%1'\nof type '%2':")
                               .arg(*fileOrProtocol)
                               .arg(*mimeType).replace("_", "/"));

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->listWidget, &QListWidget::doubleClicked, this, &QDialog::accept);

    this->setWindowTitle(tr("Open With"));

    ui->checkBoxAlwaysOpenThis->setEnabled(false);
    ui->checkBoxAlwaysOpenAll->setEnabled(false);

    QStringList *appCandidates;
    DbManager *db;

    // When selection changes and something is selected, enable the checkboxes
    // For URL scheme handlers, setting the default application "for this file" is not possible
    connect(ui->listWidget, &QListWidget::itemSelectionChanged, [=]() {
        if (ui->listWidget->selectedItems().length() > 0) {
            if (!mimeType->startsWith("x-scheme-handler"))
                ui->checkBoxAlwaysOpenThis->setEnabled(true);
            ui->checkBoxAlwaysOpenAll->setEnabled(true);
        } else {
            ui->checkBoxAlwaysOpenThis->setEnabled(false);
            ui->checkBoxAlwaysOpenAll->setEnabled(false);
        }
    });

    // Construct the path to the MIME type in question
    QString mimePath = QString("%1/%2")
                               .arg(DbManager::localShareLaunchMimePath)
                               .arg(QString(*mimeType).replace("/", "_"));

    // Create the directory if it doesn't exist so that it is easier to
    // manually add applications to it
    QDir dir(mimePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Populate appCandidates with the syminks at mimePath
    appCandidates =
            new QStringList(QDir(mimePath).entryList(QDir::NoDotAndDotDot | QDir::AllEntries));
    // Prepend each candidate with the path at which it was found
    for (auto r = 0; r < appCandidates->length(); r++) {
        appCandidates->replace(r, QString("%1/%2").arg(mimePath).arg(appCandidates->at(r)));
    }

    // Order apppCandidates by name and put ones ending in .desktop last
    std::sort(appCandidates->begin(), appCandidates->end());
    std::stable_sort(appCandidates->begin(), appCandidates->end(),
                     [](const QString &a, const QString &b) {
                         return a.endsWith(".desktop") < b.endsWith(".desktop");
                     });

    // If there are no appCandidates, then search for applications that can open the MIME type
    // up to the "_" part; e.g., if we have no candidates for "text_plain", then search for
    // candidates for "text" instead
    int appCandidatesCount = appCandidates->length();
    if (appCandidatesCount == 0) {
        qDebug() << "No candidates found for" << *mimeType;
        qDebug() << "Hence looking for candidates for"
                 << mimeType->replace("/", "_").split("_").first();
        // Get the parent of mimePath
        QString mimePathParent = QFileInfo(mimePath).path();
        // Make a mimePathDirs list of all directories in localShareLaunchMimePath that start with
        // e.g., "text_"
        QStringList *mimePathDirs = new QStringList(
                QDir(DbManager::localShareLaunchMimePath)
                        .entryList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::NoSymLinks));
        for (auto r = 0; r < mimePathDirs->length(); r++) {
            if (!mimePathDirs->at(r).startsWith(
                        QString(*mimeType).replace("/", "_").split("_").first() + "_")) {
                mimePathDirs->removeAt(r);
                r--;
            }
        }
        // Prepend each entry in mimePathDirs with their path
        for (auto r = 0; r < mimePathDirs->length(); r++) {
            mimePathDirs->replace(r,
                                  QString("%1/%2")
                                          .arg(DbManager::localShareLaunchMimePath)
                                          .arg(mimePathDirs->at(r)));
        }

        // In each of the mimePathDirs, look for candidates
        for (auto r = 0; r < mimePathDirs->length(); r++) {
            qDebug() << "Looking for candidates in" << mimePathDirs->at(r);
            QDir dir(mimePathDirs->at(r));
            dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            dir.setSorting(QDir::Name);
            QStringList *dirEntries = new QStringList(dir.entryList());
            for (auto s = 0; s < dirEntries->length(); s++) {
                // Construct path from dirEntries->at(s) and mimePathDirs->at(r)
                QString candPath = QString("%1/%2").arg(mimePathDirs->at(r)).arg(dirEntries->at(s));
                appCandidates->append(candPath);
            }
        }
    }

    // Order apppCandidates by name and put ones ending in .desktop last
    std::sort(appCandidates->begin(), appCandidates->end());
    std::stable_sort(appCandidates->begin(), appCandidates->end(),
                     [](const QString &a, const QString &b) {
                         return a.endsWith(".desktop") < b.endsWith(".desktop");
                     });

    // Remove entries with the filename "Default" from appCandidates
    for (auto r = 0; r < appCandidates->length(); r++) {
        if (QFileInfo(appCandidates->at(r)).fileName() == "Default") {
            appCandidates->removeAt(r);
            r--;
        }
    }

    // Print number of appCandidates
    qDebug() << "Found" << appCandidates->length() << "candidates for" << *mimeType;

    // Remove duplicates from appCandidates. For each candidate, resolve the symlink (if it is one)
    // Once we have resolved all symlinks, we can remove duplicates by comparing the resolved
    // paths
    QStringList *appCandidatesResolved = new QStringList();
    for (auto r = 0; r < appCandidates->length(); r++) {
        // Get canonical path of appCandidates->at(r) and if it is a symlink, resolve it
        QString appCandidateResolved = QFileInfo(appCandidates->at(r)).canonicalFilePath();
        appCandidatesResolved->append(appCandidateResolved);
    }
    *appCandidates = appCandidatesResolved->toSet().toList();

    // Print number of appCandidates
    qDebug() << "Found" << appCandidates->length() << "candidates for" << *mimeType;

    // Print appCandidates, each on a new line
    qDebug() << "appCandidates:";
    for (auto r = 0; r < appCandidates->length(); r++) {
        qDebug() << appCandidates->at(r);
    }

    // Let's see if we have at least one application candidate that is not a desktop file;
    // only fall back to showing desktop files if we don't. Those are second-class citizens
    // only supported to a minimum extent for compatibility with legacy applications

    QStringList *preferredAppCandidates;
    preferredAppCandidates = new QStringList();
    for (auto r = 0; r < appCandidates->length(); r++) {
        if (!appCandidates->at(r).endsWith(".desktop")) {
            preferredAppCandidates->append(appCandidates->at(r));
        }
    }

    // Print how many desktop files we have
    int desktopFilesCount = appCandidates->length() - preferredAppCandidates->length();
    // Print how many non-desktop files we have
    qDebug() << "Found" << preferredAppCandidates->length() << "non-desktop files";
    qDebug() << "Found" << desktopFilesCount << "desktop files";
    // Print whether showAlsoLegacyCandidates is true or false
    qDebug() << "showAlsoLegacyCandidates:" << showAlsoLegacyCandidates;

    if (!showAlsoLegacyCandidates && preferredAppCandidates->length() > 0) {
        // Use preferredAppCandidates instead of appCandidates
        appCandidates = preferredAppCandidates;
    }

    for (auto r = 0; r < appCandidates->length(); r++) {
        QListWidgetItem *item = new QListWidgetItem(appCandidates->at(r));
        item->setData(Qt::UserRole, QDir(appCandidates->at(r)).canonicalPath());
        item->setToolTip(QDir(appCandidates->at(r)).canonicalPath());
        QString completeBaseName = QFileInfo(item->text()).completeBaseName();
        item->setText(completeBaseName);
        ui->listWidget->addItem(item);
    }
}

ApplicationSelectionDialog::~ApplicationSelectionDialog()
{
    delete ui;
}

QString ApplicationSelectionDialog::getSelectedApplication()
{
    QString appPath = ui->listWidget->selectedItems().first()->data(Qt::UserRole).toString();
    if (ui->checkBoxAlwaysOpenThis->isChecked()) {
        qDebug() << "Writing open-with extended attribute";
        // Get the path from the selected item

        // Write the open-with extended attribute
        bool ok = false;
        ok = Fm::setAttributeValueQString(*fileOrProtocol, "open-with", appPath);
        if (!ok) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("Could not write the 'open-with' extended attribute");
            msgBox.exec();
        }

    } else if (ui->checkBoxAlwaysOpenAll->isChecked()) {
        
        // Clear the open-with extended attribute if it exists by setting it to NULL
        bool ok = false;
        ok = Fm::setAttributeValueQString(*fileOrProtocol, "open-with", NULL);
        if (!ok) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("Could not clear the 'open-with' extended attribute");
            msgBox.exec();
        }
        
        qDebug() << "Creating default symlink for this MIME type";
        // Use dbmanager to create a symlink in ~/.local/share/launch/MIME/<...>/Default to the
        // selected application

        QString mimePath = QString("%1/%2")
                                   .arg(DbManager::localShareLaunchMimePath)
                                   .arg(QString(*mimeType).replace("/", "_"));
        QString defaultPath = QString("%1/Default").arg(mimePath);
        qDebug() << "mimePath:" << mimePath;

        // Check if the MIME path exists and is a directory; if not, create it
        if (!QFile::exists(mimePath)) {
            qDebug() << "Creating directory for this MIME type";
            bool ok = false;
            ok = QDir().mkpath(mimePath);
            if (!ok) {
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.setText("Could not create the directory for this MIME type");
                msgBox.exec();
            }
        }

        qDebug() << "Removing existing default symlink if it exists";

        // Remove the symlink if it exists
        if (QFile::exists(defaultPath)) {
            bool ok = false;
            ok = QFile::remove(defaultPath);
            if (!ok) {
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.setText("Could not remove the existing default symlink");
                msgBox.exec();
            }
        }

        qDebug() << "Creating default symlink from %s to %s" << appPath << defaultPath;

        // Create the symlink
        ok = false;
        ok = QFile::link(appPath, defaultPath);
        if (!ok) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("Could not create the default symlink");
            msgBox.exec();
        }

    }

    return ui->listWidget->selectedItems().first()->text();
}

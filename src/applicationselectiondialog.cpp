#include "applicationselectiondialog.h"
#include "ui_applicationselectiondialog.h"
#include <QDebug>
#include "dbmanager.h"
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
                               .arg(*mimeType));

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->listWidget, &QListWidget::doubleClicked, this, &QDialog::accept);

    ui->checkBoxAlwaysOpenThis->setEnabled(false);
    ui->checkBoxAlwaysOpenAll->setEnabled(false);

    // When selection changes and something is selected, enable the checkboxes
    connect(ui->listWidget, &QListWidget::itemSelectionChanged, [=]() {
        if (ui->listWidget->selectedItems().length() > 0) {
            ui->checkBoxAlwaysOpenThis->setEnabled(true);
            ui->checkBoxAlwaysOpenAll->setEnabled(true);
        } else {
            ui->checkBoxAlwaysOpenThis->setEnabled(false);
            ui->checkBoxAlwaysOpenAll->setEnabled(false);
        }
    });

    QStringList *appCandidates;
    DbManager db;

    // Construct the path to the MIME type in question
    QString mimePath = QString("%1/%2")
                               .arg(DbManager::localShareLaunchMimePath)
                               .arg(QString(*mimeType).replace("/", "_"));

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

    // Remove entries with the filename "Default" from appCandidates
    for (auto r = 0; r < appCandidates->length(); r++) {
        if (QFileInfo(appCandidates->at(r)).fileName() == "Default") {
            appCandidates->removeAt(r);
            r--;
        }
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
        qDebug() << "Creating default symlink for this MIME type";
        // Use dbmanager to create a symlink in ~/.local/share/launch/MIME/<...>/Default to the
        // selected application
        DbManager db;
        QString mimePath = QString("%1/%2")
                                   .arg(DbManager::localShareLaunchMimePath)
                                   .arg(QString(*mimeType).replace("/", "_"));
        QString defaultPath = QString("%1/Default").arg(mimePath);

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

        // Create the symlink
        bool ok = false;
        ok = QFile::link(appPath, defaultPath);
        if (!ok) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("Could not create the default symlink");
            msgBox.exec();
        }

        // Clear the open-with extended attribute so that it doesn't override the MIME-wide default
        // for this file
        ok = false;
        ok = Fm::setAttributeValueQString(*fileOrProtocol, "open-with", NULL);
        if (!ok) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("Could not clear the 'open-with' extended attribute");
            msgBox.exec();
        }
    }

    return ui->listWidget->selectedItems().first()->text();
}

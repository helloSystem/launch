#include "applicationselectiondialog.h"
#include "ui_applicationselectiondialog.h"

ApplicationSelectionDialog::ApplicationSelectionDialog(QString *fileOrProtocol, QString *mimeType, QStringList *appCandidates, QString &selectedApplication, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ApplicationSelectionDialog)
{

    ui->setupUi(this);
    ui->label->setText(QString("Please choose an application to open \n'%1'\nof type '%2':" ).arg(*fileOrProtocol).arg(*mimeType));

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::reject);
    connect(ui->listWidget, &QListWidget::doubleClicked, this, &QDialog::accept);

    // Let's see if we have at least one application candidate that is not a desktop file;
    // only fall back to showing desktop files if we don't. Those are second-class citizens
    // only supported to a minimum extent for compatibility with legacy applications

    QStringList preferedAppCandidates = {};
    for (const QString appCandidate : *appCandidates) {
        if(!appCandidate.endsWith(".desktop")) {
            preferedAppCandidates.append(appCandidate);
        }
    }

    if(preferedAppCandidates.length() > 0) {
        // We have options that are not .desktop files; so just show those
        for (auto r=0; r < preferedAppCandidates.length(); r++) {
            QListWidgetItem *item = new QListWidgetItem(preferedAppCandidates.at(r));
            ui->listWidget->addItem(item);
            if(r==0)
                item->setSelected(true);
        }
    } else {
        // We need to show .desktop files because we have no other options available
        for (auto r=0; r < appCandidates->length(); r++) {
            QListWidgetItem *item = new QListWidgetItem(appCandidates->at(r));
            ui->listWidget->addItem(item);
            if(r==0)
                item->setSelected(true);
        }
    }


}

ApplicationSelectionDialog::~ApplicationSelectionDialog()
{
    delete ui;

}

QString ApplicationSelectionDialog::getSelectedApplication() {
   return ui->listWidget->selectedItems().first()->text();
}

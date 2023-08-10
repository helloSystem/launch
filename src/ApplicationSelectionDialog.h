#ifndef APPLICATIONSELECTIONDIALOG_H
#define APPLICATIONSELECTIONDIALOG_H

#include <QDialog>
#include <QListWidget>
#include "DbManager.h"

namespace Ui {
class ApplicationSelectionDialog;
}

class ApplicationSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApplicationSelectionDialog(QString *fileOrProtocol, QString *mimeType,
                                        bool showAlsoLegacyCandidates = false,
                                        bool showAllCandidates = false,
                                        QWidget *parent = nullptr);
    ~ApplicationSelectionDialog();
    QString getSelectedApplication();

private:
    QString *fileOrProtocol;
    QString *mimeType;
    bool showAlsoLegacyCandidates;
    Ui::ApplicationSelectionDialog *ui;
    DbManager *db;
};

#endif // APPLICATIONSELECTIONDIALOG_H

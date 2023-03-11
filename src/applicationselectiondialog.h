#ifndef APPLICATIONSELECTIONDIALOG_H
#define APPLICATIONSELECTIONDIALOG_H

#include <QDialog>
#include <QListWidget>

namespace Ui {
class ApplicationSelectionDialog;
}

class ApplicationSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApplicationSelectionDialog(QString *fileOrProtocol, QString *mimeType,
                                        QStringList *appCandidates,
                                        bool showAlsoLegacyCandidates = false,
                                        QWidget *parent = nullptr);
    ~ApplicationSelectionDialog();
    QString getSelectedApplication();

private:
    Ui::ApplicationSelectionDialog *ui;
};

#endif // APPLICATIONSELECTIONDIALOG_H

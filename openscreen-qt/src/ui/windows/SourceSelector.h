#pragma once

#include <QDialog>
#include <QList>
#include <QPair>
#include <QPixmap>
#include <QString>

class QListWidget;
class QDialogButtonBox;

namespace openscreen {

class SourceSelector : public QDialog {
    Q_OBJECT

public:
    explicit SourceSelector(QWidget* parent = nullptr);
    ~SourceSelector() override = default;

    void setSources(const QList<QPair<QString, QPixmap>>& sources);

signals:
    void sourceSelected(int index);

private slots:
    void onAccepted();
    void onItemDoubleClicked();

private:
    QListWidget* listWidget_{nullptr};
    QDialogButtonBox* buttonBox_{nullptr};
};

} // namespace openscreen

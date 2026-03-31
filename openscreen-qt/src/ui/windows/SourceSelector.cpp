#include "SourceSelector.h"

#include <QDialogButtonBox>
#include <QIcon>
#include <QListWidget>
#include <QVBoxLayout>

namespace openscreen {

SourceSelector::SourceSelector(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Source"));
    setFixedSize(620, 420);
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    listWidget_ = new QListWidget(this);
    listWidget_->setViewMode(QListWidget::IconMode);
    listWidget_->setGridSize(QSize(180, 140));
    listWidget_->setIconSize(QSize(160, 100));
    listWidget_->setResizeMode(QListWidget::Adjust);
    listWidget_->setMovement(QListWidget::Static);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(listWidget_);

    buttonBox_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &SourceSelector::onAccepted);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(listWidget_, &QListWidget::itemDoubleClicked, this, &SourceSelector::onItemDoubleClicked);
}

void SourceSelector::setSources(const QList<QPair<QString, QPixmap>>& sources)
{
    listWidget_->clear();

    for (const auto& [name, thumbnail] : sources) {
        auto* item = new QListWidgetItem(QIcon(thumbnail), name);
        listWidget_->addItem(item);
    }

    if (listWidget_->count() > 0) {
        listWidget_->setCurrentRow(0);
    }
}

void SourceSelector::onAccepted()
{
    const int index = listWidget_->currentRow();
    if (index >= 0) {
        emit sourceSelected(index);
        accept();
    }
}

void SourceSelector::onItemDoubleClicked()
{
    const int index = listWidget_->currentRow();
    if (index >= 0) {
        emit sourceSelected(index);
        accept();
    }
}

} // namespace openscreen

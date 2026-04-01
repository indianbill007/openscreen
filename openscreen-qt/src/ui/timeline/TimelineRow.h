#pragma once
#include <QWidget>
#include <QString>
#include <QColor>
#include <cstdint>
#include <vector>
#include <functional>

class QPushButton;

namespace openscreen {

class TimelineItem;

class TimelineRow : public QWidget {
    Q_OBJECT
public:
    explicit TimelineRow(const QString& label, const QColor& labelColor,
                         QWidget* parent = nullptr);

    void setViewRange(int64_t startMs, int64_t endMs);
    void setDuration(int64_t durationMs);

    /// Add an item to this row
    TimelineItem* addItem(const QString& id, TimelineItem::Variant variant,
                          int64_t startMs, int64_t endMs, const QString& label = {});

    /// Remove an item by ID
    bool removeItem(const QString& id);

    /// Remove all items
    void clearItems();

    /// Update an item's time range
    bool updateItemRange(const QString& id, int64_t startMs, int64_t endMs);

    /// Select an item (deselects others)
    void selectItem(const QString& id);

    /// Deselect all items
    void deselectAll();

    /// Get all items
    const std::vector<TimelineItem*>& items() const { return items_; }

    // Coordinate conversion
    int timeToPosition(int64_t timeMs) const;
    int64_t positionToTime(int x) const;

signals:
    void addRequested();
    void itemSelected(const QString& id);
    void itemMoved(const QString& id, int64_t startMs, int64_t endMs);
    void itemResized(const QString& id, int64_t startMs, int64_t endMs);
    void itemDeleteRequested(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void layoutItems();
    void connectItem(TimelineItem* item);

    QString label_;
    QColor labelColor_;
    QPushButton* addButton_;

    std::vector<TimelineItem*> items_;
    int64_t viewStartMs_ = 0;
    int64_t viewEndMs_ = 0;
    int64_t durationMs_ = 0;

    static constexpr int kRowHeight = 48;
    static constexpr int kHeaderWidth = 48;
    static constexpr int kItemMarginY = 4;
};

} // namespace openscreen

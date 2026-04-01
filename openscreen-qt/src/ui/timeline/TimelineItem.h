#pragma once
#include <QWidget>
#include <cstdint>
#include <functional>

namespace openscreen {

class TimelineItem : public QWidget {
    Q_OBJECT
public:
    enum class Variant { Zoom, Trim, Speed, Annotation };

    explicit TimelineItem(const QString& id, Variant variant, QWidget* parent = nullptr);
    ~TimelineItem() override = default;

    void setTimeRange(int64_t startMs, int64_t endMs);
    void setSelected(bool selected);
    void setLabel(const QString& label);

    QString id() const { return id_; }
    Variant variant() const { return variant_; }
    int64_t startMs() const { return startMs_; }
    int64_t endMs() const { return endMs_; }
    bool isSelected() const { return selected_; }

    // Conversion functions (set by parent TimelineRow)
    std::function<int(int64_t)> timeToX;
    std::function<int64_t(int)> xToTime;

signals:
    void selected(const QString& id);
    void moved(const QString& id, int64_t newStartMs, int64_t newEndMs);
    void resized(const QString& id, int64_t newStartMs, int64_t newEndMs);
    void deleteRequested(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    enum class DragMode { None, Move, ResizeLeft, ResizeRight };
    static constexpr int kResizeHandleWidth = 6;

    QColor variantColor() const;
    QColor variantColorLight() const;
    DragMode hitTest(int x) const;

    QString id_;
    Variant variant_;
    int64_t startMs_ = 0;
    int64_t endMs_ = 0;
    bool selected_ = false;
    QString label_;

    DragMode dragMode_ = DragMode::None;
    int dragStartX_ = 0;
    int64_t dragStartMs_ = 0;
    int64_t dragEndMs_ = 0;
};

} // namespace openscreen

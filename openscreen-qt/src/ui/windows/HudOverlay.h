#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QPoint;

namespace openscreen {

class HudOverlay : public QWidget {
    Q_OBJECT

public:
    explicit HudOverlay(QWidget* parent = nullptr);
    ~HudOverlay() override = default;

    void setElapsedTime(int seconds);

signals:
    void recordToggled(bool recording);
    void micToggled(bool enabled);
    void systemAudioToggled(bool enabled);
    void minimizeRequested();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void createLayout();

    QPushButton* recordButton_{nullptr};
    QLabel* timerLabel_{nullptr};
    QPushButton* micButton_{nullptr};
    QPushButton* systemAudioButton_{nullptr};
    QPushButton* minimizeButton_{nullptr};
    QPushButton* closeButton_{nullptr};

    QPoint dragStartPosition_;
};

} // namespace openscreen

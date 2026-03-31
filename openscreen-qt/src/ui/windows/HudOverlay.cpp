#include "HudOverlay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>

namespace openscreen {

HudOverlay::HudOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 155);

    createLayout();
}

void HudOverlay::createLayout()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    // Record button
    recordButton_ = new QPushButton(this);
    recordButton_->setCheckable(true);
    recordButton_->setFixedSize(48, 48);
    recordButton_->setToolTip(tr("Record / Stop"));
    recordButton_->setStyleSheet(
        "QPushButton {"
        "  background-color: #e53e3e;"
        "  border: none;"
        "  border-radius: 24px;"
        "}"
        "QPushButton:checked {"
        "  background-color: #c53030;"
        "  border-radius: 8px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #fc8181;"
        "}");
    connect(recordButton_, &QPushButton::toggled, this, &HudOverlay::recordToggled);
    layout->addWidget(recordButton_);

    // Timer label
    timerLabel_ = new QLabel("00:00", this);
    timerLabel_->setStyleSheet(
        "QLabel {"
        "  color: #ffffff;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 24px;"
        "  padding: 0 8px;"
        "}");
    layout->addWidget(timerLabel_);

    // Mic toggle
    micButton_ = new QPushButton(tr("Mic"), this);
    micButton_->setCheckable(true);
    micButton_->setChecked(true);
    micButton_->setFixedSize(48, 48);
    micButton_->setToolTip(tr("Toggle Microphone"));
    connect(micButton_, &QPushButton::toggled, this, &HudOverlay::micToggled);
    layout->addWidget(micButton_);

    // System audio toggle
    systemAudioButton_ = new QPushButton(tr("Sys"), this);
    systemAudioButton_->setCheckable(true);
    systemAudioButton_->setChecked(true);
    systemAudioButton_->setFixedSize(48, 48);
    systemAudioButton_->setToolTip(tr("Toggle System Audio"));
    connect(systemAudioButton_, &QPushButton::toggled, this, &HudOverlay::systemAudioToggled);
    layout->addWidget(systemAudioButton_);

    layout->addStretch();

    // Minimize button
    minimizeButton_ = new QPushButton(tr("_"), this);
    minimizeButton_->setFixedSize(32, 32);
    minimizeButton_->setToolTip(tr("Minimize"));
    connect(minimizeButton_, &QPushButton::clicked, this, &HudOverlay::minimizeRequested);
    layout->addWidget(minimizeButton_);

    // Close button
    closeButton_ = new QPushButton(tr("X"), this);
    closeButton_->setFixedSize(32, 32);
    closeButton_->setToolTip(tr("Close"));
    connect(closeButton_, &QPushButton::clicked, this, &HudOverlay::closeRequested);
    layout->addWidget(closeButton_);
}

void HudOverlay::setElapsedTime(int seconds)
{
    const int minutes = seconds / 60;
    const int secs = seconds % 60;
    timerLabel_->setText(
        QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0')));
}

void HudOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(30, 30, 30, 220));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 12, 12);
}

void HudOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragStartPosition_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void HudOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragStartPosition_);
        event->accept();
    }
}

} // namespace openscreen

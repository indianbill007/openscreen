#include "Theme.h"

namespace openscreen {

QString darkStyleSheet()
{
    return QStringLiteral(R"qss(

/* ── Main Window ── */
QMainWindow {
    background-color: #1a1a1a;
    color: #e0e0e0;
}

/* ── Generic Widgets / Panels ── */
QWidget {
    background-color: #242424;
    color: #e0e0e0;
    font-size: 13px;
}

/* ── Menu Bar ── */
QMenuBar {
    background-color: #1a1a1a;
    color: #e0e0e0;
    border-bottom: 1px solid #333333;
    padding: 2px;
}
QMenuBar::item {
    background: transparent;
    padding: 6px 12px;
    border-radius: 4px;
}
QMenuBar::item:selected {
    background-color: #6366f1;
    color: #ffffff;
}

/* ── Menus ── */
QMenu {
    background-color: #242424;
    color: #e0e0e0;
    border: 1px solid #333333;
    padding: 4px 0;
}
QMenu::item {
    padding: 6px 28px 6px 20px;
}
QMenu::item:selected {
    background-color: #6366f1;
    color: #ffffff;
}
QMenu::separator {
    height: 1px;
    background-color: #333333;
    margin: 4px 8px;
}

/* ── Push Buttons ── */
QPushButton {
    background-color: #2a2a2a;
    color: #e0e0e0;
    border: 1px solid #333333;
    border-radius: 6px;
    padding: 6px 16px;
    min-height: 24px;
}
QPushButton:hover {
    background-color: #6366f1;
    border-color: #6366f1;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: #4f46e5;
}
QPushButton:disabled {
    background-color: #1a1a1a;
    color: #666666;
    border-color: #2a2a2a;
}
QPushButton:checked {
    background-color: #6366f1;
    color: #ffffff;
    border-color: #6366f1;
}

/* ── Line Edit / Text Edit ── */
QLineEdit, QTextEdit, QPlainTextEdit {
    background-color: #1a1a1a;
    color: #e0e0e0;
    border: 1px solid #333333;
    border-radius: 4px;
    padding: 4px 8px;
    selection-background-color: #6366f1;
    selection-color: #ffffff;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border-color: #6366f1;
}

/* ── Combo Box ── */
QComboBox {
    background-color: #2a2a2a;
    color: #e0e0e0;
    border: 1px solid #333333;
    border-radius: 4px;
    padding: 4px 8px;
}
QComboBox:hover {
    border-color: #6366f1;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background-color: #242424;
    color: #e0e0e0;
    border: 1px solid #333333;
    selection-background-color: #6366f1;
    selection-color: #ffffff;
}

/* ── Spin Box ── */
QSpinBox, QDoubleSpinBox {
    background-color: #1a1a1a;
    color: #e0e0e0;
    border: 1px solid #333333;
    border-radius: 4px;
    padding: 4px;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #6366f1;
}

/* ── Scroll Bars ── */
QScrollBar:vertical {
    background-color: #1a1a1a;
    width: 10px;
    border: none;
}
QScrollBar::handle:vertical {
    background-color: #444444;
    min-height: 30px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background-color: #6366f1;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background-color: #1a1a1a;
    height: 10px;
    border: none;
}
QScrollBar::handle:horizontal {
    background-color: #444444;
    min-width: 30px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover {
    background-color: #6366f1;
}
QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
    width: 0;
}

/* ── Slider ── */
QSlider::groove:horizontal {
    background-color: #333333;
    height: 6px;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background-color: #6366f1;
    width: 16px;
    height: 16px;
    margin: -5px 0;
    border-radius: 8px;
}
QSlider::handle:horizontal:hover {
    background-color: #818cf8;
}
QSlider::groove:vertical {
    background-color: #333333;
    width: 6px;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background-color: #6366f1;
    width: 16px;
    height: 16px;
    margin: 0 -5px;
    border-radius: 8px;
}

/* ── Splitter ── */
QSplitter::handle {
    background-color: #333333;
}
QSplitter::handle:horizontal {
    width: 3px;
}
QSplitter::handle:vertical {
    height: 3px;
}
QSplitter::handle:hover {
    background-color: #6366f1;
}

/* ── Tab Widget ── */
QTabWidget::pane {
    background-color: #242424;
    border: 1px solid #333333;
    border-top: none;
}
QTabBar::tab {
    background-color: #1a1a1a;
    color: #e0e0e0;
    padding: 8px 16px;
    border: 1px solid #333333;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background-color: #242424;
    border-bottom-color: #242424;
    color: #ffffff;
}
QTabBar::tab:hover:!selected {
    background-color: #2a2a2a;
}

/* ── Group Box ── */
QGroupBox {
    background-color: #242424;
    border: 1px solid #333333;
    border-radius: 6px;
    margin-top: 12px;
    padding-top: 16px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 2px 8px;
    color: #e0e0e0;
}

/* ── List Widget / Tree Widget / Table Widget ── */
QListWidget, QTreeWidget, QTableWidget, QListView, QTreeView, QTableView {
    background-color: #1a1a1a;
    color: #e0e0e0;
    border: 1px solid #333333;
    alternate-background-color: #222222;
    selection-background-color: #6366f1;
    selection-color: #ffffff;
}
QHeaderView::section {
    background-color: #2a2a2a;
    color: #e0e0e0;
    border: 1px solid #333333;
    padding: 4px 8px;
}

/* ── Tool Tips ── */
QToolTip {
    background-color: #2a2a2a;
    color: #e0e0e0;
    border: 1px solid #333333;
    border-radius: 4px;
    padding: 4px 8px;
}

/* ── Status Bar ── */
QStatusBar {
    background-color: #1a1a1a;
    color: #e0e0e0;
    border-top: 1px solid #333333;
}

/* ── Progress Bar ── */
QProgressBar {
    background-color: #1a1a1a;
    border: 1px solid #333333;
    border-radius: 4px;
    text-align: center;
    color: #e0e0e0;
}
QProgressBar::chunk {
    background-color: #6366f1;
    border-radius: 3px;
}

/* ── Check Box / Radio Button ── */
QCheckBox, QRadioButton {
    color: #e0e0e0;
    spacing: 8px;
}
QCheckBox::indicator, QRadioButton::indicator {
    width: 16px;
    height: 16px;
}
QCheckBox:disabled, QRadioButton:disabled {
    color: #666666;
}

/* ── Label ── */
QLabel {
    background: transparent;
    color: #e0e0e0;
}

/* ── Dialog ── */
QDialog {
    background-color: #242424;
    color: #e0e0e0;
}

/* ── Disabled State ── */
*:disabled {
    color: #666666;
}

/* ── Focus ── */
*:focus {
    outline-color: #6366f1;
}

)qss");
}

} // namespace openscreen

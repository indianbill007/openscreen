#pragma once

#include <QMainWindow>

class QSplitter;
class QWidget;
class QAction;

namespace openscreen {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onExport();
    void onUndo();
    void onRedo();
    void onAbout();

private:
    void createMenuBar();
    void createCentralArea();
    void createStatusBar();

    QSplitter* splitter_{nullptr};
    QWidget* preview_placeholder_{nullptr};
    QWidget* settings_placeholder_{nullptr};
    QWidget* timeline_placeholder_{nullptr};

    QAction* toggleTimelineAction_{nullptr};
    QAction* toggleSettingsAction_{nullptr};
};

} // namespace openscreen

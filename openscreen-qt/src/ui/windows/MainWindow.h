#pragma once

#include <QMainWindow>

class QSplitter;
class QWidget;
class QAction;

namespace openscreen {

class RecordingSession;
class HudOverlay;
class SourceSelector;
class VideoPreview;
class PlaybackControls;
class PlaybackEngine;
class EditorHistory;

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

    void onStartRecording();
    void onStopRecording();
    void onRecordingStopped(const QString& outputPath);
    void onShowSourceSelector();
    void onOpenVideo();
    void onVideoLoaded(const QString& filePath);

private:
    void createMenuBar();
    void createCentralArea();
    void createStatusBar();
    void setupRecording();
    void setupPlayback();

    QSplitter* splitter_{nullptr};
    QWidget* settings_placeholder_{nullptr};
    QWidget* timeline_placeholder_{nullptr};

    QAction* toggleTimelineAction_{nullptr};
    QAction* toggleSettingsAction_{nullptr};

    RecordingSession* recordingSession_{nullptr};
    HudOverlay* hudOverlay_{nullptr};
    SourceSelector* sourceSelector_{nullptr};

    VideoPreview* videoPreview_{nullptr};
    PlaybackControls* playbackControls_{nullptr};
    PlaybackEngine* playbackEngine_{nullptr};
    EditorHistory* editorHistory_{nullptr};
};

} // namespace openscreen

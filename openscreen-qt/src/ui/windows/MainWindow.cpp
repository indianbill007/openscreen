#include "MainWindow.h"
#include "HudOverlay.h"
#include "SourceSelector.h"
#include "capture/RecordingSession.h"
#include "capture/ScreenCapture.h"

#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace openscreen {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("OpenScreen");
    setMinimumSize(1200, 800);

    createMenuBar();
    createCentralArea();
    createStatusBar();
    setupRecording();
}

void MainWindow::createMenuBar()
{
    auto* menuBar = this->menuBar();

    // File menu
    auto* fileMenu = menuBar->addMenu(tr("&File"));

    auto* newAction = fileMenu->addAction(tr("&New Project"));
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewProject);

    auto* openAction = fileMenu->addAction(tr("&Open Project"));
    openAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    auto* saveAction = fileMenu->addAction(tr("&Save Project"));
    saveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveProject);

    auto* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveProjectAs);

    fileMenu->addSeparator();

    auto* exportAction = fileMenu->addAction(tr("&Export"));
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExport);

    fileMenu->addSeparator();

    auto* quitAction = fileMenu->addAction(tr("&Quit"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // Edit menu
    auto* editMenu = menuBar->addMenu(tr("&Edit"));

    auto* undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    connect(undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    auto* redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    connect(redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    editMenu->addSeparator();

    auto* preferencesAction = editMenu->addAction(tr("&Preferences"));
    connect(preferencesAction, &QAction::triggered, this, [this]() {
        qDebug() << "Preferences triggered";
    });

    // View menu
    auto* viewMenu = menuBar->addMenu(tr("&View"));

    toggleTimelineAction_ = viewMenu->addAction(tr("Toggle &Timeline"));
    toggleTimelineAction_->setCheckable(true);
    toggleTimelineAction_->setChecked(true);
    connect(toggleTimelineAction_, &QAction::toggled, this, [this](bool checked) {
        qDebug() << "Toggle Timeline:" << checked;
        if (timeline_placeholder_) {
            timeline_placeholder_->setVisible(checked);
        }
    });

    toggleSettingsAction_ = viewMenu->addAction(tr("Toggle &Settings Panel"));
    toggleSettingsAction_->setCheckable(true);
    toggleSettingsAction_->setChecked(true);
    connect(toggleSettingsAction_, &QAction::toggled, this, [this](bool checked) {
        qDebug() << "Toggle Settings Panel:" << checked;
        if (settings_placeholder_) {
            settings_placeholder_->setVisible(checked);
        }
    });

    viewMenu->addSeparator();

    auto* selectSourceAction = viewMenu->addAction(tr("Select &Source"));
    connect(selectSourceAction, &QAction::triggered, this, &MainWindow::onShowSourceSelector);

    // Help menu
    auto* helpMenu = menuBar->addMenu(tr("&Help"));

    auto* aboutAction = helpMenu->addAction(tr("&About OpenScreen"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createCentralArea()
{
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Horizontal splitter: preview | settings
    splitter_ = new QSplitter(Qt::Horizontal, centralWidget);

    preview_placeholder_ = new QWidget(splitter_);
    preview_placeholder_->setObjectName("preview_placeholder");
    preview_placeholder_->setMinimumWidth(400);

    settings_placeholder_ = new QWidget(splitter_);
    settings_placeholder_->setObjectName("settings_placeholder");
    settings_placeholder_->setMinimumWidth(200);

    splitter_->addWidget(preview_placeholder_);
    splitter_->addWidget(settings_placeholder_);
    splitter_->setStretchFactor(0, 3);
    splitter_->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter_, 3);

    // Timeline placeholder below the splitter
    timeline_placeholder_ = new QWidget(centralWidget);
    timeline_placeholder_->setObjectName("timeline_placeholder");
    timeline_placeholder_->setMinimumHeight(150);

    mainLayout->addWidget(timeline_placeholder_, 1);

    setCentralWidget(centralWidget);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::onNewProject()
{
    qDebug() << "New Project triggered";
}

void MainWindow::onOpenProject()
{
    qDebug() << "Open Project triggered";
}

void MainWindow::onSaveProject()
{
    qDebug() << "Save Project triggered";
}

void MainWindow::onSaveProjectAs()
{
    qDebug() << "Save Project As triggered";
}

void MainWindow::onExport()
{
    qDebug() << "Export triggered";
}

void MainWindow::onUndo()
{
    qDebug() << "Undo triggered";
}

void MainWindow::onRedo()
{
    qDebug() << "Redo triggered";
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this,
        tr("About OpenScreen"),
        tr("<h3>OpenScreen</h3>"
           "<p>Version 2.0.0</p>"
           "<p>Free, open-source screen recording &amp; editing</p>"));
}

void MainWindow::setupRecording()
{
    recordingSession_ = new RecordingSession(this);
    hudOverlay_ = new HudOverlay(nullptr);  // top-level window
    sourceSelector_ = new SourceSelector(this);

    // HUD record toggle -> start/stop
    connect(hudOverlay_, &HudOverlay::recordToggled, this,
        [this](bool recording) {
            if (recording) {
                onStartRecording();
            } else {
                onStopRecording();
            }
        });

    // HUD audio toggles
    connect(hudOverlay_, &HudOverlay::micToggled,
            recordingSession_, &RecordingSession::setMicrophoneEnabled);
    connect(hudOverlay_, &HudOverlay::systemAudioToggled,
            recordingSession_, &RecordingSession::setSystemAudioEnabled);

    // Elapsed time -> HUD
    connect(recordingSession_, &RecordingSession::elapsedTimeChanged,
            hudOverlay_, &HudOverlay::setElapsedTime);

    // Recording finished -> restore UI
    connect(recordingSession_, &RecordingSession::recordingStopped,
            this, &MainWindow::onRecordingStopped);

    // Recording error -> status bar
    connect(recordingSession_, &RecordingSession::recordingError, this,
        [this](const QString& error) {
            statusBar()->showMessage(tr("Recording error: %1").arg(error), 5000);
        });

    // Source selector -> select source
    connect(sourceSelector_, &SourceSelector::sourceSelected, this,
        [this](int index) {
            auto sources = recordingSession_->enumerateSources();
            if (index >= 0 && index < static_cast<int>(sources.size())) {
                recordingSession_->selectSource(sources[index].id);
                statusBar()->showMessage(
                    tr("Source: %1").arg(QString::fromStdString(sources[index].name)),
                    3000);
            }
        });
}

void MainWindow::onStartRecording()
{
    auto moviesDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (moviesDir.isEmpty()) {
        moviesDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }

    auto timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    auto filePath = moviesDir + "/OpenScreen_" + timestamp + ".mp4";
    auto outputPath = filePath.toStdString();

    if (!recordingSession_->startRecording(outputPath)) {
        statusBar()->showMessage(tr("Failed to start recording"), 3000);
        return;
    }

    hide();
    hudOverlay_->show();
    statusBar()->showMessage(tr("Recording..."));
}

void MainWindow::onStopRecording()
{
    recordingSession_->stopRecording();
}

void MainWindow::onRecordingStopped(const QString& outputPath)
{
    hudOverlay_->hide();
    show();
    statusBar()->showMessage(
        tr("Recording saved: %1").arg(outputPath), 5000);
}

void MainWindow::onShowSourceSelector()
{
    auto sources = recordingSession_->enumerateSources();
    QList<QPair<QString, QPixmap>> sourceList;
    sourceList.reserve(static_cast<int>(sources.size()));

    for (const auto& src : sources) {
        sourceList.append({QString::fromStdString(src.name), src.thumbnail});
    }

    sourceSelector_->setSources(sourceList);
    sourceSelector_->show();
}

} // namespace openscreen

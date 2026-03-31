#pragma once

#include <QApplication>
#include <memory>

class QMainWindow;

namespace openscreen {

class Settings;

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    void initialize();

    Settings* settings() const;

private:
    std::unique_ptr<Settings>     m_settings;
    std::unique_ptr<QMainWindow>  m_mainWindow;
};

} // namespace openscreen

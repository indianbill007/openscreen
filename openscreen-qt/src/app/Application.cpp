#include "Application.h"

#include "Settings.h"

#include <QMainWindow>

namespace openscreen {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_settings(std::make_unique<Settings>())
{
}

Application::~Application() = default;

void Application::initialize()
{
    m_mainWindow = std::make_unique<QMainWindow>();
    m_mainWindow->setWindowTitle("OpenScreen");
    m_mainWindow->resize(1280, 720);
    m_mainWindow->show();
}

Settings* Application::settings() const
{
    return m_settings.get();
}

} // namespace openscreen

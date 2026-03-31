#include <QApplication>
#include <QIcon>

#include "app/Application.h"

int main(int argc, char* argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    openscreen::Application app(argc, argv);

    app.setApplicationName("OpenScreen");
    app.setOrganizationName("OpenScreen");
    app.setApplicationVersion("2.0.0");
    app.setWindowIcon(QIcon(":/icons/openscreen.png"));

    app.initialize();

    return app.exec();
}

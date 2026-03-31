#include "Settings.h"

#include <QJsonDocument>
#include <QSettings>

namespace openscreen {

Settings::Settings()
    : m_settings(std::make_unique<QSettings>("OpenScreen", "OpenScreen"))
{
}

Settings::~Settings() = default;

QString Settings::locale() const
{
    return m_settings->value("general/locale", "en").toString();
}

void Settings::setLocale(const QString& locale)
{
    m_settings->setValue("general/locale", locale);
}

QJsonObject Settings::shortcuts() const
{
    const QString raw = m_settings->value("shortcuts").toString();
    if (raw.isEmpty()) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    return doc.object();
}

void Settings::setShortcuts(const QJsonObject& shortcuts)
{
    const QJsonDocument doc(shortcuts);
    m_settings->setValue("shortcuts", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

QStringList Settings::recentProjects() const
{
    return m_settings->value("recentProjects").toStringList();
}

void Settings::addRecentProject(const QString& path)
{
    QStringList projects = recentProjects();

    projects.removeAll(path);
    projects.prepend(path);

    while (projects.size() > kMaxRecentProjects) {
        projects.removeLast();
    }

    m_settings->setValue("recentProjects", projects);
}

} // namespace openscreen

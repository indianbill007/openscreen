#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

class QSettings;

namespace openscreen {

class Settings
{
public:
    Settings();
    ~Settings();

    // Locale
    QString locale() const;
    void    setLocale(const QString& locale);

    // Keyboard shortcuts
    QJsonObject shortcuts() const;
    void        setShortcuts(const QJsonObject& shortcuts);

    // Recent projects
    QStringList recentProjects() const;
    void        addRecentProject(const QString& path);

private:
    static constexpr int kMaxRecentProjects = 10;

    std::unique_ptr<QSettings> m_settings;
};

} // namespace openscreen

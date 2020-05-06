#include <QStringList>
#include <QString>
#include <QDirIterator>
#include <QDebug>
#include <QFileInfo>

#include "gamelist/gamelistwatcher.hpp"

void IOTask::add_directory(const QString dir)
{
     const QStringList file_types({
        "*.iso",
        "*.cso",
        "*.elf",
        "*.gsd",
        "*.bin"
     });

    QDirIterator it(dir, file_types,
        QDir::Files, QDirIterator::Subdirectories
    );

    QList<GameInfo> list;
    while (it.hasNext())
    {
        it.next();

        auto file_info = QFileInfo(it.filePath());

        GameInfo info = {};
        info.path = it.filePath();
        info.name = file_info.baseName();
        info.size = file_info.size();

        list.append(info);
        emit game_found(info);
    }

    m_path_map.insert(dir, list);
    emit finished();
}

void IOTask::remove_directory(const QString dir)
{
    auto games = m_path_map.take(dir);

    for (const auto game : games)
        emit game_removed(game);

    emit finished();
}

GameListWatcher::GameListWatcher(QObject* parent)
{
    // needed to communicate this type between threads
    qRegisterMetaType<GameInfo>("GameInfo");

    m_task.moveToThread(&m_io_thread);

    connect(&m_io_thread, &QThread::started, []() {
        qDebug() << "[gamelist] IO thread started";
    });
    connect(&m_io_thread, &QThread::finished, []() {
        qDebug() << "[gamelist] IO thread shutdown";
    });

    connect(this, &GameListWatcher::directory_added, &m_task, &IOTask::add_directory);
    connect(this, &GameListWatcher::directory_removed, &m_task, &IOTask::remove_directory);

    connect(&m_task, &IOTask::game_found, [this](const GameInfo info) {
        emit game_loaded(info);
    });
    connect(&m_task, &IOTask::game_removed, [this](const GameInfo info) {
        emit game_removed(info);
    });
    connect(&m_task, &IOTask::finished, [this]() {
        qDebug() << "[gamelist] IO task finished";
        emit directory_processed();
    });
}

void GameListWatcher::start()
{
    m_io_thread.start();
}

void GameListWatcher::add_directory(const QString& dir)
{
    qDebug() << "[gamelist] Adding path: " << dir;
    emit directory_added(dir);
}

void GameListWatcher::remove_directory(const QString& dir)
{
    qDebug() << "[gamelist] Removing path: " << dir;
    emit directory_removed(dir);
}

GameListWatcher::~GameListWatcher()
{
    m_io_thread.quit();
    m_io_thread.wait();
}
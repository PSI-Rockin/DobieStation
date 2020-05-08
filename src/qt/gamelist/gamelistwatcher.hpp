#ifndef GAMELISTWATCHER_HPP
#define GAMELISTWATCHER_HPP
#include <QString>
#include <QMap>
#include <QThread>

struct GameInfo
{
    QString path;
    QString name;
    QString type;
    qint64 size;

    bool operator==(const GameInfo& other) const
    {
        return path == other.path;
    }
};


// Everything in IOTask executes on another thread
// with another event loop
class IOTask : public QObject
{
    Q_OBJECT

    private:
        QMap<QString, QList<GameInfo>> m_path_map;
    public slots:
        void add_directory(const QString dir);
        void remove_directory(const QString dir);
    signals:
        void game_found(const GameInfo game);
        void game_removed(const GameInfo game);
        void finished();
};

class GameListWatcher : public QObject
{
    Q_OBJECT

    private:
        QThread m_io_thread;
        IOTask m_task;

    public:
        explicit GameListWatcher(QObject* parent = nullptr);
        ~GameListWatcher();

        void start();
        void add_directory(const QString& dir);
        void remove_directory(const QString& dir);

    signals:
        void game_loaded(const GameInfo info);
        void game_updated(const GameInfo info);
        void game_removed(const GameInfo info);
        void directory_added(const QString dir);
        void directory_removed(const QString dir);
        void directory_processed();
};

#endif
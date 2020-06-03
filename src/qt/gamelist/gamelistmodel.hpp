#ifndef GAMELISTMODEL_HPP
#define GAMELISTMODEL_HPP

#include <QAbstractTableModel>
#include "gamelistwatcher.hpp"

class GameListModel final : public QAbstractTableModel
{
    Q_OBJECT

    private:
        QList<GameInfo> m_games;
        GameListWatcher watcher;

    public:
        enum
        {
            COLUMN_NAME,
            COLUMN_SIZE,
            COLUMN_TYPE,
            COLUMN_MAX
        };

        explicit GameListModel(QObject* parent = nullptr);

        QVariant data(const QModelIndex& index,
            int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation,
            int role = Qt::DisplayRole) const override;
        int rowCount(const QModelIndex& index) const override;
        int columnCount(const QModelIndex& index) const override;

        GameInfo get_game_info(int index);
        GameListWatcher* get_watcher();

    public slots:
        void add_game(const GameInfo game);
        void remove_game(const GameInfo game);

    signals:
        void directory_processed();

    private:
        QString get_formatted_data_size(uint64_t size) const;
};

#endif

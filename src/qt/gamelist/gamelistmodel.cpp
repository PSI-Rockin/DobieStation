#include <QDirIterator>
#include <QLocale>
#include <QDebug>

// For old Qt workaround
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
#include <cmath>
#endif

#include "../settings.hpp"
#include "gamelistmodel.hpp"

static QMap<QString, QString> s_file_type_desc{
    {"elf", "PS2 Executable"},
    {"iso", "ISO Image"},
    {"cso", "Compressed ISO"},
    {"gsd", "GSDump"},
    {"bin", "BIN/CUE"}
};

GameListModel::GameListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    connect(&watcher, &GameListWatcher::game_loaded, this, &GameListModel::add_game);
    connect(&watcher, &GameListWatcher::game_removed, this, &GameListModel::remove_game);
    connect(&watcher, &GameListWatcher::directory_processed, [this]() {
        emit directory_processed();
    });

    connect(&Settings::instance(), &Settings::rom_directory_committed,
        &watcher, &GameListWatcher::add_directory);
    connect(&Settings::instance(), &Settings::rom_directory_uncommitted,
        &watcher, &GameListWatcher::remove_directory);

    for (const auto& path : Settings::instance().rom_directories)
        watcher.add_directory(path);
}

QVariant GameListModel::data(
    const QModelIndex& index, int role
) const
{
    // might be cool in the future
    // but for now let's just focus
    // on display role
    if (!index.isValid())
        return QVariant();

    const auto& game = m_games.at(index.row());

    switch (index.column())
    {
        case COLUMN_NAME:
            if(role == Qt::DisplayRole || role == Qt::InitialSortOrderRole)
                return game.name;
            break;
        case COLUMN_TYPE:
            if (role == Qt::DisplayRole)
                return s_file_type_desc[game.type];

            // sort by the actual type and not
            // the description
            if (role == Qt::InitialSortOrderRole)
                return game.type;
            break;
        case COLUMN_SIZE:
            if(role == Qt::DisplayRole)
                return get_formatted_data_size(game.size);

            // sort by the actual size and not
            // the human readable string
            if (role == Qt::InitialSortOrderRole)
                return game.size;
            break;
    }

    return QVariant();
}

QVariant GameListModel::headerData(
    int section, Qt::Orientation orientation, int role
) const
{
    if (orientation == Qt::Vertical || role != Qt::DisplayRole)
        return QVariant();

    switch (section)
    {
        case COLUMN_NAME:
            return tr("Name");
        case COLUMN_TYPE:
            return tr("Type");
        case COLUMN_SIZE:
            return tr("Size");
    }

    return QVariant();
}

int GameListModel::rowCount(const QModelIndex& index) const
{
    if (index.isValid())
        return 0;

    return m_games.size();
}

int GameListModel::columnCount(const QModelIndex& index) const
{
    return COLUMN_MAX;
}

GameInfo GameListModel::get_game_info(int index)
{
    return m_games[index];
}

GameListWatcher* GameListModel::get_watcher()
{
    return &watcher;
}

void GameListModel::add_game(const GameInfo game)
{
    beginInsertRows(QModelIndex(), m_games.size(), m_games.size());
    m_games.push_back(game);
    endInsertRows();
}

void GameListModel::remove_game(const GameInfo game)
{
    auto index = m_games.indexOf(game);

    beginRemoveRows(QModelIndex(), index, index);
    m_games.removeAt(index);
    endRemoveRows();
}

QString GameListModel::get_formatted_data_size(uint64_t size) const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    // work around until ubuntu gets a new qt
    // don't rely on this function for translations
    const char* const quantifiers[] = {
        "B",
        "KiB",
        "MiB",
        "GiB",
        "TiB",
        "PiB",
        "EiB"
    };
    const int unit = std::log2(std::max<uint64_t>(size, 1)) / 10;
    const double unit_size = size / std::pow(2, unit * 10);

    return QString("%1 %2")
        .arg(unit_size, 4, 'f', 2, '0')
        .arg(quantifiers[unit]);
#else
    return QLocale().formattedDataSize(size);
#endif
}

#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QLocale>
#include <QDirIterator>
#include <QPushButton>
#include <QVBoxLayout>

#include "gamelistwidget.hpp"
#include "settings.hpp"

#include <cmath>
#include <algorithm>

GameListModel::GameListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    for (auto& path : Settings::instance().rom_directories)
    {
        games.append(get_directory_entries(path));
    }

    sort_games();

    connect(&Settings::instance(), &Settings::rom_directory_committed,
        this, &GameListModel::add_path
    );

    connect(&Settings::instance(), &Settings::rom_directory_uncommitted,
        this, &GameListModel::remove_path
    );
}

QVariant GameListModel::data(
    const QModelIndex& index, int role
) const
{
    // might be cool in the future
    // but for now let's just focus
    // on display role
    if (role != Qt::DisplayRole)
        return QVariant();

    auto file = games.at(index.row());
    QFileInfo file_info(file);

    switch (index.column())
    {
        case COLUMN_NAME:
            return file_info.fileName();
        case COLUMN_SIZE:
            return get_formatted_data_size(file_info.size());
    }

    return QVariant();
}

QVariant GameListModel::headerData(
    int section, Qt::Orientation orientation, int role
) const
{
    switch (section)
    {
        case COLUMN_NAME:
            return tr("Name");
        case COLUMN_SIZE:
            return tr("Size");
    }

    return QVariant();
}

int GameListModel::rowCount(const QModelIndex& index) const
{
    return games.size();
}

int GameListModel::columnCount(const QModelIndex& index) const
{
    return 2;
}

QStringList GameListModel::get_directory_entries(QString path)
{
    const QStringList file_types({
        "*.iso",
        "*.cso",
        "*.chd",
        "*.elf",
        "*.gsd",
        "*.bin"
    });

    QDirIterator it(path, file_types,
        QDir::Files, QDirIterator::Subdirectories
    );

    QStringList list;
    while (it.hasNext())
    {
        it.next();
        list.append(it.filePath());
    }

    return list;
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

void GameListModel::sort_games()
{
    std::sort(games.begin(), games.end(), [&](const QString& file1, const QString& file2) {
        auto file_name1 = QFileInfo(file1).fileName();
        auto file_name2 = QFileInfo(file2).fileName();

        return file_name1.compare(file_name2, Qt::CaseInsensitive) < 0;
    });
}

void GameListModel::add_path(const QString& path)
{
    QStringList new_games = get_directory_entries(path);

    beginInsertRows(QModelIndex(), 0, new_games.size() - 1);
    games.append(new_games);

    sort_games();

    games.removeDuplicates();
    endInsertRows();
}

void GameListModel::remove_path(const QString& path)
{
    QStringList remove_games = get_directory_entries(path);

    for (auto& game : remove_games)
    {
        int index = games.indexOf(game);
        if (index == -1)
            continue;

        beginRemoveRows(QModelIndex(), index, index);
        games.removeAll(game);
        endRemoveRows();
    }
}

GameListWidget::GameListWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    auto game_list_widget = new QTableView(this);
    auto game_list_model = new GameListModel(this);

    game_list_widget->setModel(game_list_model);
    game_list_widget->setShowGrid(false);
    game_list_widget->setFrameStyle(QFrame::NoFrame);
    game_list_widget->setAlternatingRowColors(true);
    game_list_widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    game_list_widget->setSelectionBehavior(QAbstractItemView::SelectRows);
    game_list_widget->verticalHeader()->setDefaultSectionSize(40);

    auto header = game_list_widget->horizontalHeader();
    header->setMinimumSectionSize(40);
    header->setSectionResizeMode(GameListModel::COLUMN_NAME, QHeaderView::Stretch);

    connect(game_list_widget, &QTableView::doubleClicked, [=](const QModelIndex& index) {
        QString path = game_list_model->games.at(index.row());
        emit game_double_clicked(path);
    });

    auto default_label = new QLabel(this);
    default_label->setAlignment(Qt::AlignHCenter);
    default_label->setText(tr(
        "No roms found.\n"
        "Add a new rom directory to see your roms listed here."
    ));

    auto default_button = new QPushButton("&Open Settings", this);
    connect(default_button, &QPushButton::clicked, [=]() {
        emit settings_requested();
    });

    auto layout = new QVBoxLayout;
    layout->addWidget(default_label);
    layout->addWidget(default_button);
    layout->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    auto default_widget = new QWidget(this);
    default_widget->setLayout(layout);

    addWidget(default_widget);
    addWidget(game_list_widget);

    if (game_list_widget->model()->rowCount())
        show_gamelist_view();

    connect(&Settings::instance(), &Settings::rom_directory_committed,
        this, &GameListWidget::show_gamelist_view
    );

    connect(&Settings::instance(), &Settings::rom_directory_uncommitted, [=]() {
        if (!game_list_widget->model()->rowCount())
            show_default_view();
    });
}

void GameListWidget::show_default_view()
{
    setCurrentIndex(VIEW::DEFAULT);
}

void GameListWidget::show_gamelist_view()
{
    setCurrentIndex(VIEW::GAMELIST);
}
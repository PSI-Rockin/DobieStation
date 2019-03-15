#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QDebug>
#include <QDirIterator>

#include "gamelistwidget.hpp"
#include "settings.hpp"

GameListModel::GameListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    for (auto& path : Settings::instance().rom_directories)
    {
        games.append(get_directory_entries(path));
    }

    games.sort(Qt::CaseInsensitive);
    games.removeDuplicates();
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
        return file_info.size();
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
    QDirIterator it(
        path, QStringList({"*.iso", "*.cso"}),
        QDir::Files, QDirIterator::Subdirectories
    );

    QStringList list;
    while (it.hasNext())
    {
        if(!it.filePath().isEmpty())
            list.append(it.filePath());

        it.next();
    }

    return list;
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

    auto default_widget = new QLabel(this);
    default_widget->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    default_widget->setText(tr(
        "No roms found.\n"
        "Configure rom directories in settings or "
        "open a rom from the file menu."
    ));

    addWidget(default_widget);
    addWidget(game_list_widget);

    if (game_list_widget->model()->rowCount())
    {
        show_gamelist_view();
    }
}

void GameListWidget::show_default_view()
{
    setCurrentIndex(0);
}

void GameListWidget::show_gamelist_view()
{
    setCurrentIndex(1);
}
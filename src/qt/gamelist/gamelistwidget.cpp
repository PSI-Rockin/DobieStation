#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QVBoxLayout>

#include "settings.hpp"
#include "gamelist/gamelistwidget.hpp"
#include "gamelist/gamelistmodel.hpp"

#include <cmath>
#include <algorithm>

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
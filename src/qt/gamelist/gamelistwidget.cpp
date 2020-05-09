#include <QLabel>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDebug>
#include <QLineEdit>

#include "../settings.hpp"
#include "gamelistwidget.hpp"

#include <cmath>
#include <algorithm>

GameListWidget::GameListWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    m_table_view = new QTableView(this);
    m_model = new GameListModel(this);
    m_proxy = new GameListProxy(this);

    m_proxy->setSourceModel(m_model);
    m_table_view->setModel(m_proxy);

    m_table_view->setShowGrid(false);
    m_table_view->setFrameStyle(QFrame::NoFrame);
    m_table_view->setAlternatingRowColors(true);
    m_table_view->setSortingEnabled(true);
    m_table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table_view->verticalHeader()->setDefaultSectionSize(40);

    auto header = m_table_view->horizontalHeader();
    header->setMinimumSectionSize(40);
    header->setSectionResizeMode(GameListModel::COLUMN_NAME, QHeaderView::Stretch);

    auto default_label = new QLabel(this);
    default_label->setAlignment(Qt::AlignHCenter);
    default_label->setText(tr(
        "No roms found.\n"
        "Add a new rom directory to see your roms listed here."
    ));

    auto default_button = new QPushButton("&Open Settings", this);

    auto default_layout = new QVBoxLayout;
    default_layout->addWidget(default_label);
    default_layout->addWidget(default_button);
    default_layout->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    auto gamelist_layout = new QVBoxLayout;
    auto search_bar = new QLineEdit(this);
    gamelist_layout->addWidget(search_bar);
    gamelist_layout->addWidget(m_table_view);
    gamelist_layout->setContentsMargins(0, 0, 0, 0);

    auto default_widget = new QWidget(this);
    default_widget->setLayout(default_layout);

    auto gamelist_widget = new QWidget(this);
    gamelist_widget->setLayout(gamelist_layout);

    connect(default_button, &QPushButton::clicked, [=]() {
        emit settings_requested();
    });

    connect(m_model->get_watcher(), &GameListWatcher::directory_processed,
        this, &GameListWidget::update_view);

    connect(m_table_view, &QTableView::doubleClicked, [=](const QModelIndex& index) {
        auto proxy_index = m_proxy->mapToSource(index);

        GameInfo info = m_model->get_game_info(proxy_index.row());
        emit game_double_clicked(info.path);
    });

    connect(search_bar, &QLineEdit::textChanged,
        m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    addWidget(default_widget);
    addWidget(gamelist_widget);

    m_model->get_watcher()->start();
}

void GameListWidget::update_view()
{
    if(m_table_view->model()->rowCount() || currentIndex() != VIEW::GAMELIST)
        setCurrentIndex(VIEW::GAMELIST);
    else if(!m_table_view->model()->rowCount())
        setCurrentIndex(VIEW::DEFAULT);
}

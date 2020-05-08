#ifndef GAMELISTWIDGET_HPP
#define GAMELISTWIDGET_HPP

#include <QWidget>
#include <QStackedWidget>
#include <QTableView>

#include "gamelistmodel.hpp"
#include "gamelistproxy.hpp"

class GameListWidget : public QStackedWidget
{
    Q_OBJECT
    private:
        GameListModel* m_model;
        GameListProxy* m_proxy;
        QTableView* m_table_view;
    public:
        enum VIEW
        {
            DEFAULT,
            GAMELIST
        };
        GameListWidget(QWidget* parent = nullptr);
    public slots:
        void update_view();
    signals:
        void game_double_clicked(QString path);
        void settings_requested();
};
#endif

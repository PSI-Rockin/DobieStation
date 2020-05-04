#ifndef GAMELISTWIDGET_HPP
#define GAMELISTWIDGET_HPP

#include <QWidget>
#include <QStackedWidget>

class GameListWidget : public QStackedWidget
{
    Q_OBJECT
    public:
        enum VIEW
        {
            DEFAULT,
            GAMELIST
        };
        GameListWidget(QWidget* parent = nullptr);

        void show_default_view();
        void show_gamelist_view();
    signals:
        void game_double_clicked(QString path);
        void settings_requested();
};
#endif
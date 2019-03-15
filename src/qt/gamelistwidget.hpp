#ifndef GAMELISTWIDGET_HPP
#define GAMELISTWIDGET_HPP

#include <QWidget>
#include <QStackedWidget>
#include <QAbstractTableModel>

class GameListModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum
    {
        COLUMN_NAME,
        COLUMN_SIZE
    };

    QStringList games;

    explicit GameListModel(QObject* parent = nullptr);

    QVariant data(const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation,
        int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex& index) const override;
    int columnCount(const QModelIndex& index) const override;
private:
    QStringList get_directory_entries(QString path);
};

class GameListWidget : public QStackedWidget
{
    Q_OBJECT
public:
    GameListWidget(QWidget* parent = nullptr);

    void show_default_view();
    void show_gamelist_view();
signals:
    void game_double_clicked(QString path);
};
#endif
#ifndef GAMELISTPROXY_HPP
#define GAMELISTPROXY_HPP

#include <QSortFilterProxyModel>

class GameListProxy final : public QSortFilterProxyModel
{
    Q_OBJECT

    public:
        explicit GameListProxy(QObject* parent = nullptr);

    protected:
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
#endif
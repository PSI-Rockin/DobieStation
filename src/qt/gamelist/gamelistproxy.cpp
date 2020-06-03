#include "gamelistproxy.hpp"
#include "gamelistmodel.hpp"

GameListProxy::GameListProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortRole(Qt::InitialSortOrderRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool GameListProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (left.data(Qt::InitialSortOrderRole) != right.data(Qt::InitialSortOrderRole))
        return QSortFilterProxyModel::lessThan(right, left);

    // if we are sorting by some other field than name
    // and the field is otherwise equal, then sort by name
    // (IE if we are sorting by type and both are ISO)
    const auto& right_index = sourceModel()->index(right.row(), GameListModel::COLUMN_NAME);
    const auto& left_index = sourceModel()->index(left.row(), GameListModel::COLUMN_NAME);

    const auto& right_title = right_index.data().toString();
    const auto& left_title = left_index.data().toString();

    if (sortOrder() == Qt::AscendingOrder)
        return left_title < right_title;

    return right_title < left_title;
}

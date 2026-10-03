#include "grid_content_model.h"

#include "comic_model.h"
#include "folder_model.h"

#include <algorithm>
#include <utility>

GridContentModel::GridContentModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GridContentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    const auto comics = comicModel ? comicModel->rowCount() : 0;
    return visibleFolderCount() + spacerCount() + comics;
}

QVariant GridContentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return { };

    if (isFolderRow(index.row())) {
        const auto sourceIndex = sourceFolderIndex(index.row());
        switch (role) {
        case ItemKindRole:
            return FolderItem;
        case SourceIndexRole:
            return sourceIndex.row();
        case TitleRole:
        case FileNameRole:
            return sourceIndex.data(FolderModel::FolderNameRole);
        case IdRole:
            return sourceIndex.data(FolderModel::IdRole);
        case CoverPathRole:
            return sourceIndex.data(FolderModel::CoverPathRole);
        case AddedRole:
            return sourceIndex.data(FolderModel::AddedRole);
        case TypeRole:
            return sourceIndex.data(FolderModel::TypeRole);
        case ShowRecentRole:
            return sourceIndex.data(FolderModel::ShowRecentRole);
        case RecentRangeRole:
            return sourceIndex.data(FolderModel::RecentRangeRole);
        case UpdatedRole:
            return sourceIndex.data(FolderModel::UpdatedRole);
        case FinishedRole:
            return sourceIndex.data(FolderModel::FinishedRole);
        default:
            return { };
        }
    }

    if (isSpacerRow(index.row())) {
        if (role == ItemKindRole)
            return SpacerItem;
        if (role == SourceIndexRole)
            return -1;
        return { };
    }

    if (!comicModel)
        return { };

    const auto sourceRow = sourceComicRow(index.row());
    const auto sourceIndex = comicModel->index(sourceRow, 0);
    switch (role) {
    case ItemKindRole:
        return ComicItem;
    case SourceIndexRole:
        return sourceRow;
    case NumberRole:
        return sourceIndex.data(ComicModel::NumberRole);
    case TitleRole:
        return sourceIndex.data(ComicModel::TitleRole);
    case FileNameRole:
        return sourceIndex.data(ComicModel::FileNameRole);
    case NumPagesRole:
        return sourceIndex.data(ComicModel::NumPagesRole);
    case IdRole:
        return sourceIndex.data(ComicModel::IdRole);
    case ReadRole:
        return sourceIndex.data(ComicModel::ReadColumnRole);
    case CurrentPageRole:
        return sourceIndex.data(ComicModel::CurrentPageRole);
    case RatingRole:
        return sourceIndex.data(ComicModel::RatingRole);
    case HasBeenOpenedRole:
        return sourceIndex.data(ComicModel::HasBeenOpenedRole);
    case CoverPathRole:
        return sourceIndex.data(ComicModel::CoverPathRole);
    case AddedRole:
        return sourceIndex.data(ComicModel::AddedRole);
    case TypeRole:
        return sourceIndex.data(ComicModel::TypeRole);
    case ShowRecentRole:
        return sourceIndex.data(ComicModel::ShowRecentRole);
    case RecentRangeRole:
        return sourceIndex.data(ComicModel::RecentRangeRole);
    default:
        return { };
    }
}

QHash<int, QByteArray> GridContentModel::roleNames() const
{
    return {
        { ItemKindRole, "item_kind" },
        { SourceIndexRole, "source_index" },
        { NumberRole, "number" },
        { TitleRole, "title" },
        { FileNameRole, "file_name" },
        { NumPagesRole, "num_pages" },
        { IdRole, "id" },
        { ReadRole, "read_column" },
        { CurrentPageRole, "current_page" },
        { RatingRole, "rating" },
        { HasBeenOpenedRole, "has_been_opened" },
        { CoverPathRole, "cover_path" },
        { AddedRole, "added_date" },
        { TypeRole, "type" },
        { ShowRecentRole, "show_recent" },
        { RecentRangeRole, "recent_range" },
        { UpdatedRole, "updated" },
        { FinishedRole, "is_finished" },
    };
}

void GridContentModel::setComicModel(ComicModel *model)
{
    if (comicModel == model)
        return;

    beginResetModel();
    comicModel = model;
    rebuildSortedOrder();
    endResetModel();
    reconnectModels();
}

void GridContentModel::setFolderModel(FolderModel *model, const QModelIndex &folderIndex)
{
    beginResetModel();
    folderModel = model;
    selectedFolderIndex = folderIndex;
    selectedFolderIsRoot = model && !folderIndex.isValid();
    rebuildSortedOrder();
    endResetModel();
    reconnectModels();
}

void GridContentModel::clearFolderModel()
{
    setFolderModel(nullptr, { });
}

void GridContentModel::setMixFoldersAndComics(bool enabled)
{
    if (mixFoldersAndComics == enabled)
        return;

    beginResetModel();
    mixFoldersAndComics = enabled;
    endResetModel();
}

void GridContentModel::setStartComicsOnNewRow(bool enabled)
{
    if (startComicsOnNewRow == enabled)
        return;

    beginResetModel();
    startComicsOnNewRow = enabled;
    endResetModel();
}

void GridContentModel::setSortByDate(bool enabled)
{
    if (sortByDate == enabled)
        return;

    sortByDate = enabled;
    resetFromSource();
}

void GridContentModel::setGridColumnCount(int columns)
{
    columns = qMax(1, columns);
    if (gridColumnCount == columns)
        return;

    const auto previousSpacerCount = spacerCount();
    gridColumnCount = columns;
    if (previousSpacerCount != spacerCount())
        resetFromSource();
}

bool GridContentModel::isFolderRow(int viewRow) const
{
    return viewRow >= 0 && viewRow < visibleFolderCount();
}

bool GridContentModel::isSpacerRow(int viewRow) const
{
    return viewRow >= visibleFolderCount() && viewRow < visibleFolderCount() + spacerCount();
}

int GridContentModel::visibleFolderCount() const
{
    const auto folders = sourceFolderCount();
    if (!mixFoldersAndComics && comicModel && comicModel->rowCount() > 0)
        return 0;
    return folders;
}

int GridContentModel::sourceComicRow(int viewRow) const
{
    const auto sourceRow = viewRow - visibleFolderCount() - spacerCount();
    if (sortByDate && sourceRow >= 0 && sourceRow < sortedComicRows.size())
        return sortedComicRows.at(sourceRow);
    return sourceRow;
}

int GridContentModel::viewRowForComicRow(int sourceRow) const
{
    if (sourceRow < 0)
        return -1;
    const auto base = visibleFolderCount() + spacerCount();
    if (sortByDate && sourceRow < comicSourceToViewRow.size())
        return base + comicSourceToViewRow.at(sourceRow);
    return base + sourceRow;
}

int GridContentModel::viewRowForComicId(qulonglong id) const
{
    if (!comicModel)
        return -1;

    const auto sourceIndex = comicModel->getIndexFromId(id);
    return sourceIndex.isValid() ? viewRowForComicRow(sourceIndex.row()) : -1;
}

int GridContentModel::viewRowForFolderId(qulonglong id) const
{
    for (auto row = 0; row < visibleFolderCount(); ++row) {
        if (data(index(row, 0), IdRole).toULongLong() == id)
            return row;
    }
    return -1;
}

QModelIndex GridContentModel::sourceFolderIndex(int viewRow) const
{
    if (!folderModel || !isFolderRow(viewRow))
        return { };
    const QModelIndex parent = selectedFolderIsRoot ? QModelIndex() : QModelIndex(selectedFolderIndex);
    const auto sourceRow = sortByDate && viewRow < sortedFolderRows.size() ? sortedFolderRows.at(viewRow) : viewRow;
    return folderModel->index(sourceRow, 0, parent);
}

Folder GridContentModel::folderAt(int viewRow) const
{
    if (!folderModel)
        return { };

    return folderModel->getFolder(sourceFolderIndex(viewRow));
}

QUrl GridContentModel::comicCoverUrlForHash(const QString &hash) const
{
    return comicModel ? comicModel->getCoverUrlPathForComicHash(hash) : QUrl();
}

void GridContentModel::reconnectModels()
{
    for (const auto &connection : std::as_const(sourceConnections))
        disconnect(connection);
    sourceConnections.clear();

    if (folderModel) {
        sourceConnections << connect(folderModel, &QAbstractItemModel::modelReset, this, &GridContentModel::resetFromSource);
        sourceConnections << connect(folderModel, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](const QModelIndex &parent, int first, int last) {
            if (parent == selectedFolderIndex && forwardsFolderRowsDirectly())
                beginInsertRows({ }, first, last);
        });
        sourceConnections << connect(folderModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent) {
            if (parent != selectedFolderIndex)
                return;
            if (forwardsFolderRowsDirectly())
                endInsertRows();
            else if (mixFoldersAndComics)
                resetFromSource();
        });
        sourceConnections << connect(folderModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &parent, int first, int last) {
            if (parent == selectedFolderIndex && forwardsFolderRowsDirectly())
                beginRemoveRows({ }, first, last);
        });
        sourceConnections << connect(folderModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent) {
            if (parent != selectedFolderIndex)
                return;
            if (forwardsFolderRowsDirectly())
                endRemoveRows();
            else if (mixFoldersAndComics)
                resetFromSource();
        });
        sourceConnections << connect(folderModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            if (visibleFolderCount() > 0 && topLeft.parent() == selectedFolderIndex && bottomRight.parent() == selectedFolderIndex) {
                const auto firstSourceRow = topLeft.row();
                const auto lastSourceRow = bottomRight.row();
                const auto firstViewRow = sortByDate && firstSourceRow < folderSourceToViewRow.size() ? folderSourceToViewRow.at(firstSourceRow) : firstSourceRow;
                const auto lastViewRow = sortByDate && lastSourceRow < folderSourceToViewRow.size() ? folderSourceToViewRow.at(lastSourceRow) : lastSourceRow;
                if (firstViewRow < 0 || lastViewRow < 0)
                    return;
                emit dataChanged(index(qMin(firstViewRow, lastViewRow)), index(qMax(firstViewRow, lastViewRow)));
            }
        });
    }

    if (comicModel) {
        sourceConnections << connect(comicModel, &QAbstractItemModel::modelReset, this, &GridContentModel::resetFromSource);
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](const QModelIndex &parent, int first, int last) {
            if (parent.isValid())
                return;
            if (!forwardsComicRowsDirectly())
                return;
            const auto offset = visibleFolderCount();
            beginInsertRows({ }, offset + first, offset + last);
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent) {
            if (parent.isValid())
                return;
            if (forwardsComicRowsDirectly())
                endInsertRows();
            else
                resetFromSource();
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &parent, int first, int last) {
            if (parent.isValid())
                return;
            if (!forwardsComicRowsDirectly())
                return;
            const auto offset = visibleFolderCount();
            beginRemoveRows({ }, offset + first, offset + last);
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent) {
            if (parent.isValid())
                return;
            if (forwardsComicRowsDirectly())
                endRemoveRows();
            else
                resetFromSource();
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsAboutToBeMoved, this, [this](const QModelIndex &sourceParent, int first, int last, const QModelIndex &destinationParent, int destination) {
            if (sourceParent.isValid() || destinationParent.isValid())
                return;
            if (!forwardsComicRowsDirectly())
                return;
            const auto offset = visibleFolderCount();
            beginMoveRows({ }, offset + first, offset + last, { }, offset + destination);
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &sourceParent, int, int, const QModelIndex &destinationParent) {
            if (sourceParent.isValid() || destinationParent.isValid())
                return;
            if (forwardsComicRowsDirectly())
                endMoveRows();
            else
                resetFromSource();
        });
        sourceConnections << connect(comicModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            if (topLeft.parent().isValid() || bottomRight.parent().isValid())
                return;
            emit dataChanged(index(viewRowForComicRow(topLeft.row())), index(viewRowForComicRow(bottomRight.row())));
        });
    }
}

void GridContentModel::resetFromSource()
{
    rebuildSortedOrder();
    beginResetModel();
    endResetModel();
}

void GridContentModel::rebuildSortedOrder()
{
    sortedFolderRows.clear();
    sortedComicRows.clear();
    folderSourceToViewRow.clear();
    comicSourceToViewRow.clear();

    if (!sortByDate)
        return;

    const QModelIndex folderParent = selectedFolderIsRoot ? QModelIndex() : QModelIndex(selectedFolderIndex);

    if (folderModel) {
        const auto folders = sourceFolderCount();
        QVector<std::pair<qulonglong, int>> datedFolders;
        datedFolders.reserve(folders);
        for (auto i = 0; i < folders; ++i) {
            const auto date = folderModel->index(i, 0, folderParent).data(FolderModel::AddedRole).toULongLong();
            datedFolders.append({ date, i });
        }
        std::stable_sort(datedFolders.begin(), datedFolders.end(), [](const auto &a, const auto &b) {
            return a.first > b.first; // newest first
        });
        folderSourceToViewRow.fill(-1, folders);
        sortedFolderRows.reserve(folders);
        auto viewRow = 0;
        for (const auto &entry : std::as_const(datedFolders)) {
            sortedFolderRows.append(entry.second);
            folderSourceToViewRow[entry.second] = viewRow++;
        }
    }

    if (comicModel) {
        const auto comics = comicModel->rowCount();
        QVector<std::pair<qulonglong, int>> datedComics;
        datedComics.reserve(comics);
        for (auto i = 0; i < comics; ++i) {
            const auto date = comicModel->index(i, 0).data(ComicModel::AddedRole).toULongLong();
            datedComics.append({ date, i });
        }
        std::stable_sort(datedComics.begin(), datedComics.end(), [](const auto &a, const auto &b) {
            return a.first > b.first; // newest first
        });
        comicSourceToViewRow.fill(-1, comics);
        sortedComicRows.reserve(comics);
        auto viewRow = 0;
        for (const auto &entry : std::as_const(datedComics)) {
            sortedComicRows.append(entry.second);
            comicSourceToViewRow[entry.second] = viewRow++;
        }
    }
}

int GridContentModel::sourceFolderCount() const
{
    if (!folderModel)
        return 0;
    if (selectedFolderIsRoot)
        return folderModel->rowCount();
    return selectedFolderIndex.isValid() ? folderModel->rowCount(selectedFolderIndex) : 0;
}

int GridContentModel::spacerCount() const
{
    const auto folders = visibleFolderCount();
    const auto comics = comicModel ? comicModel->rowCount() : 0;
    if (!mixFoldersAndComics || !startComicsOnNewRow || folders == 0 || comics == 0)
        return 0;

    return (gridColumnCount - (folders % gridColumnCount)) % gridColumnCount;
}

bool GridContentModel::forwardsFolderRowsDirectly() const
{
    if (sortByDate)
        return false;

    const auto comics = comicModel ? comicModel->rowCount() : 0;
    return comics == 0 || (mixFoldersAndComics && !startComicsOnNewRow);
}

bool GridContentModel::forwardsComicRowsDirectly() const
{
    if (sortByDate)
        return false;

    return mixFoldersAndComics && !startComicsOnNewRow;
}

#ifndef GRID_CONTENT_MODEL_H
#define GRID_CONTENT_MODEL_H

#include <QAbstractListModel>
#include <QPersistentModelIndex>
#include <QUrl>
#include <QVector>

class ComicModel;
class FolderModel;
class Folder;

class GridContentModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemKind {
        FolderItem = 0,
        ComicItem,
        SpacerItem
    };
    Q_ENUM(ItemKind)

    enum Roles {
        ItemKindRole = Qt::UserRole + 1,
        SourceIndexRole,
        NumberRole,
        TitleRole,
        FileNameRole,
        NumPagesRole,
        IdRole,
        ReadRole,
        CurrentPageRole,
        RatingRole,
        HasBeenOpenedRole,
        CoverPathRole,
        AddedRole,
        TypeRole,
        ShowRecentRole,
        RecentRangeRole,
        UpdatedRole,
        FinishedRole
    };

    explicit GridContentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setComicModel(ComicModel *model);
    void setFolderModel(FolderModel *model, const QModelIndex &selectedFolderIndex);
    void clearFolderModel();
    void setMixFoldersAndComics(bool enabled);
    void setStartComicsOnNewRow(bool enabled);
    void setSortByDate(bool enabled);
    void setGridColumnCount(int columns);

    bool isFolderRow(int viewRow) const;
    bool isSpacerRow(int viewRow) const;
    int visibleFolderCount() const;
    int sourceComicRow(int viewRow) const;
    int viewRowForComicRow(int sourceRow) const;
    int viewRowForComicId(qulonglong id) const;
    int viewRowForFolderId(qulonglong id) const;
    QModelIndex sourceFolderIndex(int viewRow) const;
    Folder folderAt(int viewRow) const;
    Q_INVOKABLE QUrl comicCoverUrlForHash(const QString &hash) const;

private:
    void reconnectModels();
    void resetFromSource();
    void rebuildSortedOrder();
    int sourceFolderCount() const;
    int spacerCount() const;
    bool forwardsFolderRowsDirectly() const;
    bool forwardsComicRowsDirectly() const;

    ComicModel *comicModel = nullptr;
    FolderModel *folderModel = nullptr;
    QPersistentModelIndex selectedFolderIndex;
    bool selectedFolderIsRoot = false;
    bool mixFoldersAndComics = true;
    bool startComicsOnNewRow = false;
    bool sortByDate = false;
    int gridColumnCount = 1;
    QList<QMetaObject::Connection> sourceConnections;

    // Sorting by date (creation date) caches. When `sortByDate` is false these are
    // left empty and the identity mapping is used.
    QVector<int> sortedFolderRows; // view row -> source folder row
    QVector<int> sortedComicRows; // view row (within the comics section) -> source comic row
    QVector<int> folderSourceToViewRow; // source folder row -> view row
    QVector<int> comicSourceToViewRow; // source comic row -> view row (within the comics section)
};

#endif // GRID_CONTENT_MODEL_H

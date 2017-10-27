#ifndef SEARCHMANAGER_H
#define SEARCHMANAGER_H

#include "shared/graph/elementid.h"
#include "shared/graph/elementid_containers.h"

#include <QObject>

class GraphModel;

class SearchManager : public QObject
{
    Q_OBJECT
public:
    explicit SearchManager(const GraphModel& graphModel);

    void findNodes(const QString& regex, std::vector<QString> attributeNames = {});
    void clearFoundNodeIds();
    void refresh();

    const NodeIdSet& foundNodeIds() const { return _foundNodeIds; }
    bool nodeWasFound(NodeId nodeId) const;

    bool active() const { return !_regex.isEmpty(); }

private:
    QString _regex;
    std::vector<QString> _attributeNames;

    const GraphModel* _graphModel = nullptr;
    NodeIdSet _foundNodeIds;

signals:
    void foundNodeIdsChanged(const SearchManager*);
};

#endif // SEARCHMANAGER_H

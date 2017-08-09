#include "basegenericplugin.h"

#include "shared/loading/gmlfileparser.h"
#include "shared/loading/pairwisetxtfileparser.h"
#include "shared/loading/graphmlparser.h"

#include <QJsonArray>
#include <QJsonDocument>

BaseGenericPluginInstance::BaseGenericPluginInstance()
{
    connect(this, SIGNAL(loadSuccess()), this, SLOT(onLoadSuccess()));
    connect(this, SIGNAL(selectionChanged(const ISelectionManager*)),
            this, SLOT(onSelectionChanged(const ISelectionManager*)), Qt::DirectConnection);
}

void BaseGenericPluginInstance::initialise(const IPlugin* plugin, IGraphModel* graphModel, ISelectionManager* selectionManager,
                                           ICommandManager* commandManager, const IParserThread* parserThread)
{
    BasePluginInstance::initialise(plugin, graphModel, selectionManager, commandManager, parserThread);

    _userNodeData.initialise(graphModel->mutableGraph());
    _nodeAttributeTableModel.initialise(selectionManager, graphModel, &_userNodeData);
}

std::unique_ptr<IParser> BaseGenericPluginInstance::parserForUrlTypeName(const QString& urlTypeName)
{
    if(urlTypeName == "GML")
        return std::make_unique<GmlFileParser>(&_userNodeData);
    else if(urlTypeName == "PairwiseTXT")
        return std::make_unique<PairwiseTxtFileParser>(this, &_userNodeData);
    else if(urlTypeName == "GraphML")
        return std::make_unique<GraphMLParser>(&_userNodeData);

    return nullptr;
}

void BaseGenericPluginInstance::setEdgeWeight(EdgeId edgeId, float weight)
{
    if(_edgeWeights == nullptr)
    {
        _edgeWeights = std::make_unique<EdgeArray<float>>(graphModel()->mutableGraph());

        graphModel()->createAttribute(tr("Edge Weight"))
            .setFloatValueFn([this](EdgeId edgeId_) { return _edgeWeights->get(edgeId_); })
            .setFlag(AttributeFlag::AutoRangeMutable)
            .setDescription(tr("The Edge Weight is a generic value associated with the edge."))
            .setUserDefined(true);
    }

    _edgeWeights->set(edgeId, weight);
}

QByteArray BaseGenericPluginInstance::save(IMutableGraph& graph, const ProgressFn& progressFn) const
{
    int i = 0;
    QJsonObject jsonObject;

    if(_edgeWeights != nullptr)
    {
        graph.setPhase(QObject::tr("Edge Weights"));
        QJsonArray weights;
        for(auto edgeId : graph.edgeIds())
        {
            QJsonObject weight;
            weight["id"] = QString::number(edgeId);
            weight["weight"] = _edgeWeights->at(edgeId);

            weights.append(weight);
            progressFn((i++ * 100) / graph.numEdges());
        }

        jsonObject["edgeWeights"] = weights;
    }

    progressFn(-1);

    jsonObject["userNodeData"] = _userNodeData.save(graph, progressFn);

    QJsonDocument jsonDocument(jsonObject);
    return jsonDocument.toJson();
}

bool BaseGenericPluginInstance::load(const QByteArray& data, int dataVersion,
                                     IMutableGraph& graph, const ProgressFn& progressFn)
{
    if(dataVersion != plugin()->dataVersion())
        return false;

    QJsonDocument jsonDocument = QJsonDocument::fromJson(data);

    if(jsonDocument.isNull() || !jsonDocument.isObject())
        return false;

    const auto& jsonObject = jsonDocument.object();

    if(jsonObject.contains("edgeWeights") && jsonObject["edgeWeights"].isArray())
    {
        const auto& jsonEdgeWeights = jsonObject["edgeWeights"].toArray();

        int i = 0;

        graph.setPhase(QObject::tr("Edge Weights"));
        for(const auto& edgeWeight : jsonEdgeWeights)
        {
            auto id = edgeWeight.toObject()["id"].toInt();
            auto weight = edgeWeight.toObject()["weight"].toDouble();

            setEdgeWeight(id, weight);

            progressFn((i++ * 100) / jsonEdgeWeights.size());
        }
    }

    progressFn(-1);

    if(!jsonObject.contains("userNodeData") || !jsonObject["userNodeData"].isObject())
        return false;

    const auto& jsonUserNodeData = jsonObject["userNodeData"].toObject();

    if(!_userNodeData.load(jsonUserNodeData, progressFn))
        return false;

    return true;
}

QString BaseGenericPluginInstance::selectedNodeNames() const
{
    QString s;

    for(auto nodeId : selectionManager()->selectedNodes())
    {
        if(!s.isEmpty())
            s += ", ";

        s += graphModel()->nodeName(nodeId);
    }

    return s;
}

void BaseGenericPluginInstance::onLoadSuccess()
{
    _userNodeData.setNodeNamesToFirstUserDataVector(*graphModel());
    _userNodeData.exposeAsAttributes(*graphModel());
    _nodeAttributeTableModel.updateRoleNames();
}

void BaseGenericPluginInstance::onSelectionChanged(const ISelectionManager*)
{
    emit selectedNodeNamesChanged();
    _nodeAttributeTableModel.onSelectionChanged();
}

BaseGenericPlugin::BaseGenericPlugin()
{
    registerUrlType("GML", QObject::tr("GML File"), QObject::tr("GML Files"), {"gml"});
    registerUrlType("PairwiseTXT", QObject::tr("Pairwise Text File"), QObject::tr("Pairwise Text Files"), {"txt", "layout"});
    registerUrlType("GraphML", QObject::tr("GraphML File"), QObject::tr("GraphML Files"), {"graphml"});

}

QStringList BaseGenericPlugin::identifyUrl(const QUrl& url) const
{
    //FIXME actually look at the file contents
    return identifyByExtension(url);
}

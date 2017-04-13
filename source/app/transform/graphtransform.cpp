#include "graphtransform.h"
#include "transformedgraph.h"

#include "graph/graph.h"

bool GraphTransform::applyFromSource(const Graph& source, TransformedGraph& target) const
{
    target.cloneFrom(source);
    return applyAndUpdate(target);
}

bool GraphTransform::applyAndUpdate(TransformedGraph& target) const
{
    bool anyChange = false;
    bool change = false;

    do
    {
        change = apply(target);
        anyChange = anyChange || change;
        target.update();
    } while(repeating() && change);

    return anyChange;
}

bool GraphTransform::hasUnknownAttributes(const std::vector<QString>& referencedAttributes,
                                          const std::vector<QString>& availableAttributes) const
{
    bool unknownAttributes = false;

    for(const auto& referencedAttributeName : referencedAttributes)
    {
        if(!u::contains(availableAttributes, referencedAttributeName))
        {
            addAlert(AlertType::Error, QObject::tr(R"(Unknown Attribute: "%1")").arg(referencedAttributeName));
            unknownAttributes = true;
        }
    }

    return unknownAttributes;
}

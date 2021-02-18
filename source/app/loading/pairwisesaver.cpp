/* Copyright © 2013-2020 Graphia Technologies Ltd.
 *
 * This file is part of Graphia.
 *
 * Graphia is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Graphia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Graphia.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pairwisesaver.h"

#include "shared/attributes/iattribute.h"
#include "shared/graph/igraph.h"
#include "shared/graph/igraphmodel.h"
#include "shared/graph/imutablegraph.h"
#include "ui/document.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

bool PairwiseSaver::save()
{
    QFile file(_url.toLocalFile());
    file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);

    QTextStream stream(&file);
    int edgeCount = _graphModel->graph().numEdges();
    int runningCount = 0;

    auto escape = [](QString string)
    {
        string.replace(QStringLiteral(R"(")"), QStringLiteral(R"(\")"));
        return string;
    };

    _graphModel->mutableGraph().setPhase(QObject::tr("Edges"));
    for(auto edgeId : _graphModel->graph().edgeIds())
    {
        const auto& edge = _graphModel->graph().edgeById(edgeId);
        auto sourceName = escape(_graphModel->nodeName(edge.sourceId()));
        auto targetName = escape(_graphModel->nodeName(edge.targetId()));

        if(sourceName.isEmpty())
            sourceName = QString::number(static_cast<int>(edge.sourceId()));

        if(targetName.isEmpty())
            targetName = QString::number(static_cast<int>(edge.targetId()));

        if(_graphModel->attributeExists(QStringLiteral("Edge Weight")) &&
           _graphModel->attributeByName(QStringLiteral("Edge Weight"))->valueType() & ValueType::Numerical)
        {
            const auto* attribute = _graphModel->attributeByName(QStringLiteral("Edge Weight"));
            stream << QStringLiteral(R"("%1")").arg(sourceName) << " "
                   << QStringLiteral(R"("%1")").arg(targetName) << " " << attribute->floatValueOf(edgeId)
                   << "\n";
        }
        else
        {
            stream << QStringLiteral(R"("%1")").arg(sourceName) << " "
                   << QStringLiteral(R"("%1")").arg(targetName) << "\n";
        }

        runningCount++;
        setProgress(runningCount * 100 / edgeCount);
    }

    return true;
}

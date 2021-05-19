/* Copyright © 2013-2021 Graphia Technologies Ltd.
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

#include "importattributeskeydetection.h"

#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include "ui/document.h"

ImportAttributesKeyDetection::ImportAttributesKeyDetection()
{
    connect(&_watcher, &QFutureWatcher<void>::started, this, &ImportAttributesKeyDetection::busyChanged);
    connect(&_watcher, &QFutureWatcher<void>::finished, this, &ImportAttributesKeyDetection::busyChanged);
}

ImportAttributesKeyDetection::~ImportAttributesKeyDetection()
{
    _watcher.waitForFinished();
}

void ImportAttributesKeyDetection::start()
{
    uncancel();

    QFuture<void> future = QtConcurrent::run([this]
    {
        QString bestAttributeName;
        size_t bestColumnIndex = 0;
        int bestPercent = 0;

        auto attributeNames = _document->availableAttributeNames(
            static_cast<int>(ElementType::All), static_cast<int>(ValueType::String));

        auto typeIdentities = _tabularData->typeIdentities();

        for(size_t columnIndex = 0; columnIndex < _tabularData->numColumns(); columnIndex++)
        {
            if(typeIdentities.at(columnIndex).type() != TypeIdentity::Type::String)
                continue;

            for(const auto& attributeName: attributeNames)
            {
                auto values = _document->allAttributeValues(attributeName);
                auto percent = _tabularData->columnMatchPercentage(columnIndex, values);

                // If we already have an equivalent match, prefer the one with the shorter attribute name
                if(percent == bestPercent && attributeName.size() > bestAttributeName.size())
                    continue;

                // If we already have an equivalent match, prefer the earlier column
                if(percent == bestPercent && columnIndex > bestColumnIndex)
                    continue;

                if(percent >= bestPercent)
                {
                    bestAttributeName = attributeName;
                    bestColumnIndex = columnIndex;
                    bestPercent = percent;
                }

                // Can't improve on 100%!
                if(bestPercent >= 100 || cancelled())
                    break;
            }

            if(bestPercent >= 100 || cancelled())
                break;
        }

        _result.clear();

        if(!cancelled())
        {
            _result.insert(QStringLiteral("attributeName"), bestAttributeName);
            _result.insert(QStringLiteral("column"), static_cast<int>(bestColumnIndex));
            _result.insert(QStringLiteral("percent"), bestPercent);
        }

        emit resultChanged();
    });

    _watcher.setFuture(future);
}

void ImportAttributesKeyDetection::reset()
{
    _attributeValues = {};
    _result = {};
    emit resultChanged();
}

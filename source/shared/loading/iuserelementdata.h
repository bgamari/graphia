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

#ifndef IUSERELEMENTDATA_H
#define IUSERELEMENTDATA_H

#include "shared/loading/iuserdata.h"

#include "shared/graph/elementid.h"

#include <QVariant>

class QString;

template<typename E>
class IUserElementData : public virtual IUserData
{
public:
    virtual ~IUserElementData() = default;

    virtual QVariant valueBy(E elementId, const QString& name) const = 0;
    virtual void setValueBy(E elementId, const QString& name, const QString& value) = 0;
};

using IUserNodeData = IUserElementData<NodeId>;
using IUserEdgeData = IUserElementData<EdgeId>;

#endif // IUSERELEMENTDATA_H

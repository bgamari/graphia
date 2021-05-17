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

import QtQuick 2.7
import QtQuick.Controls 1.5
import QtQuick.Layouts 1.3

import SortFilterProxyModel 0.2

PluginContent
{
    id: root

    anchors.fill: parent

    Action
    {
        id: resizeColumnsToContentsAction
        text: qsTr("&Resize Columns To Contents")
        iconName: "auto-column-resize"
        onTriggered:
        {
            tableView.resizeColumnsToContents();
        }
    }

    Action
    {
        id: selectColumnsAction
        text: qsTr("&Select Visible Columns")
        iconName: "column-select"
        checkable: true
        checked: tableView.columnSelectionMode

        onTriggered: { tableView.columnSelectionMode = !tableView.columnSelectionMode; }
    }

    function createMenu(index, menu)
    {
        switch(index)
        {
        case 0:
            tableView.populateTableMenu(menu);
            return true;
        }

        return false;
    }

    toolStrip: ToolBar
    {
        RowLayout
        {
            anchors.fill: parent

            ToolButton { action: resizeColumnsToContentsAction }
            ToolButton { action: selectColumnsAction }
            ToolButton { action: tableView.exportAction }

            Item { Layout.fillWidth: true }
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        NodeAttributeTableView
        {
            id: tableView

            Layout.fillWidth: true
            Layout.fillHeight: true

            model: plugin.model.nodeAttributeTableModel

            onSelectedRowsChanged:
            {
                // If the tableView's selection is less than complete, highlight
                // the corresponding nodes in the graph, otherwise highlight nothing
                plugin.model.highlightedRows = tableView.selectedRows.length < rowCount ?
                    tableView.selectedRows : [];
            }

            onSortIndicatorColumnChanged: { root.saveRequired = true; }
            onSortIndicatorOrderChanged: { root.saveRequired = true; }
        }
    }

    function initialise()
    {
        tableView.initialise();
    }

    property bool saveRequired: false

    function save()
    {
        let data =
        {
            "sortColumn": tableView.sortIndicatorColumn,
            "sortOrder": tableView.sortIndicatorOrder,
            "hiddenColumns": tableView.hiddenColumns,
            "columnOrder": tableView.columnOrder
        };

        return data;
    }

    function load(data, version)
    {
        if(version < 2)
        {
            if(data.sortColumn !== undefined && Number.isInteger(data.sortColumn) && data.sortColumn >= 0)
                data.sortColumn = tableView.model.columnNameFor(data.sortColumn);
            else
                data.sortColumn = "";
        }

        if(data.sortColumn !== undefined)               tableView.sortIndicatorColumn = data.sortColumn;
        if(data.sortOrder !== undefined)                tableView.sortIndicatorOrder = data.sortOrder;
        if(data.hiddenColumns !== undefined)            tableView.setHiddenColumns(data.hiddenColumns);
        if(data.columnOrder !== undefined)              tableView.columnOrder = data.columnOrder;
    }
}

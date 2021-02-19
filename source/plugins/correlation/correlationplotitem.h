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

#ifndef CORRELATIONPLOTITEM_H
#define CORRELATIONPLOTITEM_H

#include "columnannotation.h"

#include "shared/utils/qmlenum.h"

#include <qcustomplot.h>

#include <QQuickPaintedItem>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QThread>
#include <QPixmap>
#include <QOffscreenSurface>

#include <vector>
#include <set>
#include <mutex>
#include <atomic>

class CorrelationPluginInstance;

DEFINE_QML_ENUM(
    Q_GADGET, PlotScaleType,
    Raw,
    Log,
    MeanCentre,
    UnitVariance,
    Pareto);

DEFINE_QML_ENUM(
    Q_GADGET, PlotAveragingType,
    Individual,
    MeanLine,
    MedianLine,
    MeanHistogram,
    IQRPlot);

DEFINE_QML_ENUM(
    Q_GADGET, PlotDispersionType,
    None,
    StdErr,
    StdDev);

DEFINE_QML_ENUM(
    Q_GADGET, PlotDispersionVisualType,
    Bars,
    Area,
    StdDev);

DEFINE_QML_ENUM(
    Q_GADGET, PlotColumnSortType,
    Natural,
    ColumnName,
    ColumnAnnotation);

enum class CorrelationPlotUpdateType
{
    None,
    Render,
    RenderAndTooltips,
    ReplotAndRenderAndTooltips,
};

class CorrelationPlotWorker : public QObject
{
    Q_OBJECT

public:
    CorrelationPlotWorker(std::recursive_mutex& mutex,
        QCustomPlot& customPlot, QCPLayer& tooltipLayer);

    bool busy() const;

    Q_INVOKABLE void setShowGridLines(bool showGridLines);
    Q_INVOKABLE void setWidth(int width);
    Q_INVOKABLE void setHeight(int height);
    Q_INVOKABLE void setXAxisRange(double min, double max);
    Q_INVOKABLE void updatePixmap(CorrelationPlotUpdateType updateType);

private:
    bool _debug = false;
    QElapsedTimer _replotTimer;

    mutable std::recursive_mutex* _mutex;
    std::atomic_bool _busy;

    QCustomPlot* _customPlot = nullptr;
    QOffscreenSurface* _surface = nullptr;
    QCPLayer* _tooltipLayer = nullptr;

    int _width = -1;
    int _height = -1;
    double _xAxisMin = 0.0;
    double _xAxisMax = 0.0;
    bool _showGridLines = true;

    std::atomic_bool _updateQueued = false;
    CorrelationPlotUpdateType _updateType = CorrelationPlotUpdateType::None;

    Q_INVOKABLE void renderPixmap();

signals:
    void busyChanged();
    void pixmapUpdated(QPixmap pixmap);
};

class CorrelationPlotItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(CorrelationPluginInstance* model MEMBER _pluginInstance WRITE setPluginInstance)
    Q_PROPERTY(double horizontalScrollPosition MEMBER _horizontalScrollPosition
        WRITE setHorizontalScrollPosition NOTIFY horizontalScrollPositionChanged)
    Q_PROPERTY(double visibleHorizontalFraction READ visibleHorizontalFraction NOTIFY visibleHorizontalFractionChanged)
    Q_PROPERTY(QVector<int> selectedRows MEMBER _selectedRows WRITE setSelectedRows)

    Q_PROPERTY(QStringList visibleColumnAnnotationNames READ visibleColumnAnnotationNames
        WRITE setVisibleColumnAnnotationNames NOTIFY plotOptionsChanged)
    Q_PROPERTY(bool canShowColumnAnnotationSelection READ canShowColumnAnnotationSelection NOTIFY heightChanged)
    Q_PROPERTY(bool columnAnnotationSelectionModeEnabled READ columnAnnotationSelectionModeEnabled
        WRITE setColumnAnnotationSelectionModeEnabled NOTIFY columnAnnotationSelectionModeEnabledChanged)

    Q_PROPERTY(int elideLabelWidth MEMBER _elideLabelWidth WRITE setElideLabelWidth)
    Q_PROPERTY(bool showColumnNames MEMBER _showColumnNames WRITE setShowColumnNames NOTIFY plotOptionsChanged)
    Q_PROPERTY(bool showGridLines MEMBER _showGridLines WRITE setShowGridLines NOTIFY plotOptionsChanged)
    Q_PROPERTY(bool showLegend MEMBER _showLegend WRITE setShowLegend NOTIFY plotOptionsChanged)
    Q_PROPERTY(int plotScaleType MEMBER _plotScaleType WRITE setPlotScaleType NOTIFY plotOptionsChanged)
    Q_PROPERTY(int plotAveragingType MEMBER _plotAveragingType WRITE setPlotAveragingType NOTIFY plotOptionsChanged)
    Q_PROPERTY(QString plotAveragingAttributeName MEMBER _plotAveragingAttributeName
        WRITE setPlotAveragingAttributeName NOTIFY plotOptionsChanged)
    Q_PROPERTY(int plotDispersionType MEMBER _plotDispersionType WRITE setPlotDispersionType NOTIFY plotOptionsChanged)
    Q_PROPERTY(int plotDispersionVisualType MEMBER _plotDispersionVisualType
        WRITE setPlotDispersionVisualType NOTIFY plotOptionsChanged)
    Q_PROPERTY(QVector<QVariantMap> columnSortOrders MEMBER _columnSortOrders
        WRITE setColumnSortOrders NOTIFY plotOptionsChanged)
    Q_PROPERTY(QString xAxisLabel MEMBER _xAxisLabel WRITE setXAxisLabel NOTIFY plotOptionsChanged)
    Q_PROPERTY(QString yAxisLabel MEMBER _yAxisLabel WRITE setYAxisLabel NOTIFY plotOptionsChanged)
    Q_PROPERTY(int xAxisPadding MEMBER _xAxisPadding WRITE setXAxisPadding NOTIFY plotOptionsChanged)
    Q_PROPERTY(bool includeYZero MEMBER _includeYZero WRITE setIncludeYZero NOTIFY plotOptionsChanged)
    Q_PROPERTY(bool showAllColumns MEMBER _showAllColumns WRITE setShowAllColumns NOTIFY plotOptionsChanged)
    Q_PROPERTY(bool isWide READ isWide NOTIFY isWideChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

    Q_PROPERTY(int minimumHeight READ minimumHeight CONSTANT)

public:
    explicit CorrelationPlotItem(QQuickItem* parent = nullptr);
    ~CorrelationPlotItem() override;

    void paint(QPainter* painter) override;

    Q_INVOKABLE void savePlotImage(const QUrl& url, const QStringList& extensions);
    Q_INVOKABLE void sortBy(int type, const QString& text = {});

    void setPlotScaleType(int plotScaleType);
    void setPlotDispersionType(int plotDispersionType);
    void setXAxisLabel(const QString& plotXAxisLabel);
    void setYAxisLabel(const QString& plotYAxisLabel);
    void setIncludeYZero(bool includeYZero);
    void setShowAllColumns(bool showAllColumns);
    void setPlotAveragingType(int plotAveragingType);
    void setPlotAveragingAttributeName(const QString& attributeName);
    void setPlotDispersionVisualType(int plotDispersionVisualType);

protected:
    void routeMouseEvent(QMouseEvent* event);
    void routeWheelEvent(QWheelEvent* event);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    enum class InvalidateCache { No, Yes };

    void rebuildPlot(InvalidateCache invalidateCache = InvalidateCache::No);

private:
    bool _debug = false;

    enum class RebuildRequired
    {
        None,
        Partial,
        Full
    };

    RebuildRequired _rebuildRequired = RebuildRequired::None;
    bool _tooltipUpdateRequired = false;
    QCPLayer* _tooltipLayer = nullptr;
    QPointF _hoverPoint{-1.0, -1.0};
    QCPItemText* _hoverLabel = nullptr;
    QCPItemRect* _hoverColorRect = nullptr;
    QCPItemTracer* _itemTracer = nullptr;
    QFont _defaultFont9Pt;
    QFontMetrics _defaultFontMetrics{_defaultFont9Pt};

    QCustomPlot _customPlot;
    QCPLayoutGrid* _mainAxisLayout = nullptr;
    QCPAxisRect* _mainAxisRect = nullptr;
    QCPAxis* _mainXAxis = nullptr;
    QCPAxis* _mainYAxis = nullptr;
    QVector<QCPAbstractPlottable*> _meanPlots;
    QCPAxisRect* _columnAnnotationsAxisRect = nullptr;
    bool _columnAnnotationSelectionModeEnabled = false;

    CorrelationPluginInstance* _pluginInstance = nullptr;

    int _elideLabelWidth = 120;
    QVector<int> _selectedRows;
    bool _showColumnNames = true;
    bool _showGridLines = true;
    bool _showLegend = false;
    int _plotScaleType = static_cast<int>(PlotScaleType::Raw);
    int _plotAveragingType = static_cast<int>(PlotAveragingType::Individual);
    QString _plotAveragingAttributeName;
    int _plotDispersionType = static_cast<int>(PlotDispersionType::None);
    int _plotDispersionVisualType = static_cast<int>(PlotDispersionVisualType::Bars);
    QVector<QVariantMap> _columnSortOrders;
    double _horizontalScrollPosition = 0.0;
    QString _xAxisLabel;
    QString _yAxisLabel;
    bool _includeYZero = false;
    bool _showAllColumns = false;
    int _xAxisPadding = 0;

    std::vector<size_t> _sortMap;

    std::set<QString> _visibleColumnAnnotationNames;
    bool _showColumnAnnotations = true;

    QCPLayer* _lineGraphLayer = nullptr;

    struct LineCacheEntry
    {
        QCPGraph* _graph = nullptr;
        double _minY = std::numeric_limits<double>::max();
        double _maxY = std::numeric_limits<double>::lowest();
    };

    QMap<int, LineCacheEntry> _lineGraphCache;

    using LabelElisionCacheEntry = QMap<int, QString>;
    QMap<QString, LabelElisionCacheEntry> _labelElisionCache;

    std::recursive_mutex _mutex;
    QThread _plotRenderThread;
    CorrelationPlotWorker* _worker = nullptr;

    QPixmap _pixmap;

    void populateMeanLinePlot();
    void populateMedianLinePlot();
    void populateLinePlot();
    void populateMeanHistogramPlot();
    void populateIQRPlot();
    void plotDispersion(QCPAbstractPlottable* meanPlot,
        double& minY, double& maxY,
        const QVector<double>& stdDevs, const QString& name);
    void populateStdDevPlot(QCPAbstractPlottable* meanPlot,
        double& minY, double& maxY,
        const QVector<int>& rows, QVector<double>& means);
    void populateStdErrorPlot(QCPAbstractPlottable* meanPlot,
        double& minY, double& maxY,
        const QVector<int>& rows, QVector<double>& means);
    void populateDispersion(QCPAbstractPlottable* meanPlot,
        double& minY, double& maxY,
        const QVector<int>& rows, QVector<double>& means);

    bool busy() const { return _worker != nullptr ? _worker->busy() : false; }

    static int minimumHeight() { return 100; }

    void setPluginInstance(CorrelationPluginInstance* pluginInstance);

    void setSelectedRows(const QVector<int>& selectedRows);
    void setElideLabelWidth(int elideLabelWidth);
    void setShowColumnNames(bool showColumnNames);
    void setShowGridLines(bool showGridLines);
    void setShowLegend(bool showLegend);
    void setHorizontalScrollPosition(double horizontalScrollPosition);
    void setXAxisPadding(int padding);

    void updateSortMap();
    void setColumnSortOrders(const QVector<QVariantMap> columnSortOrders);

    QString elideLabel(const QString& label);

    QStringList visibleColumnAnnotationNames() const;
    void setVisibleColumnAnnotationNames(const QStringList& columnAnnotations);
    bool columnAnnotationSelectionModeEnabled() const;
    void setColumnAnnotationSelectionModeEnabled(bool enabled);
    size_t numVisibleColumnAnnotations() const;
    QString columnAnnotationValueAt(size_t x, size_t y) const;

    void computeXAxisRange();
    void setYAxisRange(double min, double max);
    QVector<double> meanAverageData(double& min, double& max, const QVector<int>& rows);

    void updateColumnAnnotationVisibility();
    bool canShowColumnAnnotationSelection() const;

    double visibleHorizontalFraction() const;
    bool isWide() const;
    double labelHeight() const;
    double minColumnWidth() const;
    double columnAxisWidth() const;
    double columnAnnotaionsHeight(bool allAttributes) const;

    QCPAxis* configureColumnAnnotations(QCPAxis* xAxis);
    void configureLegend();

    void onLeftClick(const QPoint& pos);

    void updatePixmap(CorrelationPlotUpdateType updateType);

    QCPAbstractPlottable* abstractPlottableUnderCursor(double& keyCoord);

private slots:
    void onPixmapUpdated(const QPixmap& pixmap);
    void updatePlotSize();
    void updateTooltip();

signals:
    void rightClick();
    void horizontalScrollPositionChanged();
    void visibleHorizontalFractionChanged();
    void isWideChanged();
    void plotOptionsChanged();
    void busyChanged();

    void columnAnnotationSelectionModeEnabledChanged();
};
#endif // CORRELATIONPLOTITEM_H

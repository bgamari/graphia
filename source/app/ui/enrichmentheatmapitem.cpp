#include "enrichmentheatmapitem.h"
#include <set>
#include <iterator>

EnrichmentHeatmapItem::EnrichmentHeatmapItem(QQuickItem* parent) : QQuickPaintedItem(parent)
{
    setRenderTarget(RenderTarget::FramebufferObject);

    _customPlot.setOpenGl(true);
    _customPlot.addLayer(QStringLiteral("textLayer"));

    _colorMap = new QCPColorMap(_customPlot.xAxis, _customPlot.yAxis2);
    _colorScale = new QCPColorScale(&_customPlot);
    _colorScale->setLabel(tr("Fishers P-Value"));
    _colorScale->setType(QCPAxis::atBottom);
    _customPlot.plotLayout()->addElement(1, 0, _colorScale);
    _colorScale->setMinimumMargins(QMargins(6, 0, 6, 0));

    _textLayer = _customPlot.layer(QStringLiteral("textLayer"));
    _textLayer->setMode(QCPLayer::LayerMode::lmBuffered);

    _customPlot.yAxis2->setVisible(true);
    _customPlot.yAxis->setVisible(false);

    QCPColorGradient gradient;
    auto insignificantColor = QColor(Qt::gray);
    auto verySignificantColor = QColor(Qt::yellow);
    auto significantColor = QColor(Qt::red);
    gradient.setColorStopAt(0, verySignificantColor);
    gradient.setColorStopAt(5.0 / 6.0, significantColor);
    gradient.setColorStopAt(5.0 / 6.0 + 0.001, insignificantColor);
    gradient.setColorStopAt(1.0, insignificantColor);

    _colorMap->setInterpolate(false);
    _colorMap->setColorScale(_colorScale);
    _colorMap->setGradient(gradient);
    _colorMap->setTightBoundary(true);

    QFont defaultFont10Pt;
    defaultFont10Pt.setPointSize(10);

    _defaultFont9Pt.setPointSize(9);

    _hoverLabel = new QCPItemText(&_customPlot);
    _hoverLabel->setPositionAlignment(Qt::AlignVCenter|Qt::AlignLeft);
    _hoverLabel->setLayer(_textLayer);
    _hoverLabel->setFont(defaultFont10Pt);
    _hoverLabel->setPen(QPen(Qt::black));
    _hoverLabel->setBrush(QBrush(Qt::white));
    _hoverLabel->setPadding(QMargins(3, 3, 3, 3));
    _hoverLabel->setClipToAxisRect(false);
    _hoverLabel->setVisible(false);

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    setFlag(QQuickItem::ItemHasContents, true);

    connect(this, &EnrichmentHeatmapItem::tableModelChanged, this, &EnrichmentHeatmapItem::buildPlot);
    connect(this, &QQuickPaintedItem::widthChanged, this, &EnrichmentHeatmapItem::horizontalRangeSizeChanged);
    connect(this, &QQuickPaintedItem::heightChanged, this, &EnrichmentHeatmapItem::verticalRangeSizeChanged);
    connect(this, &QQuickPaintedItem::widthChanged, this, &EnrichmentHeatmapItem::updatePlotSize);
    connect(this, &QQuickPaintedItem::heightChanged, this, &EnrichmentHeatmapItem::updatePlotSize);
    connect(&_customPlot, &QCustomPlot::afterReplot, this, &EnrichmentHeatmapItem::onCustomReplot);
}

void EnrichmentHeatmapItem::paint(QPainter *painter)
{
    QPixmap picture(boundingRect().size().toSize());
    QCPPainter qcpPainter(&picture);

    _customPlot.toPainter(&qcpPainter);

    painter->drawPixmap(QPoint(), picture);
}

void EnrichmentHeatmapItem::mousePressEvent(QMouseEvent* event)
{
    routeMouseEvent(event);
    if(event->button() == Qt::MouseButton::LeftButton)
    {
        auto xCoord = static_cast<int>(std::round(_customPlot.xAxis->pixelToCoord(event->pos().x())));
        auto yCoord = static_cast<int>(std::round(_customPlot.yAxis2->pixelToCoord(event->pos().y())));
        emit plotValueClicked(_tableModel->rowFromAttributeSets(_xAxisToFullLabel[xCoord], _yAxisToFullLabel[yCoord]));
    }
}

void EnrichmentHeatmapItem::mouseReleaseEvent(QMouseEvent* event)
{
    routeMouseEvent(event);
    hideTooltip();
    if(event->button() == Qt::RightButton)
        emit rightClick();
}

void EnrichmentHeatmapItem::mouseMoveEvent(QMouseEvent* event)
{
    routeMouseEvent(event);
}

void EnrichmentHeatmapItem::hoverMoveEvent(QHoverEvent *event)
{
    _hoverPoint = event->posF();

    auto* currentPlottable = _customPlot.plottableAt(event->posF(), true);
    if(_hoverPlottable != currentPlottable)
    {
        _hoverPlottable = currentPlottable;
        hideTooltip();
    }

    if(_hoverPlottable != nullptr)
        showTooltip();
}

void EnrichmentHeatmapItem::hoverLeaveEvent(QHoverEvent *event)
{
    hideTooltip();
    Q_UNUSED(event);
}

void EnrichmentHeatmapItem::routeMouseEvent(QMouseEvent* event)
{
    auto* newEvent = new QMouseEvent(event->type(), event->localPos(),
                                     event->button(), event->buttons(),
                                     event->modifiers());
    QCoreApplication::postEvent(&_customPlot, newEvent);
}

void EnrichmentHeatmapItem::buildPlot()
{
    if(_tableModel == nullptr)
        return;

    QSharedPointer<QCPAxisTickerText> xCategoryTicker(new QCPAxisTickerText);
    QSharedPointer<QCPAxisTickerText> yCategoryTicker(new QCPAxisTickerText);

    _customPlot.xAxis->setTicker(xCategoryTicker);
    _customPlot.xAxis->setTickLabelRotation(90);
    _customPlot.yAxis2->setTicker(yCategoryTicker);

    std::set<QString> attributeValueSetA;
    std::set<QString> attributeValueSetB;
    std::map<QString, int> fullLabelToXAxis;
    std::map<QString, int> fullLabelToYAxis;

    _xAxisToFullLabel.clear();
    _yAxisToFullLabel.clear();

    for(int i = 0; i < _tableModel->rowCount(); ++i)
    {
        attributeValueSetA.insert(_tableModel->data(i, _tableModel->resultToString(EnrichmentTableModel::Results::SelectionA)).toString());
        attributeValueSetB.insert(_tableModel->data(i, _tableModel->resultToString(EnrichmentTableModel::Results::SelectionB)).toString());
    }

    // Sensible sort strings using numbers
    QCollator collator;
    collator.setNumericMode(true);
    std::vector<QString> sortAttributeValueSetA(attributeValueSetA.begin(), attributeValueSetA.end());
    std::vector<QString> sortAttributeValueSetB(attributeValueSetB.begin(), attributeValueSetB.end());
    std::sort(sortAttributeValueSetA.begin(), sortAttributeValueSetA.end(), collator);
    std::sort(sortAttributeValueSetB.begin(), sortAttributeValueSetB.end(), collator);

    QFontMetrics metrics(_defaultFont9Pt);

    int column = 0;
    for(const auto& labelName: sortAttributeValueSetA)
    {
        fullLabelToXAxis[labelName] = column;
        _xAxisToFullLabel[column] = labelName;
        if(_elideLabelWidth > 0)
            xCategoryTicker->addTick(column++, metrics.elidedText(labelName, Qt::ElideRight, _elideLabelWidth));
        else
            xCategoryTicker->addTick(column++, labelName);
    }
    column = 0;
    for(const auto& labelName: sortAttributeValueSetB)
    {
        fullLabelToYAxis[labelName] = column;
        _yAxisToFullLabel[column] = labelName;
        if(_elideLabelWidth > 0)
            yCategoryTicker->addTick(column++, metrics.elidedText(labelName, Qt::ElideRight, _elideLabelWidth));
        else
            yCategoryTicker->addTick(column++, labelName);
    }

    // WARNING: Colour maps seem to overdraw the map size, this means hover events won't be triggered on the
    // overdrawn edges. As a fix I add a 1 cell margin on all sides of the map, offset the data by 1 cell
    // and range it to match
    _colorMap->data()->setSize(static_cast<int>(attributeValueSetA.size() + 2), static_cast<int>(attributeValueSetB.size() + 2));
    _colorMap->data()->setRange(QCPRange(-1, attributeValueSetA.size()), QCPRange(-1, attributeValueSetB.size()));

    _attributeACount = static_cast<int>(attributeValueSetA.size());
    _attributeBCount = static_cast<int>(attributeValueSetB.size());

    for(int i=0; i<_tableModel->rowCount(); i++)
    {
        // The data is offset by 1 to account for the empty margin
        // Set the data of the cell
        auto xValue = fullLabelToXAxis[_tableModel->data(i, _tableModel->resultToString(EnrichmentTableModel::Results::SelectionA)).toString()];
        auto yValue = fullLabelToYAxis[_tableModel->data(i, _tableModel->resultToString(EnrichmentTableModel::Results::SelectionB)).toString()];
        _colorMap->data()->setCell(xValue + 1,
                                   yValue + 1,
                                   _tableModel->data(i, QStringLiteral("Fishers")).toFloat());

        // Ugly hack: Colors blend from margin cells. I recolour them to match adjacent cells so you can't tell
        // 200 IQ fix really...
        if(xValue == 0)
        {
            _colorMap->data()->setCell(xValue,
                                       yValue + 1,
                                       _tableModel->data(i, QStringLiteral("Fishers")).toFloat());
        }
        if(yValue == static_cast<int>(attributeValueSetB.size()) - 1)
        {
            _colorMap->data()->setCell(xValue + 1,
                                       yValue + 2,
                                       _tableModel->data(i, QStringLiteral("Fishers")).toFloat());
        }
    }
    _colorScale->setDataRange(QCPRange(0, 0.06));
}

void EnrichmentHeatmapItem::updatePlotSize()
{
    _customPlot.setGeometry(0, 0, static_cast<int>(width()), static_cast<int>(height()));
    scaleXAxis();
    scaleYAxis();
}

double EnrichmentHeatmapItem::columnAxisWidth()
{
    const auto& margins = _customPlot.axisRect()->margins();
    const unsigned int axisWidth = margins.left() + margins.right();

    return width() - axisWidth;
}

double EnrichmentHeatmapItem::columnAxisHeight()
{
    const auto& margins = _customPlot.axisRect()->margins();
    const unsigned int axisHeight = margins.top() + margins.bottom();

    return height() - axisHeight;
}

void EnrichmentHeatmapItem::scaleXAxis()
{
    auto maxX = static_cast<double>(_attributeACount);
    double visiblePlotWidth = columnAxisWidth();
    double textHeight = columnLabelSize();

    double position = (_attributeACount - (visiblePlotWidth / textHeight)) * _scrollXAmount;

    if(position + (visiblePlotWidth / textHeight) <= maxX)
        _customPlot.xAxis->setRange(position - _HEATMAP_OFFSET, position + (visiblePlotWidth / textHeight) - _HEATMAP_OFFSET);
    else
        _customPlot.xAxis->setRange(-_HEATMAP_OFFSET, maxX - _HEATMAP_OFFSET);
}

void EnrichmentHeatmapItem::scaleYAxis()
{
    auto maxY = static_cast<double>(_attributeBCount);
    double visiblePlotHeight = columnAxisHeight();
    double textHeight = columnLabelSize();

    double position = (_attributeBCount - (visiblePlotHeight / textHeight)) * (1.0-_scrollYAmount);

    if((visiblePlotHeight / textHeight) <= maxY)
        _customPlot.yAxis2->setRange(position - _HEATMAP_OFFSET, position + (visiblePlotHeight / textHeight) - _HEATMAP_OFFSET);
    else
        _customPlot.yAxis2->setRange(-_HEATMAP_OFFSET, maxY - _HEATMAP_OFFSET);
}

void EnrichmentHeatmapItem::setElideLabelWidth(int elideLabelWidth)
{
    bool changed = (_elideLabelWidth != elideLabelWidth);
    _elideLabelWidth = elideLabelWidth;

    if(changed)
    {
        updatePlotSize();
        buildPlot();
        _customPlot.replot(QCustomPlot::rpQueuedReplot);
    }
}

void EnrichmentHeatmapItem::setScrollXAmount(double scrollAmount)
{
    _scrollXAmount = scrollAmount;
    scaleXAxis();
    _customPlot.replot();
}

void EnrichmentHeatmapItem::setScrollYAmount(double scrollAmount)
{
    _scrollYAmount = scrollAmount;
    scaleYAxis();
    _customPlot.replot();
    update();
}

double EnrichmentHeatmapItem::columnLabelSize()
{
    QFontMetrics metrics(_defaultFont9Pt);
    const unsigned int columnPadding = 1;
    return metrics.height() + columnPadding;
}

double EnrichmentHeatmapItem::horizontalRangeSize()
{
    return (columnAxisWidth() / (columnLabelSize() * _attributeACount));
}

double EnrichmentHeatmapItem::verticalRangeSize()
{
    return (columnAxisHeight() / (columnLabelSize() * _attributeBCount));
}

void EnrichmentHeatmapItem::showTooltip()
{
    _hoverLabel->setVisible(true);
    double key, value;
    _colorMap->pixelsToCoords(_hoverPoint, key, value);
    _hoverLabel->setText(tr("P-value: %1").arg(QString::number(_colorMap->data()->data(key,value), 'f', 2)));

    const auto COLOR_RECT_WIDTH = 10.0;
    const auto HOVER_MARGIN = 10.0;
    auto hoverlabelWidth = _hoverLabel->right->pixelPosition().x() -
            _hoverLabel->left->pixelPosition().x();
    auto hoverlabelHeight = _hoverLabel->bottom->pixelPosition().y() -
            _hoverLabel->top->pixelPosition().y();
    auto hoverLabelRightX = _hoverPoint.x() +
            hoverlabelWidth + HOVER_MARGIN + COLOR_RECT_WIDTH;
    auto xBounds = clipRect().width();
    QPointF targetPosition(_hoverPoint.x() + HOVER_MARGIN,
                           _hoverPoint.y());

    // If it falls out of bounds, clip to bounds and move label above marker
    if(hoverLabelRightX > xBounds)
    {
        targetPosition.rx() = xBounds - hoverlabelWidth - COLOR_RECT_WIDTH - 1.0;

        // If moving the label above marker is less than 0, clip to 0 + labelHeight/2;
        if(targetPosition.y() - (hoverlabelHeight * 0.5) - HOVER_MARGIN * 2.0 < 0.0)
            targetPosition.setY(hoverlabelHeight * 0.5);
        else
            targetPosition.ry() -= HOVER_MARGIN * 2.0;
    }

    _hoverLabel->position->setPixelPosition(targetPosition);

    update();
}

void EnrichmentHeatmapItem::savePlotImage(const QUrl& url, const QStringList& extensions)
{
    if(extensions.contains(QStringLiteral("png")))
        _customPlot.savePng(url.toLocalFile());
    else if(extensions.contains(QStringLiteral("pdf")))
        _customPlot.savePdf(url.toLocalFile());
    else if(extensions.contains(QStringLiteral("jpg")))
        _customPlot.saveJpg(url.toLocalFile());

    QDesktopServices::openUrl(url);
}

void EnrichmentHeatmapItem::hideTooltip()
{
    _hoverLabel->setVisible(false);
    update();
}

void EnrichmentHeatmapItem::onCustomReplot()
{
    update();
}


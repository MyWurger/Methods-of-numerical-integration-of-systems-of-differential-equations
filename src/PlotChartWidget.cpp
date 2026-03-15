#include "PlotChartWidget.h"

#include <QGestureEvent>
#include <QFontMetricsF>
#include <QLabel>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPinchGesture>
#include <QPointer>
#include <QGraphicsLayout>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kMaxRenderedPoints = 20000;
constexpr double kMinZoomFactor = 0.75;
constexpr double kMaxZoomFactor = 1.35;
constexpr double kAxisArrowLength = 9.0;
constexpr double kAxisArrowHalfWidth = 4.5;

double NiceTickStep(double span, int targetTicks)
{
    if (!std::isfinite(span) || span <= 0.0 || targetTicks < 2)
    {
        return 1.0;
    }

    const double rawStep = span / static_cast<double>(targetTicks - 1);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double normalized = rawStep / magnitude;

    double nice = 1.0;
    if (normalized >= 7.5)
    {
        nice = 10.0;
    }
    else if (normalized >= 3.5)
    {
        nice = 5.0;
    }
    else if (normalized >= 1.5)
    {
        nice = 2.0;
    }

    return nice * magnitude;
}

int DecimalsForStep(double step)
{
    if (!std::isfinite(step) || step <= 0.0)
    {
        return 6;
    }
    if (step >= 1.0)
    {
        return 0;
    }

    const int decimals = static_cast<int>(std::ceil(-std::log10(step))) + 1;
    return std::clamp(decimals, 0, 12);
}

double StableTickStep(double previousStep, double span, int targetTicks)
{
    const double desired = NiceTickStep(span, targetTicks);
    if (!(previousStep > 0.0) || !std::isfinite(previousStep))
    {
        return desired * 0.5;
    }

    const double ratio = desired / previousStep;
    if (!std::isfinite(ratio) || ratio <= 0.0)
    {
        return desired;
    }

    const double hysteresis = 1.20;
    if (ratio > hysteresis || ratio < (1.0 / hysteresis))
    {
        return desired;
    }

    return previousStep;
}

QVector<QPointF> DownsamplePoints(const QVector<QPointF>& points)
{
    if (points.size() <= kMaxRenderedPoints)
    {
        return points;
    }

    QVector<QPointF> sampled;
    sampled.reserve(kMaxRenderedPoints);
    const double step = static_cast<double>(points.size() - 1) / static_cast<double>(kMaxRenderedPoints - 1);
    const int maxIndex = points.size() - 1;

    for (int i = 0; i < kMaxRenderedPoints - 1; ++i)
    {
        const int index = static_cast<int>(std::round(i * step));
        sampled.append(points[std::clamp(index, 0, maxIndex)]);
    }
    sampled.append(points.back());
    return sampled;
}

bool IsMonotonicByX(const QVector<QPointF>& points)
{
    for (int i = 1; i < points.size(); ++i)
    {
        if (points[i].x() < points[i - 1].x())
        {
            return false;
        }
    }
    return true;
}
}

class InteractiveChartView final : public QChartView
{
public:
    explicit InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent = nullptr)
        : QChartView(chart, parent), owner_(owner)
    {
        setDragMode(QGraphicsView::NoDrag);
        setRubberBand(QChartView::NoRubberBand);
        setMouseTracking(true);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
        grabGesture(Qt::PinchGesture);
        viewport()->grabGesture(Qt::PinchGesture);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QChartView::paintEvent(event);

        if (owner_ == nullptr || !owner_->hasData_ || chart() == nullptr)
        {
            return;
        }

        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QPen axisPen(QColor("#4b647b"), 1.35);
        painter.setPen(axisPen);

        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();

        const bool drawXAxis = (yMin <= 0.0 && yMax >= 0.0);
        const bool drawYAxis = (xMin <= 0.0 && xMax >= 0.0);

        if (drawXAxis)
        {
            DrawHorizontalAxis(&painter, xMin, xMax);
        }
        if (drawYAxis)
        {
            DrawVerticalAxis(&painter, yMin, yMax);
        }

        if (owner_->hasEventMarker_)
        {
            DrawEventMarker(&painter);
        }
        if (owner_->hasEventPoint_)
        {
            DrawEventPoint(&painter);
        }
    }

    bool viewportEvent(QEvent* event) override
    {
        if (event == nullptr || owner_ == nullptr)
        {
            return QChartView::viewportEvent(event);
        }

        if (event->type() == QEvent::NativeGesture)
        {
            auto* nativeEvent = static_cast<QNativeGestureEvent*>(event);
            if (HandleNativeGesture(nativeEvent))
            {
                return true;
            }
        }

        if (event->type() == QEvent::Gesture)
        {
            auto* gestureEvent = static_cast<QGestureEvent*>(event);
            if (HandleGesture(gestureEvent))
            {
                return true;
            }
        }

        return QChartView::viewportEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event == nullptr || owner_ == nullptr || !owner_->hasData_)
        {
            QChartView::wheelEvent(event);
            return;
        }

        if ((event->modifiers() & Qt::ControlModifier) != 0)
        {
            QChartView::wheelEvent(event);
            return;
        }

        const QPoint pixelDelta = event->pixelDelta();
        if (!pixelDelta.isNull())
        {
            owner_->PanByPixels(pixelDelta);
            event->accept();
            return;
        }

        QChartView::wheelEvent(event);
    }

private:
    void DrawHorizontalAxis(QPainter* painter, double xMin, double xMax)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr)
        {
            return;
        }

        const QPointF leftChartPos = chart()->mapToPosition(QPointF(xMin, 0.0), owner_->lineSeries_);
        const QPointF rightChartPos = chart()->mapToPosition(QPointF(xMax, 0.0), owner_->lineSeries_);
        const QPoint leftViewPos = mapFromScene(chart()->mapToScene(leftChartPos));
        const QPoint rightViewPos = mapFromScene(chart()->mapToScene(rightChartPos));

        DrawArrowHead(painter, QPointF(rightViewPos), QPointF(leftViewPos));
        DrawAxisLabel(painter,
                      owner_->xAxisLabel_,
                      QPointF(rightViewPos.x() - 16.0, rightViewPos.y() - 24.0),
                      Qt::AlignRight | Qt::AlignBottom);
    }

    void DrawVerticalAxis(QPainter* painter, double yMin, double yMax)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr)
        {
            return;
        }

        const QPointF topChartPos = chart()->mapToPosition(QPointF(0.0, yMax), owner_->lineSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(0.0, yMin), owner_->lineSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        DrawArrowHead(painter, QPointF(topViewPos), QPointF(bottomViewPos));
        DrawAxisLabel(painter,
                      owner_->yAxisLabel_,
                      QPointF(topViewPos.x() + 16.0, topViewPos.y() - 10.0),
                      Qt::AlignLeft | Qt::AlignBottom);
    }

    void DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from)
    {
        if (painter == nullptr)
        {
            return;
        }

        const QLineF direction(from, tip);
        if (qFuzzyIsNull(direction.length()))
        {
            return;
        }

        QLineF unit = direction.unitVector();
        const QPointF delta = unit.p2() - unit.p1();
        const QPointF back = tip - delta * kAxisArrowLength;
        const QPointF normal(-delta.y(), delta.x());

        const QPointF wing1 = back + normal * kAxisArrowHalfWidth;
        const QPointF wing2 = back - normal * kAxisArrowHalfWidth;

        painter->drawLine(QLineF(tip, wing1));
        painter->drawLine(QLineF(tip, wing2));
    }

    void DrawAxisLabel(QPainter* painter, const QString& text, const QPointF& anchor, Qt::Alignment alignment)
    {
        if (painter == nullptr || text.isEmpty())
        {
            return;
        }

        QFont font = painter->font();
        font.setPointSizeF(std::max(font.pointSizeF(), 10.5));
        font.setBold(true);
        painter->setFont(font);

        const QFontMetricsF metrics(font);
        QRectF textRect = metrics.boundingRect(text);
        textRect.adjust(-6.0, -3.0, 6.0, 3.0);

        QPointF topLeft = anchor;
        if (alignment & Qt::AlignRight)
        {
            topLeft.rx() -= textRect.width();
        }
        else if (alignment & Qt::AlignHCenter)
        {
            topLeft.rx() -= textRect.width() * 0.5;
        }

        if (alignment & Qt::AlignBottom)
        {
            topLeft.ry() -= textRect.height();
        }
        else if (alignment & Qt::AlignVCenter)
        {
            topLeft.ry() -= textRect.height() * 0.5;
        }

        const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(6.0, 6.0, -6.0, -6.0);
        topLeft.setX(std::clamp(topLeft.x(), viewportRect.left(), viewportRect.right() - textRect.width()));
        topLeft.setY(std::clamp(topLeft.y(), viewportRect.top(), viewportRect.bottom() - textRect.height()));

        textRect.moveTopLeft(topLeft);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 220));
        painter->drawRoundedRect(textRect, 5.0, 5.0);

        painter->setPen(QColor("#203146"));
        painter->drawText(textRect.adjusted(6.0, 3.0, -6.0, -3.0), Qt::AlignCenter, text);
        painter->setPen(QColor("#4b647b"));
    }

    void DrawEventMarker(QPainter* painter)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->lineSeries_ == nullptr)
        {
            return;
        }

        const double xValue = owner_->eventMarkerX_;
        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        if (xValue < xMin || xValue > xMax)
        {
            return;
        }

        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();
        const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->lineSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->lineSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        QPen markerPen(QColor("#d11f4e"));
        markerPen.setWidthF(1.3);
        markerPen.setStyle(Qt::DashLine);
        painter->setPen(markerPen);
        painter->drawLine(QLineF(topViewPos, bottomViewPos));

        DrawAxisLabel(
            painter,
            owner_->eventMarkerLabel_.isEmpty() ? QStringLiteral("|r| = Rземли") : owner_->eventMarkerLabel_,
            QPointF(topViewPos.x() + 10.0, topViewPos.y() + 22.0),
            Qt::AlignLeft | Qt::AlignTop);
        painter->setPen(QColor("#4b647b"));
    }

    void DrawEventPoint(QPainter* painter)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->lineSeries_ == nullptr)
        {
            return;
        }

        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();
        const QPointF point = owner_->eventPoint_;
        if (point.x() < xMin || point.x() > xMax || point.y() < yMin || point.y() > yMax)
        {
            return;
        }

        const QPointF chartPos = chart()->mapToPosition(point, owner_->lineSeries_);
        const QPoint viewPos = mapFromScene(chart()->mapToScene(chartPos));

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#d11f4e"));
        painter->drawEllipse(QPointF(viewPos), 4.5, 4.5);

        DrawAxisLabel(
            painter,
            owner_->eventPointLabel_.isEmpty() ? QStringLiteral("|r| = Rземли") : owner_->eventPointLabel_,
            QPointF(viewPos.x() + 10.0, viewPos.y() - 10.0),
            Qt::AlignLeft | Qt::AlignBottom);
        painter->setPen(QColor("#4b647b"));
    }

    bool HandleNativeGesture(QNativeGestureEvent* event)
    {
        if (event == nullptr || owner_ == nullptr)
        {
            return false;
        }

        const QPointF viewportPos = event->position();

        switch (event->gestureType())
        {
        case Qt::ZoomNativeGesture:
        {
            const double delta = event->value();
            if (!std::isfinite(delta) || qFuzzyIsNull(delta))
            {
                event->accept();
                return true;
            }

            const double factor = std::clamp(std::exp(-delta), kMinZoomFactor, kMaxZoomFactor);
            owner_->ZoomAt(viewportPos, factor);
            event->accept();
            return true;
        }
        case Qt::SmartZoomNativeGesture:
            owner_->ResetView();
            event->accept();
            return true;
        case Qt::PanNativeGesture:
        case Qt::SwipeNativeGesture:
            owner_->PanByPixels(event->delta());
            event->accept();
            return true;
        case Qt::BeginNativeGesture:
        case Qt::EndNativeGesture:
            event->accept();
            return true;
        default:
            return false;
        }
    }

    bool HandleGesture(QGestureEvent* event)
    {
        if (event == nullptr || owner_ == nullptr)
        {
            return false;
        }

        QGesture* gesture = event->gesture(Qt::PinchGesture);
        if (gesture == nullptr)
        {
            return false;
        }

        auto* pinch = static_cast<QPinchGesture*>(gesture);
        if ((pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) == 0)
        {
            event->accept(gesture);
            return true;
        }

        const double scale = pinch->scaleFactor();
        if (!std::isfinite(scale) || scale <= 0.0)
        {
            event->accept(gesture);
            return true;
        }

        const double factor = std::clamp(1.0 / scale, kMinZoomFactor, kMaxZoomFactor);
        owner_->ZoomAt(pinch->centerPoint(), factor);
        event->accept(gesture);
        return true;
    }

    QPointer<PlotChartWidget> owner_;
};

PlotChartWidget::PlotChartWidget(const QString& title,
                                 const QString& xAxisLabel,
                                 const QString& yAxisLabel,
                                 const QColor& lineColor,
                                 QWidget* parent)
    : QFrame(parent),
      xAxisLabel_(xAxisLabel),
      yAxisLabel_(yAxisLabel),
      lineColor_(lineColor)
{
    setObjectName("plotCard");
    setMinimumHeight(220);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setObjectName("plotTitle");

    emptyStateLabel_ = new QLabel("Нет данных для отображения", this);
    emptyStateLabel_->setObjectName("plotEmptyState");
    emptyStateLabel_->setAlignment(Qt::AlignCenter);
    emptyStateLabel_->setWordWrap(true);
    emptyStateLabel_->setMinimumHeight(170);
    emptyStateLabel_->setStyleSheet(
        "QLabel#plotEmptyState {"
        " color: #5a7188;"
        " background: #f8f8fa;"
        " border: 1px dashed #c7d7e8;"
        " border-radius: 16px;"
        " font-size: 18px;"
        " font-weight: 600;"
        " padding: 24px;"
        "}");

    chart_ = new QChart();
    chart_->setTheme(QChart::ChartThemeLight);
    chart_->setBackgroundBrush(QBrush(Qt::white));
    chart_->setBackgroundRoundness(0.0);
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setPlotAreaBackgroundBrush(QColor(248, 248, 250));
    chart_->setAnimationOptions(QChart::NoAnimation);
    chart_->legend()->setVisible(false);
    chart_->setMargins(QMargins(0, 0, 0, 0));
    if (chart_->layout() != nullptr)
    {
        chart_->layout()->setContentsMargins(0, 0, 0, 0);
    }

    axisX_ = new QValueAxis();
    axisY_ = new QValueAxis();
    axisX_->setTitleText("");
    axisY_->setTitleText("");
    axisX_->setLabelsColor(QColor("#334155"));
    axisY_->setLabelsColor(QColor("#334155"));
    axisX_->setGridLineColor(QColor("#d3dee9"));
    axisY_->setGridLineColor(QColor("#d3dee9"));
    axisX_->setMinorGridLineColor(QColor("#e6edf5"));
    axisY_->setMinorGridLineColor(QColor("#e6edf5"));
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(0.0, 1.0);

    chart_->addAxis(axisX_, Qt::AlignBottom);
    chart_->addAxis(axisY_, Qt::AlignLeft);

    axisXSeries_ = new QLineSeries();
    axisYSeries_ = new QLineSeries();
    QPen axisPen(QColor("#4b647b"));
    axisPen.setWidthF(1.35);
    axisXSeries_->setPen(axisPen);
    axisYSeries_->setPen(axisPen);

    lineSeries_ = new QLineSeries();
    QPen linePen(lineColor_);
    linePen.setWidthF(2.1);
    lineSeries_->setPen(linePen);

    chart_->addSeries(axisXSeries_);
    chart_->addSeries(axisYSeries_);
    chart_->addSeries(lineSeries_);
    axisXSeries_->attachAxis(axisX_);
    axisXSeries_->attachAxis(axisY_);
    axisYSeries_->attachAxis(axisX_);
    axisYSeries_->attachAxis(axisY_);
    lineSeries_->attachAxis(axisX_);
    lineSeries_->attachAxis(axisY_);

    chartView_ = new InteractiveChartView(this, chart_, this);
    chartView_->setObjectName("plotChartView");
    chartView_->setRenderHint(QPainter::Antialiasing, false);
    chartView_->setMinimumHeight(170);
    chartView_->setFrameShape(QFrame::NoFrame);
    chartView_->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(titleLabel_);
    layout->addWidget(emptyStateLabel_, 1);
    layout->addWidget(chartView_, 1);

    chartView_->hide();

    RefreshAxes();
}

void PlotChartWidget::Clear()
{
    fullPoints_.clear();
    axisXSeries_->clear();
    axisYSeries_->clear();
    lineSeries_->clear();
    ClearEventMarker();
    chartView_->hide();
    emptyStateLabel_->show();
    hasData_ = false;
    defaultXMin_ = 0.0;
    defaultXMax_ = 1.0;
    defaultYMin_ = 0.0;
    defaultYMax_ = 1.0;
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(0.0, 1.0);
    xTickStep_ = -1.0;
    yTickStep_ = -1.0;
    RefreshAxes();
}

void PlotChartWidget::SetData(const QVector<QPointF>& points)
{
    fullPoints_ = points;
    lineSeries_->clear();

    if (points.isEmpty())
    {
        Clear();
        return;
    }

    emptyStateLabel_->hide();
    chartView_->show();

    double xMin = points.front().x();
    double xMax = points.front().x();
    double yMin = points.front().y();
    double yMax = points.front().y();

    for (const QPointF& point : points)
    {
        xMin = std::min(xMin, point.x());
        xMax = std::max(xMax, point.x());
        yMin = std::min(yMin, point.y());
        yMax = std::max(yMax, point.y());
    }

    double xSpan = xMax - xMin;
    double ySpan = yMax - yMin;

    if (!(xSpan > 0.0) || !std::isfinite(xSpan))
    {
        const double pad = std::max(1.0, std::fabs(xMin) * 0.05);
        xMin -= pad;
        xMax += pad;
    }
    else
    {
        const double pad = std::max(1e-9, xSpan * 0.12);
        xMin -= pad;
        xMax += pad;
    }

    if (!(ySpan > 0.0) || !std::isfinite(ySpan))
    {
        const double pad = std::max(1.0, std::fabs(yMin) * 0.05);
        yMin -= pad;
        yMax += pad;
    }
    else
    {
        const double pad = std::max(1e-9, ySpan * 0.12);
        yMin -= pad;
        yMax += pad;
    }

    if (xAxisLabel_ != "t" && chartView_ != nullptr)
    {
        const QRect viewRect = chartView_->viewport()->rect();
        const double viewWidth = std::max(1.0, static_cast<double>(viewRect.width()));
        const double viewHeight = std::max(1.0, static_cast<double>(viewRect.height()));
        const double spanX = std::max(1e-9, xMax - xMin);
        const double spanY = std::max(1e-9, yMax - yMin);
        const double scale = std::max(spanX / viewWidth, spanY / viewHeight);
        const double adjustedSpanX = scale * viewWidth;
        const double adjustedSpanY = scale * viewHeight;
        const double centerX = 0.5 * (xMin + xMax);
        const double centerY = 0.5 * (yMin + yMax);

        xMin = centerX - adjustedSpanX * 0.5;
        xMax = centerX + adjustedSpanX * 0.5;
        yMin = centerY - adjustedSpanY * 0.5;
        yMax = centerY + adjustedSpanY * 0.5;
    }

    hasData_ = true;
    defaultXMin_ = xMin;
    defaultXMax_ = xMax;
    defaultYMin_ = yMin;
    defaultYMax_ = yMax;
    ApplyAxisRange(xMin, xMax, yMin, yMax);
}

void PlotChartWidget::SetEventMarker(double xValue, const QString& label)
{
    hasEventMarker_ = std::isfinite(xValue);
    eventMarkerX_ = xValue;
    eventMarkerLabel_ = label;
    if (chartView_ != nullptr)
    {
        chartView_->viewport()->update();
    }
}

void PlotChartWidget::SetEventPoint(const QPointF& point, const QString& label)
{
    hasEventPoint_ = std::isfinite(point.x()) && std::isfinite(point.y());
    eventPoint_ = point;
    eventPointLabel_ = label;
    if (chartView_ != nullptr)
    {
        chartView_->viewport()->update();
    }
}

void PlotChartWidget::ClearEventMarker()
{
    hasEventMarker_ = false;
    eventMarkerX_ = 0.0;
    eventMarkerLabel_.clear();
    hasEventPoint_ = false;
    eventPoint_ = QPointF();
    eventPointLabel_.clear();
    if (chartView_ != nullptr)
    {
        chartView_->viewport()->update();
    }
}

void PlotChartWidget::ApplyAxisRange(double xMin, double xMax, double yMin, double yMax)
{
    if (!(xMax > xMin) || !std::isfinite(xMin) || !std::isfinite(xMax))
    {
        xMin = 0.0;
        xMax = 1.0;
    }
    if (!(yMax > yMin) || !std::isfinite(yMin) || !std::isfinite(yMax))
    {
        yMin = 0.0;
        yMax = 1.0;
    }

    const double xSpan = std::fabs(xMax - xMin);
    const double ySpan = std::fabs(yMax - yMin);

    double newXTickStep = StableTickStep(xTickStep_, xSpan, 5);
    double newYTickStep = StableTickStep(yTickStep_, ySpan, 5);

    if (!(newXTickStep > 0.0) || !std::isfinite(newXTickStep))
    {
        newXTickStep = 1.0;
    }
    if (!(newYTickStep > 0.0) || !std::isfinite(newYTickStep))
    {
        newYTickStep = 1.0;
    }

    const int xDecimals = DecimalsForStep(newXTickStep);
    const int yDecimals = DecimalsForStep(newYTickStep);

    axisX_->setTickType(QValueAxis::TicksDynamic);
    axisY_->setTickType(QValueAxis::TicksDynamic);
    axisX_->setTickAnchor(0.0);
    axisY_->setTickAnchor(0.0);
    axisX_->setTickInterval(newXTickStep);
    axisY_->setTickInterval(newYTickStep);
    axisX_->setMinorTickCount(3);
    axisY_->setMinorTickCount(3);
    axisX_->setLabelsAngle(0);
    axisY_->setLabelsAngle(0);
    axisX_->setLabelFormat(QString::asprintf("%%.%df", xDecimals));
    axisY_->setLabelFormat(QString::asprintf("%%.%df", yDecimals));
    axisX_->setTruncateLabels(false);
    axisY_->setTruncateLabels(false);

    xTickStep_ = newXTickStep;
    yTickStep_ = newYTickStep;

    axisX_->setRange(xMin, xMax);
    axisY_->setRange(yMin, yMax);
    UpdateAxisSeries();
    RefreshLineSeries();
}

QVector<QPointF> PlotChartWidget::BuildRenderedPoints() const
{
    if (fullPoints_.size() <= kMaxRenderedPoints)
    {
        return fullPoints_;
    }

    if (!IsMonotonicByX(fullPoints_))
    {
        return DownsamplePoints(fullPoints_);
    }

    const double visibleXMin = axisX_->min();
    const double visibleXMax = axisX_->max();
    const auto leftIt = std::lower_bound(
        fullPoints_.cbegin(), fullPoints_.cend(), visibleXMin, [](const QPointF& point, double value) {
            return point.x() < value;
        });
    const auto rightIt = std::upper_bound(
        fullPoints_.cbegin(), fullPoints_.cend(), visibleXMax, [](double value, const QPointF& point) {
            return value < point.x();
        });

    int firstIndex = static_cast<int>(std::distance(fullPoints_.cbegin(), leftIt));
    int lastIndex = static_cast<int>(std::distance(fullPoints_.cbegin(), rightIt)) - 1;

    firstIndex = std::max(0, firstIndex - 1);
    lastIndex = std::min(static_cast<int>(fullPoints_.size()) - 1, std::max(firstIndex, lastIndex + 1));

    QVector<QPointF> visiblePoints;
    visiblePoints.reserve(lastIndex - firstIndex + 1);
    for (int index = firstIndex; index <= lastIndex; ++index)
    {
        visiblePoints.append(fullPoints_[index]);
    }

    if (visiblePoints.size() <= kMaxRenderedPoints)
    {
        return visiblePoints;
    }

    return DownsamplePoints(visiblePoints);
}

void PlotChartWidget::RefreshLineSeries()
{
    if (fullPoints_.isEmpty())
    {
        lineSeries_->clear();
        return;
    }

    lineSeries_->replace(BuildRenderedPoints());
}

void PlotChartWidget::UpdateAxisSeries()
{
    if (axisXSeries_ == nullptr || axisYSeries_ == nullptr)
    {
        return;
    }

    axisXSeries_->clear();
    axisYSeries_->clear();

    const double xMin = axisX_->min();
    const double xMax = axisX_->max();
    const double yMin = axisY_->min();
    const double yMax = axisY_->max();

    if (yMin <= 0.0 && yMax >= 0.0)
    {
        axisXSeries_->append(xMin, 0.0);
        axisXSeries_->append(xMax, 0.0);
    }

    if (xMin <= 0.0 && xMax >= 0.0)
    {
        axisYSeries_->append(0.0, yMin);
        axisYSeries_->append(0.0, yMax);
    }
}

void PlotChartWidget::RefreshAxes()
{
    ApplyAxisRange(axisX_->min(), axisX_->max(), axisY_->min(), axisY_->max());
}

void PlotChartWidget::PanByPixels(const QPointF& pixelDelta)
{
    if (!hasData_ || chart_ == nullptr || !(std::isfinite(pixelDelta.x()) && std::isfinite(pixelDelta.y())))
    {
        return;
    }

    const QRectF plotArea = chart_->plotArea();
    if (!(plotArea.width() > 1.0) || !(plotArea.height() > 1.0))
    {
        return;
    }

    const double currentXMin = axisX_->min();
    const double currentXMax = axisX_->max();
    const double currentYMin = axisY_->min();
    const double currentYMax = axisY_->max();
    const double currentXSpan = currentXMax - currentXMin;
    const double currentYSpan = currentYMax - currentYMin;

    if (!(currentXSpan > 0.0) || !(currentYSpan > 0.0))
    {
        return;
    }

    const double dx = -(pixelDelta.x() / plotArea.width()) * currentXSpan;
    const double dy = (pixelDelta.y() / plotArea.height()) * currentYSpan;

    ApplyAxisRange(currentXMin + dx, currentXMax + dx, currentYMin + dy, currentYMax + dy);
}

void PlotChartWidget::ResetView()
{
    if (!hasData_)
    {
        Clear();
        return;
    }

    xTickStep_ = -1.0;
    yTickStep_ = -1.0;
    ApplyAxisRange(defaultXMin_, defaultXMax_, defaultYMin_, defaultYMax_);
}

void PlotChartWidget::ZoomAt(const QPointF& viewportPos, double factor)
{
    if (!hasData_ || !(factor > 0.0) || !std::isfinite(factor) || chartView_ == nullptr || chart_ == nullptr)
    {
        return;
    }

    const QPointF scenePos = chartView_->mapToScene(viewportPos.toPoint());
    const QPointF chartPos = chart_->mapFromScene(scenePos);
    const QRectF plotArea = chart_->plotArea();
    if (!plotArea.contains(chartPos))
    {
        return;
    }

    const QPointF anchor = chart_->mapToValue(chartPos, lineSeries_);
    const double currentXMin = axisX_->min();
    const double currentXMax = axisX_->max();
    const double currentYMin = axisY_->min();
    const double currentYMax = axisY_->max();

    const double currentXSpan = currentXMax - currentXMin;
    const double currentYSpan = currentYMax - currentYMin;
    if (!(currentXSpan > 0.0) || !(currentYSpan > 0.0))
    {
        return;
    }

    const double baseXSpan = std::max(1e-12, std::fabs(defaultXMax_ - defaultXMin_));
    const double baseYSpan = std::max(1e-12, std::fabs(defaultYMax_ - defaultYMin_));
    const double minXSpan = baseXSpan * 1e-6;
    const double minYSpan = baseYSpan * 1e-6;
    const double maxXSpan = baseXSpan * 1000.0;
    const double maxYSpan = baseYSpan * 1000.0;

    double newXSpan = std::clamp(currentXSpan * factor, minXSpan, maxXSpan);
    double newYSpan = std::clamp(currentYSpan * factor, minYSpan, maxYSpan);

    if (!(newXSpan > 0.0) || !(newYSpan > 0.0))
    {
        return;
    }

    const double xRatio = (anchor.x() - currentXMin) / currentXSpan;
    const double yRatio = (anchor.y() - currentYMin) / currentYSpan;

    const double clampedXRatio = std::clamp(xRatio, 0.0, 1.0);
    const double clampedYRatio = std::clamp(yRatio, 0.0, 1.0);

    const double newXMin = anchor.x() - newXSpan * clampedXRatio;
    const double newXMax = newXMin + newXSpan;
    const double newYMin = anchor.y() - newYSpan * clampedYRatio;
    const double newYMax = newYMin + newYSpan;

    ApplyAxisRange(newXMin, newXMax, newYMin, newYMax);
}

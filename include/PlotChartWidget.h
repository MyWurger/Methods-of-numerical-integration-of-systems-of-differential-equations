#pragma once

#include <QColor>
#include <QFrame>
#include <QPointF>
#include <QVector>

QT_BEGIN_NAMESPACE
class QLabel;
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
QT_END_NAMESPACE

class InteractiveChartView;

class PlotChartWidget final : public QFrame
{
public:
    explicit PlotChartWidget(const QString& title,
                             const QString& xAxisLabel,
                             const QString& yAxisLabel,
                             const QColor& lineColor,
                             QWidget* parent = nullptr);

    void Clear();
    void SetData(const QVector<QPointF>& points);
    void SetEventMarker(double xValue, const QString& label);
    void SetEventPoint(const QPointF& point, const QString& label);
    void ClearEventMarker();

private:
    friend class InteractiveChartView;

    void ApplyAxisRange(double xMin, double xMax, double yMin, double yMax);
    QVector<QPointF> BuildRenderedPoints() const;
    void RefreshLineSeries();
    void UpdateAxisSeries();
    void PanByPixels(const QPointF& pixelDelta);
    void RefreshAxes();
    void ResetView();
    void ZoomAt(const QPointF& viewportPos, double factor);

    QLabel* titleLabel_ = nullptr;
    QLabel* emptyStateLabel_ = nullptr;
    QChart* chart_ = nullptr;
    QChartView* chartView_ = nullptr;
    QLineSeries* axisXSeries_ = nullptr;
    QLineSeries* axisYSeries_ = nullptr;
    QLineSeries* lineSeries_ = nullptr;
    QValueAxis* axisX_ = nullptr;
    QValueAxis* axisY_ = nullptr;
    QString xAxisLabel_;
    QString yAxisLabel_;
    QColor lineColor_;
    QVector<QPointF> fullPoints_;

    double xTickStep_ = -1.0;
    double yTickStep_ = -1.0;
    double defaultXMin_ = 0.0;
    double defaultXMax_ = 1.0;
    double defaultYMin_ = 0.0;
    double defaultYMax_ = 1.0;
    bool hasData_ = false;
    bool hasEventMarker_ = false;
    double eventMarkerX_ = 0.0;
    QString eventMarkerLabel_;
    bool hasEventPoint_ = false;
    QPointF eventPoint_;
    QString eventPointLabel_;
};

// ============================================================================
// ФАЙЛ PLOTCHARTWIDGET.H - ОБЪЯВЛЕНИЕ ВИДЖЕТА ИНТЕРАКТИВНОГО ГРАФИКА
// ============================================================================
// Назначение файла:
// 1) Объявить класс PlotChartWidget, который отображает один график.
// 2) Описать данные и служебные методы, нужные для работы с осями, сериями,
//    zoom, pan и дополнительными отметками событий.
// 3) Хранить текущее состояние отображаемого набора точек и диапазонов осей.
// ============================================================================

// Защита от повторного включения заголовочного файла.
#pragma once

// QColor нужен для задания цвета линии графика.
#include <QColor>
// QFrame используется как базовый класс карточки графика.
#include <QFrame>
// QPointF нужен для представления одной точки графика в вещественных координатах.
#include <QPointF>
// QVector используется для хранения наборов точек графика.
#include <QVector>

QT_BEGIN_NAMESPACE
// Предварительное объявление QLabel для заголовка и заглушки "нет данных".
class QLabel;
// Предварительное объявление QChart для самой диаграммы Qt Charts.
class QChart;
// Предварительное объявление QChartView для отображения графика в виджете.
class QChartView;
// Предварительное объявление QLineSeries для основной линии и осевых серий.
class QLineSeries;
// Предварительное объявление QValueAxis для числовых осей графика.
class QValueAxis;
QT_END_NAMESPACE

// Предварительное объявление вспомогательного класса интерактивного view.
class InteractiveChartView;

// ----------------------------------------------------------------------------
// КЛАСС PlotChartWidget - КАРТОЧКА ОДНОГО ИНТЕРАКТИВНОГО ГРАФИКА
// ----------------------------------------------------------------------------
// Класс отвечает за:
// - показ одного графика в отдельной карточке;
// - хранение полного набора точек;
// - отрисовку только нужной части данных;
// - управление диапазоном осей;
// - zoom и pan;
// - показ маркеров событий и точек остановки.
// ----------------------------------------------------------------------------
class PlotChartWidget final : public QFrame
{
public:
    // Конструктор виджета графика.
    //
    // explicit запрещает неявное создание PlotChartWidget из одного
    // строкового или иного одиночного аргумента конструктора.
    //
    // Принимает:
    // - title      : текст заголовка карточки;
    // - xAxisLabel : подпись оси X;
    // - yAxisLabel : подпись оси Y;
    // - lineColor  : цвет основной линии графика;
    // - parent     : родительский Qt-виджет.
    explicit PlotChartWidget(const QString& title,
                             const QString& xAxisLabel,
                             const QString& yAxisLabel,
                             const QColor& lineColor,
                             QWidget* parent = nullptr);

    // Полностью очищает график:
    // - удаляет точки;
    // - сбрасывает маркеры;
    // - возвращает диапазон по умолчанию;
    // - показывает заглушку "Нет данных".
    void Clear();

    // Передает в виджет новый набор точек для отображения.
    void SetData(const QVector<QPointF>& points);

    // Устанавливает вертикальную отметку события по координате X.
    // Такой маркер используется, например, на графиках координат для показа
    // момента столкновения.
    void SetEventMarker(double xValue, const QString& label);

    // Устанавливает точечную отметку события в одной точке графика.
    void SetEventPoint(const QPointF& point, const QString& label);

    // Удаляет все текущие отметки событий с графика.
    void ClearEventMarker();

private:
    // InteractiveChartView получает доступ к закрытым данным графика,
    // поскольку сам обрабатывает жесты, zoom, pan и дорисовку поверх chart.
    friend class InteractiveChartView;

    // Применяет новый численный диапазон осей.
    void ApplyAxisRange(double xMin, double xMax, double yMin, double yMax);

    // Строит укороченный набор точек для текущего видимого диапазона, чтобы
    // не перегружать отрисовку слишком большими сериями.
    QVector<QPointF> BuildRenderedPoints() const;

    // Обновляет основную линейную серию графика.
    void RefreshLineSeries();

    // Обновляет служебные осевые серии, если координатные оси должны быть
    // показаны внутри графика.
    void UpdateAxisSeries();

    // Сдвигает окно просмотра по графику на величину в пикселях.
    void PanByPixels(const QPointF& pixelDelta);

    // Обновляет оформление и состояние осей.
    void RefreshAxes();

    // Возвращает график к исходному диапазону данных.
    void ResetView();

    // Выполняет zoom относительно выбранной точки viewport.
    void ZoomAt(const QPointF& viewportPos, double factor);

private:
    // Заголовок карточки графика.
    QLabel* titleLabel_ = nullptr;

    // Сообщение-заглушка, показываемое при отсутствии данных.
    QLabel* emptyStateLabel_ = nullptr;

    // Объект диаграммы Qt Charts.
    QChart* chart_ = nullptr;

    // Виджет отображения диаграммы.
    QChartView* chartView_ = nullptr;

    // Серия внутренней оси X.
    QLineSeries* axisXSeries_ = nullptr;

    // Серия внутренней оси Y.
    QLineSeries* axisYSeries_ = nullptr;

    // Основная линейная серия самого графика.
    QLineSeries* lineSeries_ = nullptr;

    // Числовая ось X.
    QValueAxis* axisX_ = nullptr;

    // Числовая ось Y.
    QValueAxis* axisY_ = nullptr;

    // Текст подписи оси X.
    QString xAxisLabel_;

    // Текст подписи оси Y.
    QString yAxisLabel_;

    // Цвет линии графика.
    QColor lineColor_;

    // Полный набор точек без сокращения.
    QVector<QPointF> fullPoints_;

    // Текущий "красивый" шаг по оси X.
    double xTickStep_ = -1.0;

    // Текущий "красивый" шаг по оси Y.
    double yTickStep_ = -1.0;

    // Исходный минимальный предел по X.
    double defaultXMin_ = 0.0;

    // Исходный максимальный предел по X.
    double defaultXMax_ = 1.0;

    // Исходный минимальный предел по Y.
    double defaultYMin_ = 0.0;

    // Исходный максимальный предел по Y.
    double defaultYMax_ = 1.0;

    // Признак того, что в графике уже есть реальные данные.
    bool hasData_ = false;

    // Признак наличия вертикального маркера события.
    bool hasEventMarker_ = false;

    // Координата X вертикального маркера.
    double eventMarkerX_ = 0.0;

    // Подпись вертикального маркера.
    QString eventMarkerLabel_;

    // Признак наличия точечного маркера.
    bool hasEventPoint_ = false;

    // Координаты точечного маркера.
    QPointF eventPoint_;

    // Подпись точечного маркера.
    QString eventPointLabel_;
};

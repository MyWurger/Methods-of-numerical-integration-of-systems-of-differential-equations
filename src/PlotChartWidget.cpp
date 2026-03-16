// ============================================================================
// ФАЙЛ PlotChartWidget.cpp - РЕАЛИЗАЦИЯ КАРТОЧКИ ГРАФИКА И ЕГО ИНТЕРАКТИВНОСТИ
// ============================================================================
// Назначение файла:
// 1) создать готовый виджет-карточку для показа одного графика;
// 2) настроить Qt Charts оси, серии и стиль отображения;
// 3) реализовать пустое состояние "Нет данных для отображения";
// 4) реализовать пользовательскую отрисовку осей, стрелок и подписей;
// 5) добавить интерактивность через тачпад:
//    - zoom;
//    - pan;
//    - reset view;
// 6) уметь прореживать данные, чтобы интерфейс не зависал на слишком больших
//    массивах точек.
// ============================================================================

// Подключаем объявление собственного класса PlotChartWidget.
#include "PlotChartWidget.h"

// Событие жестов Qt.
#include <QGestureEvent>

// Метрики шрифта нужны для аккуратной ручной отрисовки подписей осей.
#include <QFontMetricsF>

// QLabel используется для заголовка карточки и пустого состояния.
#include <QLabel>

// Событие native-жестов macOS.
#include <QNativeGestureEvent>

// QPainter нужен для ручной дорисовки стрелок осей, подписей и маркеров.
#include <QPainter>

// Событие перерисовки QWidget/QGraphicsView.
#include <QPaintEvent>

// Жест pinch используется для зума на тачпаде.
#include <QPinchGesture>

// QPointer - безопасный указатель Qt на QObject-потомка.
// Если объект будет уничтожен, QPointer автоматически станет nullptr.
#include <QPointer>

// Нужен для доступа к layout самого QChart.
#include <QGraphicsLayout>

// Вертикальный layout карточки графика.
#include <QVBoxLayout>

// QWheelEvent используется для панорамирования через pixelDelta.
#include <QWheelEvent>

// Основной объект Qt Charts, представляющий сам график.
#include <QtCharts/QChart>

// Виджет просмотра графика.
#include <QtCharts/QChartView>

// Линейные серии для кривой и осей.
#include <QtCharts/QLineSeries>

// Числовые оси графика.
#include <QtCharts/QValueAxis>

// <algorithm> нужен для std::min, std::max, std::clamp, std::lower_bound и
// других алгоритмов.
#include <algorithm>

// <cmath> нужен для log10, floor, pow, ceil, fabs, exp и других численных
// операций.
#include <cmath>

// ----------------------------------------------------------------------------
// ЛОКАЛЬНЫЕ КОНСТАНТЫ И ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ----------------------------------------------------------------------------
// Все, что находится в анонимном namespace, доступно только внутри данного
// .cpp файла и не "засоряет" глобальное пространство имен проекта.
// ----------------------------------------------------------------------------
namespace
{
// Верхняя граница на число одновременно рисуемых точек.
// Полный набор данных хранится отдельно, но на экран выводится разумно
// прореженная версия, чтобы не перегружать интерфейс.
constexpr int kMaxRenderedPoints = 20000;

// Минимальный и максимальный множители zoom за один жест/шаг.
constexpr double kMinZoomFactor = 0.75;
constexpr double kMaxZoomFactor = 1.35;

// Геометрические размеры стрелок осей.
constexpr double kAxisArrowLength = 9.0;
constexpr double kAxisArrowHalfWidth = 4.5;

// ----------------------------------------------------------------------------
// ФУНКЦИЯ NiceTickStep
// ----------------------------------------------------------------------------
// Назначение:
// Подобрать "красивый" шаг делений оси на основе диапазона и желаемого
// числа крупных делений.
//
// Почему это нужно:
// Если брать шаг делений просто как span / (targetTicks - 1), подписи осей
// часто получаются неудобными: 3.14159, 742857 и т.п.
//
// Поэтому шаг округляется к более "человеческим" значениям:
// 1, 2, 5, 10 и их степенным масштабам.
// ----------------------------------------------------------------------------
double NiceTickStep(double span, int targetTicks)
{
    // Если диапазон некорректный или число целевых делений слишком мало,
    // возвращаем безопасный шаг по умолчанию.
    if (!std::isfinite(span) || span <= 0.0 || targetTicks < 2)
    {
        return 1.0;
    }

    // rawStep - это "сырой" шаг без округления:
    // просто делим диапазон на желаемое число интервалов.
    const double rawStep = span / static_cast<double>(targetTicks - 1);

    // magnitude - порядок величины rawStep.
    // Это нужно, чтобы затем нормализовать шаг к диапазону примерно [1; 10).
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));

    // normalized - это rawStep, приведенный к удобной шкале.
    const double normalized = rawStep / magnitude;

    // nice - "красивый" коэффициент внутри одного порядка величины.
    // По умолчанию берем 1.
    double nice = 1.0;

    // Дальше округляем normalized к одному из удобных значений:
    // 1, 2, 5 или 10.
    //
    // Именно такие значения обычно дают хорошо читаемые деления оси.
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

    // Возвращаем итоговый "красивый" шаг:
    // коэффициент * порядок величины.
    return nice * magnitude;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ DecimalsForStep
// ----------------------------------------------------------------------------
// Назначение:
// Оценить, сколько знаков после запятой разумно показывать в подписях оси
// для данного шага делений.
// ----------------------------------------------------------------------------
int DecimalsForStep(double step)
{
    // Если шаг некорректный, возвращаем запасное разумное число знаков.
    if (!std::isfinite(step) || step <= 0.0)
    {
        return 6;
    }

    // Если шаг уже не меньше единицы, дробная часть в подписях обычно не нужна.
    if (step >= 1.0)
    {
        return 0;
    }

    // Для шагов меньше 1 оцениваем, сколько знаков после запятой потребуется,
    // чтобы подписи оси не теряли смысл.
    const int decimals = static_cast<int>(std::ceil(-std::log10(step))) + 1;

    // Ограничиваем результат разумными пределами, чтобы формат подписей
    // не стал слишком коротким или слишком перегруженным.
    return std::clamp(decimals, 0, 12);
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ StableTickStep
// ----------------------------------------------------------------------------
// Назначение:
// Стабилизировать шаг делений оси при небольших изменениях масштаба.
//
// Зачем это нужно:
// Если при каждом маленьком зуме шаг перескакивает на другое значение,
// подписи оси начинают "дрожать". Здесь добавлен небольшой hysteresis,
// чтобы шаг менялся не слишком нервно.
// ----------------------------------------------------------------------------
double StableTickStep(double previousStep, double span, int targetTicks)
{
    // desired - новый "идеальный" шаг делений для текущего диапазона.
    const double desired = NiceTickStep(span, targetTicks);

    // Если предыдущий шаг еще не был корректно задан, возвращаем значение,
    // связанное с desired, чтобы инициализация происходила мягче.
    if (!(previousStep > 0.0) || !std::isfinite(previousStep))
    {
        return desired * 0.5;
    }

    // ratio показывает, насколько новый желаемый шаг отличается от старого.
    const double ratio = desired / previousStep;

    // Если отношение некорректно, просто берем новый желаемый шаг.
    if (!std::isfinite(ratio) || ratio <= 0.0)
    {
        return desired;
    }

    // hysteresis задает "мертвую зону", внутри которой шаг оси не меняется.
    // Это уменьшает визуальное дрожание подписей при небольших zoom/pan.
    const double hysteresis = 1.20;

    // Если различие уже достаточно заметное, принимаем новый шаг.
    if (ratio > hysteresis || ratio < (1.0 / hysteresis))
    {
        return desired;
    }

    // Иначе сохраняем предыдущий шаг, чтобы шкала вела себя стабильнее.
    return previousStep;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ DownsamplePoints
// ----------------------------------------------------------------------------
// Назначение:
// Прореживать набор точек до разумного количества для отрисовки.
//
// Полные данные не теряются: они по-прежнему хранятся в fullPoints_.
// Прореживание касается только визуального представления.
// ----------------------------------------------------------------------------
QVector<QPointF> DownsamplePoints(const QVector<QPointF>& points)
{
    // Если точек и так немного, прореживание не требуется.
    if (points.size() <= kMaxRenderedPoints)
    {
        return points;
    }

    // Создаем новый контейнер под уменьшенный набор точек.
    QVector<QPointF> sampled;

    // reserve(...) заранее выделяет память под нужное количество элементов,
    // чтобы уменьшить число внутренних перераспределений памяти.
    sampled.reserve(kMaxRenderedPoints);

    // step показывает, через какой дробный индекс нужно брать точки,
    // чтобы равномерно покрыть весь исходный массив.
    const double step = static_cast<double>(points.size() - 1) / static_cast<double>(kMaxRenderedPoints - 1);

    // Последний допустимый индекс исходного массива.
    const int maxIndex = points.size() - 1;

    // Выбираем почти все точки равномерно по всему диапазону исходных данных.
    for (int i = 0; i < kMaxRenderedPoints - 1; ++i)
    {
        // std::round(...) округляет дробный индекс до ближайшего целого.
        const int index = static_cast<int>(std::round(i * step));

        // std::clamp(...) страхует нас от случайного выхода за допустимые
        // границы массива.
        sampled.append(points[std::clamp(index, 0, maxIndex)]);
    }

    // Последнюю точку всегда добавляем отдельно, чтобы гарантированно
    // сохранить правый конец графика.
    sampled.append(points.back());
    return sampled;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ IsMonotonicByX
// ----------------------------------------------------------------------------
// Назначение:
// Проверить, неубывает ли X-координата точек.
//
// Это важно для временных графиков вида t -> value:
// если X монотонен, можно эффективно выделять только видимый диапазон
// через lower_bound / upper_bound.
// ----------------------------------------------------------------------------
bool IsMonotonicByX(const QVector<QPointF>& points)
{
    // Начинаем с элемента 1, потому что каждый элемент сравнивается
    // с предыдущим.
    for (int i = 1; i < points.size(); ++i)
    {
        // Если нашли хотя бы одно убывание по X, монотонность нарушена.
        if (points[i].x() < points[i - 1].x())
        {
            return false;
        }
    }

    // Если ни одного нарушения не найдено, X-координата не убывает.
    return true;
}
}

// ----------------------------------------------------------------------------
// КЛАСС InteractiveChartView
// ----------------------------------------------------------------------------
// Это внутренний вспомогательный класс, расширяющий QChartView.
//
// Зачем он нужен:
// стандартный QChartView не знает о нашей пользовательской логике:
// - дорисовке стрелок и подписей осей;
// - обработке native-жестов macOS;
// - zoom относительно точки под курсором;
// - панорамировании двумя пальцами.
//
// Поэтому создается отдельный наследник, который:
// 1) перехватывает нужные события;
// 2) вызывает методы владельца PlotChartWidget;
// 3) дорисовывает дополнительные элементы поверх стандартного графика.
// ----------------------------------------------------------------------------
class InteractiveChartView final : public QChartView
{
public:
    // Конструктор получает указатель на владеющий PlotChartWidget,
    // чтобы можно было обращаться к его данным и настройкам.
    //
    // final означает, что от этого класса дальше нельзя наследоваться.
    // В данном случае это логично: класс задуман как закрытая внутренняя
    // реализация под конкретный PlotChartWidget, а не как новая точка
    // расширения иерархии.
    explicit InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent = nullptr)
        : QChartView(chart, parent), owner_(owner)
    {
        // Отключаем стандартный drag и rubber band, потому что используем
        // собственную логику pan/zoom.
        setDragMode(QGraphicsView::NoDrag);
        setRubberBand(QChartView::NoRubberBand);
        setMouseTracking(true);

        // FullViewportUpdate уменьшает вероятность артефактов при ручной
        // дорисовке поверх QChartView.
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

        // Разрешаем принимать touch/gesture события как самому view,
        // так и его viewport.
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
        grabGesture(Qt::PinchGesture);
        viewport()->grabGesture(Qt::PinchGesture);
    }

protected:
    // paintEvent(...) - это виртуальный метод QWidget/QGraphicsView,
    // который вызывается каждый раз, когда виджет должен перерисоваться.
    //
    // override означает, что мы именно переопределяем унаследованный
    // виртуальный метод базового класса.
    void paintEvent(QPaintEvent* event) override
    {
        // Сначала даем QChartView отрисовать обычный график средствами Qt.
        QChartView::paintEvent(event);

        // Если данных нет, дополнительно поверх рисовать нечего.
        if (owner_ == nullptr || !owner_->hasData_ || chart() == nullptr)
        {
            return;
        }

        // Создаем painter уже для ручной дорисовки осей, стрелок, подписей
        // и маркеров событий.
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QPen axisPen(QColor("#4b647b"), 1.35);
        painter.setPen(axisPen);

        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();

        // drawXAxis / drawYAxis - логические флаги, показывающие, попадает ли
        // математический ноль в текущий диапазон осей.
        //
        // Если ноль не виден, внутреннюю ось поверх графика рисовать не надо.
        const bool drawXAxis = (yMin <= 0.0 && yMax >= 0.0);
        const bool drawYAxis = (xMin <= 0.0 && xMax >= 0.0);

        // Ось Ox рисуем только если значение y = 0 попадает в текущий видимый
        // диапазон. Аналогично с осью Oy.
        // Если ось Ox должна быть видна, дорисовываем ее поверх графика.
        if (drawXAxis)
        {
            DrawHorizontalAxis(&painter, xMin, xMax);
        }

        // Аналогично, при необходимости дорисовываем ось Oy.
        if (drawYAxis)
        {
            DrawVerticalAxis(&painter, yMin, yMax);
        }

        // Далее при наличии включаем дополнительные пользовательские
        // маркеры событий: сначала вертикальный маркер, затем точечный.
        if (owner_->hasEventMarker_)
        {
            DrawEventMarker(&painter);
        }
        if (owner_->hasEventPoint_)
        {
            DrawEventPoint(&painter);
        }
    }

    // viewportEvent(...) перехватывает события, приходящие во viewport.
    // Здесь мы отлавливаем жесты и native-жесты.
    //
    // Почему именно viewportEvent(...), а не только обычные mouse/wheel
    // обработчики:
    // часть жестов в QGraphicsView/QChartView приходит именно на viewport.
    bool viewportEvent(QEvent* event) override
    {
        // Если самого события нет или владелец уже недоступен,
        // используем стандартную обработку базового класса.
        if (event == nullptr || owner_ == nullptr)
        {
            return QChartView::viewportEvent(event);
        }

        // Проверяем, не является ли событие native-жестом платформы.
        // На macOS сюда попадают zoom/pan-жесты тачпада.
        if (event->type() == QEvent::NativeGesture)
        {
            // static_cast здесь допустим, потому что тип события уже проверен
            // через event->type().
            auto* nativeEvent = static_cast<QNativeGestureEvent*>(event);

            // Если HandleNativeGesture(...) смог обработать событие,
            // дальше вниз по цепочке его передавать уже не нужно.
            if (HandleNativeGesture(nativeEvent))
            {
                // Возврат true означает: событие обработано здесь и дальше
                // передавать его не нужно.
                return true;
            }
        }

        // Если это не native gesture, проверяем обычный Qt gesture event.
        if (event->type() == QEvent::Gesture)
        {
            // Та же логика для обычного Qt gesture event.
            auto* gestureEvent = static_cast<QGestureEvent*>(event);
            if (HandleGesture(gestureEvent))
            {
                return true;
            }
        }

        // Если ни одна наша специализированная ветка не сработала,
        // передаем событие стандартной реализации QChartView.
        return QChartView::viewportEvent(event);
    }

    // wheelEvent(...) здесь используется не только для классического колесика,
    // но и для pixelDelta от трекпада на macOS.
    void wheelEvent(QWheelEvent* event) override
    {
        // Если график пустой или владелец недоступен, передаем обработку
        // базовому классу.
        if (event == nullptr || owner_ == nullptr || !owner_->hasData_)
        {
            QChartView::wheelEvent(event);
            return;
        }

        // Не перехватываем Ctrl+wheel, чтобы не ломать стандартные сценарии,
        // если Qt или система захотят обработать их иначе.
        if ((event->modifiers() & Qt::ControlModifier) != 0)
        {
            QChartView::wheelEvent(event);
            return;
        }

        // pixelDelta() особенно полезен для трекпада, потому что дает
        // плавное смещение в пикселях.
        const QPoint pixelDelta = event->pixelDelta();
        if (!pixelDelta.isNull())
        {
            // Передаем панорамирование владельцу графика.
            owner_->PanByPixels(pixelDelta);

            // accept() помечает событие как обработанное в этом месте.
            event->accept();
            return;
        }

        // Если pixelDelta нет, возвращаем обработку стандартному поведению.
        QChartView::wheelEvent(event);
    }

private:
    // ----------------------------------------------------------------------------
    // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ РУЧНОЙ ОТРИСОВКИ
    // ----------------------------------------------------------------------------
    void DrawHorizontalAxis(QPainter* painter, double xMin, double xMax)
    {
        // Без painter, владельца или самого chart отрисовка невозможна.
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr)
        {
            return;
        }

        // mapToPosition(...) переводит точку из координат данных графика
        // в координаты plot area Qt Charts.
        const QPointF leftChartPos = chart()->mapToPosition(QPointF(xMin, 0.0), owner_->lineSeries_);
        const QPointF rightChartPos = chart()->mapToPosition(QPointF(xMax, 0.0), owner_->lineSeries_);

        // Далее через mapToScene(...) и mapFromScene(...) переводим координаты
        // уже в систему viewport, в которой рисует QPainter.
        const QPoint leftViewPos = mapFromScene(chart()->mapToScene(leftChartPos));
        const QPoint rightViewPos = mapFromScene(chart()->mapToScene(rightChartPos));

        // Рисуем стрелку только в положительном направлении оси.
        DrawArrowHead(painter, QPointF(rightViewPos), QPointF(leftViewPos));

        // Подпись оси X ставим рядом с правым концом оси, то есть возле
        // положительного направления.
        //
        // В DrawAxisLabel(...) передаются:
        // 1) painter - объект рисования;
        // 2) owner_->xAxisLabel_ - текст подписи оси;
        // 3) QPointF(...) - опорная точка, около которой должна появиться
        //    подпись;
        // 4) Qt::AlignRight | Qt::AlignBottom - флаги выравнивания.
        //
        // Символ | здесь означает побитовое ИЛИ.
        // В Qt таким образом часто объединяют несколько флагов в одно значение.
        //
        // Qt::AlignRight | Qt::AlignBottom читается как:
        // "разместить подпись так, чтобы ее прямоугольник был прижат
        // вправо и вниз относительно anchor-точки".
        DrawAxisLabel(painter,
                      owner_->xAxisLabel_,
                      QPointF(rightViewPos.x() - 16.0, rightViewPos.y() - 24.0),
                      Qt::AlignRight | Qt::AlignBottom);
    }

    void DrawVerticalAxis(QPainter* painter, double yMin, double yMax)
    {
        // Те же защитные проверки, что и для горизонтальной оси.
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr)
        {
            return;
        }

        // Вычисляем экранные позиции верхней и нижней точек оси Oy.
        const QPointF topChartPos = chart()->mapToPosition(QPointF(0.0, yMax), owner_->lineSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(0.0, yMin), owner_->lineSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        // Стрелка у вертикальной оси тоже рисуется только вверх,
        // то есть в положительном направлении.
        DrawArrowHead(painter, QPointF(topViewPos), QPointF(bottomViewPos));

        // Подпись оси Y ставим рядом с верхней частью оси.
        //
        // Здесь используется другой набор флагов:
        // Qt::AlignLeft | Qt::AlignBottom.
        //
        // Это означает:
        // - подпись считается "прижатой" влево;
        // - и одновременно вниз
        // относительно опорной точки QPointF(...).
        //
        // Небольшие смещения +16 и -10 снова служат чисто для визуальной
        // подстройки положения текста рядом со стрелкой оси.
        DrawAxisLabel(painter,
                      owner_->yAxisLabel_,
                      QPointF(topViewPos.x() + 16.0, topViewPos.y() - 10.0),
                      Qt::AlignLeft | Qt::AlignBottom);
    }

    void DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from)
    {
        // Без painter рисовать нечем.
        if (painter == nullptr)
        {
            return;
        }

        // direction задает направление от базовой точки к вершине стрелки.
        const QLineF direction(from, tip);
        if (qFuzzyIsNull(direction.length()))
        {
            return;
        }

        // unitVector() позволяет получить направление единичной длины.
        // Это удобно, чтобы независимо от реального размера оси строить
        // стрелки одинакового размера.
        QLineF unit = direction.unitVector();

        // delta - единичный вектор направления стрелки.
        const QPointF delta = unit.p2() - unit.p1();

        // back - точка чуть позади вершины стрелки.
        // От нее затем строятся "крылья".
        const QPointF back = tip - delta * kAxisArrowLength;

        // normal - вектор, перпендикулярный направлению стрелки.
        // Он нужен, чтобы раздвинуть два крыла в стороны.
        const QPointF normal(-delta.y(), delta.x());

        // Две боковые точки наконечника стрелки.
        const QPointF wing1 = back + normal * kAxisArrowHalfWidth;
        const QPointF wing2 = back - normal * kAxisArrowHalfWidth;

        // Рисуем две стороны наконечника.
        painter->drawLine(QLineF(tip, wing1));
        painter->drawLine(QLineF(tip, wing2));
    }

    void DrawAxisLabel(QPainter* painter, const QString& text, const QPointF& anchor, Qt::Alignment alignment)
    {
        // Если текста нет или рисовать нечем, ничего не делаем.
        if (painter == nullptr || text.isEmpty())
        {
            return;
        }

        // Берем текущий шрифт painter и делаем подпись оси заметнее.
        QFont font = painter->font();

        // setPointSizeF(...) задает размер шрифта в вещественных единицах,
        // а не только целым числом.
        //
        // std::max(...) здесь гарантирует, что подпись не станет слишком
        // маленькой даже если исходный шрифт painter был меньше.
        font.setPointSizeF(std::max(font.pointSizeF(), 10.5));
        font.setBold(true);

        // Применяем обновленный шрифт к painter.
        painter->setFont(font);

        // QFontMetricsF нужен, чтобы узнать, какой реальный размер занимает
        // подпись в текущем шрифте.
        const QFontMetricsF metrics(font);

        // boundingRect(...) дает прямоугольник, который нужен для текста.
        QRectF textRect = metrics.boundingRect(text);

        // Немного расширяем его, чтобы вокруг текста был внутренний воздух.
        //
        // adjust(left, top, right, bottom) изменяет границы прямоугольника:
        // - отрицательные значения слева/сверху расширяют его наружу;
        // - положительные справа/снизу тоже расширяют его.
        textRect.adjust(-6.0, -3.0, 6.0, 3.0);

        // anchor - это опорная точка, относительно которой будет
        // вычисляться фактический левый верхний угол подписи.
        QPointF topLeft = anchor;

        // По горизонтали размещаем подпись в зависимости от requested alignment.
        if (alignment & Qt::AlignRight)
        {
            // rx() возвращает ссылку на x-координату точки, что позволяет
            // изменить ее прямо "на месте".
            topLeft.rx() -= textRect.width();
        }
        else if (alignment & Qt::AlignHCenter)
        {
            topLeft.rx() -= textRect.width() * 0.5;
        }

        // Аналогично корректируем положение по вертикали.
        if (alignment & Qt::AlignBottom)
        {
            // ry() аналогично rx(), но для y-координаты.
            topLeft.ry() -= textRect.height();
        }
        else if (alignment & Qt::AlignVCenter)
        {
            topLeft.ry() -= textRect.height() * 0.5;
        }

        // Ограничиваем подпись пределами viewport, чтобы она не уезжала
        // за границы видимой области.
        //
        // viewport()->rect() возвращает прямоугольник всей видимой области.
        // adjusted(...) дополнительно делает внутренний отступ от краев.
        const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(6.0, 6.0, -6.0, -6.0);

        // std::clamp(value, min, max) ограничивает значение диапазоном.
        // Благодаря этому верхний левый угол подписи всегда остается внутри
        // допустимой области viewport.
        topLeft.setX(std::clamp(topLeft.x(), viewportRect.left(), viewportRect.right() - textRect.width()));
        topLeft.setY(std::clamp(topLeft.y(), viewportRect.top(), viewportRect.bottom() - textRect.height()));

        // Переносим подготовленный прямоугольник в вычисленную позицию.
        textRect.moveTopLeft(topLeft);

        // Сначала рисуем светлую подложку, чтобы подпись не терялась на фоне
        // сетки и линий графика.

        // Qt::NoPen означает: контур у следующей фигуры рисовать не нужно.
        painter->setPen(Qt::NoPen);

        // setBrush(...) задает кисть заливки.
        // Цвет с альфа-каналом 220 делает подложку слегка прозрачной.
        painter->setBrush(QColor(255, 255, 255, 220));

        // drawRoundedRect(...) рисует скругленный прямоугольник под текстом.
        // Последние два аргумента - радиусы скругления углов.
        painter->drawRoundedRect(textRect, 5.0, 5.0);

        // Затем рисуем сам текст по центру подложки.
        painter->setPen(QColor("#203146"));

        // Здесь снова используется adjust(...), но уже для внутреннего
        // текстового поля внутри подложки.
        //
        // Qt::AlignCenter означает центрирование текста внутри этого
        // внутреннего прямоугольника.
        painter->drawText(textRect.adjusted(6.0, 3.0, -6.0, -3.0), Qt::AlignCenter, text);

        // Возвращаем рабочий цвет пера для последующей отрисовки осей.
        painter->setPen(QColor("#4b647b"));
    }

    void DrawEventMarker(QPainter* painter)
    {
        // Для отрисовки маркера нужны:
        // - painter;
        // - владелец;
        // - сам chart;
        // - основная серия, относительно которой выполняется отображение
        //   координат данных в координаты графика.
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->lineSeries_ == nullptr)
        {
            return;
        }

        // xValue - координата времени/оси X, в которой нужно провести
        // вертикальный маркер.
        const double xValue = owner_->eventMarkerX_;
        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();

        // Если маркер находится вне текущей видимой области, рисовать его
        // не нужно.
        if (xValue < xMin || xValue > xMax)
        {
            return;
        }

        // Берем верхнюю и нижнюю точки вертикального маркера в координатах
        // данных и переводим их сначала в chart, а затем во viewport.
        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();
        const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->lineSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->lineSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        // Маркер столкновения рисуется пунктирной линией.
        QPen markerPen(QColor("#d11f4e"));
        markerPen.setWidthF(1.3);

        // Qt::DashLine означает пунктирный стиль линии.
        markerPen.setStyle(Qt::DashLine);
        painter->setPen(markerPen);
        painter->drawLine(QLineF(topViewPos, bottomViewPos));

        // Тернарный оператор ? : здесь выбирает текст подписи:
        // - если собственная подпись пуста, используем значение по умолчанию;
        // - иначе используем заданный текст.
        DrawAxisLabel(
            painter,
            owner_->eventMarkerLabel_.isEmpty() ? QStringLiteral("|r| = Rземли") : owner_->eventMarkerLabel_,
            QPointF(topViewPos.x() + 10.0, topViewPos.y() + 22.0),
            Qt::AlignLeft | Qt::AlignTop);

        // После временной настройки пера под маркер возвращаем стандартный
        // рабочий цвет для дальнейшей отрисовки.
        painter->setPen(QColor("#4b647b"));
    }

    void DrawEventPoint(QPainter* painter)
    {
        // Те же базовые проверки, что и для вертикального маркера.
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->lineSeries_ == nullptr)
        {
            return;
        }

        // Берем верхнюю и нижнюю точки вертикального маркера в координатах
        // данных и переводим их сначала в chart, а затем во viewport.
        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();
        const QPointF point = owner_->eventPoint_;

        // Если сама точка лежит вне текущего окна просмотра, показывать ее
        // на графике нет смысла.
        if (point.x() < xMin || point.x() > xMax || point.y() < yMin || point.y() > yMax)
        {
            return;
        }

        // Переводим точку из координат данных в координаты viewport.
        const QPointF chartPos = chart()->mapToPosition(point, owner_->lineSeries_);
        const QPoint viewPos = mapFromScene(chart()->mapToScene(chartPos));

        // Точечный маркер рисуем без контура и с красной заливкой.
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#d11f4e"));

        // drawEllipse(center, rx, ry) рисует эллипс/круг с центром в точке
        // viewPos и заданными радиусами по осям.
        painter->drawEllipse(QPointF(viewPos), 4.5, 4.5);

        DrawAxisLabel(
            painter,
            owner_->eventPointLabel_.isEmpty() ? QStringLiteral("|r| = Rземли") : owner_->eventPointLabel_,
            QPointF(viewPos.x() + 10.0, viewPos.y() - 10.0),
            Qt::AlignLeft | Qt::AlignBottom);

        // Возвращаем стандартный цвет пера.
        painter->setPen(QColor("#4b647b"));
    }

    bool HandleNativeGesture(QNativeGestureEvent* event)
    {
        // Если события нет или владелец недоступен, обработка невозможна.
        if (event == nullptr || owner_ == nullptr)
        {
            return false;
        }

        // position() дает точку жеста в координатах viewport.
        const QPointF viewportPos = event->position();

        // На macOS некоторые жесты приходят именно как native gestures.
        //
        // switch(...) выбирает одну из веток обработки в зависимости
        // от типа жеста.
        switch (event->gestureType())
        {
        case Qt::ZoomNativeGesture:
        {
            // value() - численный параметр жеста zoom.
            const double delta = event->value();

            // qFuzzyIsNull(...) проверяет, не слишком ли близко значение к нулю
            // с учетом погрешностей вещественных чисел.
            if (!std::isfinite(delta) || qFuzzyIsNull(delta))
            {
                event->accept();
                return true;
            }

            // exp(-delta) переводит "сырой" жестовый delta в более плавный
            // коэффициент zoom.
            const double factor = std::clamp(std::exp(-delta), kMinZoomFactor, kMaxZoomFactor);

            // ZoomAt(...) масштабирует график вокруг точки жеста viewportPos.
            owner_->ZoomAt(viewportPos, factor);
            event->accept();
            return true;
        }
        case Qt::SmartZoomNativeGesture:
            // Smart Zoom используем как команду вернуть исходный масштаб.
            owner_->ResetView();
            event->accept();
            return true;
        case Qt::PanNativeGesture:
        case Qt::SwipeNativeGesture:
            // delta() в этих жестах несет информацию о смещении.
            owner_->PanByPixels(event->delta());
            event->accept();
            return true;
        case Qt::BeginNativeGesture:
        case Qt::EndNativeGesture:
            // Начало и конец жеста сами по себе нам не нужны, но событие
            // считаем обработанным.
            event->accept();
            return true;
        default:
            // default срабатывает для всех остальных типов жестов, которые
            // данный класс специально не обрабатывает.
            return false;
        }
    }

    bool HandleGesture(QGestureEvent* event)
    {
        // Если самого gesture event нет или владелец недоступен,
        // обработка невозможна.
        if (event == nullptr || owner_ == nullptr)
        {
            return false;
        }

        // Пытаемся извлечь из события именно pinch-жест.
        QGesture* gesture = event->gesture(Qt::PinchGesture);
        if (gesture == nullptr)
        {
            return false;
        }

        // static_cast здесь безопасен, потому что выше уже проверено,
        // что жест имеет тип Qt::PinchGesture.
        auto* pinch = static_cast<QPinchGesture*>(gesture);

        // changeFlags() показывает, какие именно параметры pinch-жеста
        // изменились на данной итерации.
        //
        // Нас интересует именно изменение scale factor.
        if ((pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) == 0)
        {
            // accept(gesture) помечает обработанным конкретно этот жест
            // внутри общего gesture event.
            event->accept(gesture);
            return true;
        }

        // scaleFactor() - коэффициент масштабирования pinch-жеста.
        const double scale = pinch->scaleFactor();
        if (!std::isfinite(scale) || scale <= 0.0)
        {
            event->accept(gesture);
            return true;
        }

        // Инвертируем scale, чтобы привести его к логике ZoomAt(...),
        // затем ограничиваем диапазон коэффициента через std::clamp(...).
        const double factor = std::clamp(1.0 / scale, kMinZoomFactor, kMaxZoomFactor);

        // centerPoint() возвращает центр pinch-жеста, то есть опорную точку,
        // относительно которой и выполняется масштабирование.
        owner_->ZoomAt(pinch->centerPoint(), factor);
        event->accept(gesture);
        return true;
    }

    // QPointer хранит ссылку на владеющий PlotChartWidget.
    // Его преимущество перед обычным сырым указателем в том, что если объект
    // owner_ будет уничтожен, QPointer автоматически станет nullptr.
    QPointer<PlotChartWidget> owner_;
};

// ----------------------------------------------------------------------------
// КОНСТРУКТОР PlotChartWidget
// ----------------------------------------------------------------------------
// Назначение:
// Создать карточку одного графика с заголовком, пустым состоянием и
// полностью настроенным QChart/QChartView.
//
// Принимает:
// - title      - текст заголовка карточки графика;
// - xAxisLabel - подпись оси X;
// - yAxisLabel - подпись оси Y;
// - lineColor  - цвет основной линии графика;
// - parent     - родительский Qt-виджет.
// ----------------------------------------------------------------------------
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
    // Список инициализации после ":" заполняет поля класса еще до входа
    // в тело конструктора.
    //
    // Это нормальный и предпочтительный способ инициализации полей в C++.

    // objectName нужен для стилизации всей карточки через stylesheet.
    setObjectName("plotCard");
    setMinimumHeight(220);

    // Основной вертикальный layout карточки.
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    // Заголовок графика.
    titleLabel_ = new QLabel(title, this);
    titleLabel_->setObjectName("plotTitle");

    // Пустое состояние показывается до тех пор, пока реальные данные
    // не будут переданы через SetData(...).
    emptyStateLabel_ = new QLabel("Нет данных для отображения", this);
    emptyStateLabel_->setObjectName("plotEmptyState");
    emptyStateLabel_->setAlignment(Qt::AlignCenter);
    emptyStateLabel_->setWordWrap(true);
    emptyStateLabel_->setMinimumHeight(170);

    // setStyleSheet(...) задает локальный Qt stylesheet прямо для этого
    // виджета. Здесь он используется для аккуратного оформления пустого
    // состояния без вынесения правил в общий большой stylesheet окна.
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

    // Создаем сам QChart и настраиваем его визуальный стиль.
    chart_ = new QChart();

    // ChartThemeLight включает светлую базовую тему Qt Charts.
    chart_->setTheme(QChart::ChartThemeLight);

    // BackgroundBrush задает фон всего chart-объекта.
    chart_->setBackgroundBrush(QBrush(Qt::white));
    chart_->setBackgroundRoundness(0.0);

    // Plot area - это внутренняя область, где реально рисуются оси и серия.
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setPlotAreaBackgroundBrush(QColor(248, 248, 250));

    // Анимации отключены, чтобы график обновлялся быстрее и без лишней
    // нагрузки при частой перерисовке.
    chart_->setAnimationOptions(QChart::NoAnimation);

    // Легенду скрываем, потому что каждый виджет показывает только один
    // график и она здесь не нужна.
    chart_->legend()->setVisible(false);

    // Убираем внешние отступы самого chart.
    chart_->setMargins(QMargins(0, 0, 0, 0));

    // У QChart есть собственный внутренний layout.
    // Если он существует, тоже убираем у него отступы.
    if (chart_->layout() != nullptr)
    {
        chart_->layout()->setContentsMargins(0, 0, 0, 0);
    }

    // Создаем числовые оси.
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

    // До получения реальных данных ставим базовый диапазон 0..1.
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(0.0, 1.0);

    // addAxis(...) физически добавляет оси в chart и привязывает их
    // к заданным сторонам: нижней и левой.
    chart_->addAxis(axisX_, Qt::AlignBottom);
    chart_->addAxis(axisY_, Qt::AlignLeft);

    // Отдельные серии для осей Ox и Oy.
    // Они нужны, чтобы оси рисовались под графиком как полноценные линии,
    // а не только как вручную дорисованные элементы.
    axisXSeries_ = new QLineSeries();
    axisYSeries_ = new QLineSeries();
    QPen axisPen(QColor("#4b647b"));
    axisPen.setWidthF(1.35);
    axisXSeries_->setPen(axisPen);
    axisYSeries_->setPen(axisPen);

    // Основная серия самого графика.
    lineSeries_ = new QLineSeries();
    QPen linePen(lineColor_);
    linePen.setWidthF(2.1);
    lineSeries_->setPen(linePen);

    chart_->addSeries(axisXSeries_);
    chart_->addSeries(axisYSeries_);
    chart_->addSeries(lineSeries_);

    // attachAxis(...) связывает серии с конкретными осями.
    // После этого Qt Charts знает, как интерпретировать координаты точек
    // этих серий относительно шкал X и Y.
    axisXSeries_->attachAxis(axisX_);
    axisXSeries_->attachAxis(axisY_);
    axisYSeries_->attachAxis(axisX_);
    axisYSeries_->attachAxis(axisY_);
    lineSeries_->attachAxis(axisX_);
    lineSeries_->attachAxis(axisY_);

    // Используем не обычный QChartView, а наш расширенный InteractiveChartView.
    chartView_ = new InteractiveChartView(this, chart_, this);
    chartView_->setObjectName("plotChartView");
    chartView_->setRenderHint(QPainter::Antialiasing, false);
    chartView_->setMinimumHeight(170);
    chartView_->setFrameShape(QFrame::NoFrame);
    chartView_->setContentsMargins(0, 0, 0, 0);

    // Второй аргумент 1 в addWidget(..., 1) - это stretch factor.
    // Он говорит layout, что пустое состояние и график должны получать
    // растягиваемое вертикальное пространство.
    layout->addWidget(titleLabel_);
    layout->addWidget(emptyStateLabel_, 1);
    layout->addWidget(chartView_, 1);

    // Пока данных нет, график скрыт, а видна только заглушка "Нет данных".
    chartView_->hide();

    // Инициализируем оси в базовый диапазон.
    RefreshAxes();
}

// Очищает график и переводит виджет обратно в пустое состояние.
void PlotChartWidget::Clear()
{
    // Очищаем полный набор исходных точек.
    fullPoints_.clear();

    // Очищаем все видимые серии графика.
    axisXSeries_->clear();
    axisYSeries_->clear();
    lineSeries_->clear();

    // Убираем любые маркеры событий, если они были установлены.
    ClearEventMarker();

    // Скрываем сам график.
    chartView_->hide();
    // После очистки снова показываем текст "Нет данных для отображения".
    emptyStateLabel_->show();

    // Сбрасываем флаг наличия данных.
    hasData_ = false;

    // Возвращаем диапазоны "по умолчанию".
    defaultXMin_ = 0.0;
    defaultXMax_ = 1.0;
    defaultYMin_ = 0.0;
    defaultYMax_ = 1.0;

    // Немедленно ставим базовые пределы осей.
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(0.0, 1.0);

    // Значение -1.0 здесь используется как служебный признак:
    // "предыдущий шаг делений еще не инициализирован корректно".
    xTickStep_ = -1.0;
    yTickStep_ = -1.0;
    // Возвращаем оси к стандартному диапазону и базовым настройкам.
    RefreshAxes();
}

// ----------------------------------------------------------------------------
// МЕТОД SetData
// ----------------------------------------------------------------------------
// Назначение:
// Передать графику новый набор точек и пересчитать диапазоны осей.
//
// Принимает:
// - points - набор точек графика в виде пар (x, y).
//
// Здесь же:
// - скрывается пустое состояние;
// - вычисляются минимумы и максимумы;
// - добавляются отступы от краев;
// - для траекторий выравнивается геометрический масштаб по осям.
// ----------------------------------------------------------------------------
void PlotChartWidget::SetData(const QVector<QPointF>& points)
{
    // Сохраняем полный набор исходных точек.
    // Он нужен не только для первой отрисовки, но и для последующих zoom/pan,
    // когда может потребоваться пересобирать только видимую часть.
    fullPoints_ = points;

    // На всякий случай очищаем текущую отображаемую серию перед дальнейшей
    // настройкой.
    lineSeries_->clear();

    // Если данных нет, проще всего перейти в стандартное пустое состояние.
    if (points.isEmpty())
    {
        Clear();
        return;
    }

    // Если данные есть, скрываем текстовую заглушку и показываем сам график.
    emptyStateLabel_->hide();
    chartView_->show();

    // front() возвращает первый элемент контейнера.
    // С него удобно начать инициализацию минимумов/максимумов.
    double xMin = points.front().x();
    double xMax = points.front().x();
    double yMin = points.front().y();
    double yMax = points.front().y();

    // Находим фактические минимумы и максимумы по обеим осям.
    for (const QPointF& point : points)
    {
        xMin = std::min(xMin, point.x());
        xMax = std::max(xMax, point.x());
        yMin = std::min(yMin, point.y());
        yMax = std::max(yMax, point.y());
    }

    // Вычисляем полный размах данных по каждой оси.
    double xSpan = xMax - xMin;
    double ySpan = yMax - yMin;

    // Если диапазон выродился, добавляем симметричный запас вручную.
    if (!(xSpan > 0.0) || !std::isfinite(xSpan))
    {
        // Когда все x почти одинаковы, берем запас либо 1.0, либо 5% от
        // величины координаты - что больше.
        const double pad = std::max(1.0, std::fabs(xMin) * 0.05);
        xMin -= pad;
        xMax += pad;
    }
    else
    {
        // В обычном случае добавляем небольшой относительный отступ в 12%,
        // чтобы линия не прилипала к краям окна.
        const double pad = std::max(1e-9, xSpan * 0.12);
        xMin -= pad;
        xMax += pad;
    }

    if (!(ySpan > 0.0) || !std::isfinite(ySpan))
    {
        // Полностью аналогичная логика для оси Y:
        // если диапазон вырожден или некорректен, вручную добавляем
        // симметричный запас вокруг текущего значения.
        const double pad = std::max(1.0, std::fabs(yMin) * 0.05);
        yMin -= pad;
        yMax += pad;
    }
    else
    {
        // В обычной ситуации тоже добавляем небольшой вертикальный отступ,
        // чтобы линия графика не упиралась вплотную в верхнюю и нижнюю границы.
        const double pad = std::max(1e-9, ySpan * 0.12);
        yMin -= pad;
        yMax += pad;
    }

    // Для пространственных траекторий (не временных графиков) стараемся
    // сохранить одинаковый геометрический масштаб по осям, чтобы круги не
    // выглядели искусственно сплющенными только из-за размеров окна.
    if (xAxisLabel_ != "t" && chartView_ != nullptr)
    {
        // Берем текущий размер viewport, в который реально рисуется график.
        const QRect viewRect = chartView_->viewport()->rect();

        // Приводим размеры к double и страхуемся от нулевых значений.
        const double viewWidth = std::max(1.0, static_cast<double>(viewRect.width()));
        const double viewHeight = std::max(1.0, static_cast<double>(viewRect.height()));

        // Реальный диапазон данных после добавления отступов.
        const double spanX = std::max(1e-9, xMax - xMin);
        const double spanY = std::max(1e-9, yMax - yMin);

        // Выбираем общий масштаб так, чтобы по обеим осям единицы измерения
        // были согласованы геометрически.
        const double scale = std::max(spanX / viewWidth, spanY / viewHeight);

        // Пересчитываем диапазоны X и Y под найденный общий масштаб.
        const double adjustedSpanX = scale * viewWidth;
        const double adjustedSpanY = scale * viewHeight;

        // Центры текущего диапазона.
        const double centerX = 0.5 * (xMin + xMax);
        const double centerY = 0.5 * (yMin + yMax);

        // Строим новый диапазон симметрично относительно центра.
        xMin = centerX - adjustedSpanX * 0.5;
        xMax = centerX + adjustedSpanX * 0.5;
        yMin = centerY - adjustedSpanY * 0.5;
        yMax = centerY + adjustedSpanY * 0.5;
    }

    // Запоминаем "домашний" диапазон, к которому потом можно вернуться
    // через ResetView().
    hasData_ = true;
    defaultXMin_ = xMin;
    defaultXMax_ = xMax;
    defaultYMin_ = yMin;
    defaultYMax_ = yMax;

    // Применяем подготовленные диапазоны и обновляем оси/серии.
    ApplyAxisRange(xMin, xMax, yMin, yMax);
}

// Устанавливает вертикальный маркер события на графике координат.
void PlotChartWidget::SetEventMarker(double xValue, const QString& label)
{
    // std::isfinite(...) проверяет, является ли значение нормальным конечным
    // числом, а не NaN и не бесконечностью.
    hasEventMarker_ = std::isfinite(xValue);

    // Сохраняем положение маркера и его подпись.
    eventMarkerX_ = xValue;
    eventMarkerLabel_ = label;

    // update() просит Qt перерисовать viewport при ближайшей возможности.
    // Это нужно, чтобы маркер появился сразу после изменения данных.
    if (chartView_ != nullptr)
    {
        chartView_->viewport()->update();
    }
}

// Устанавливает точечный маркер события.
void PlotChartWidget::SetEventPoint(const QPointF& point, const QString& label)
{
    // Маркер точки считаем корректным только если обе координаты конечны.
    hasEventPoint_ = std::isfinite(point.x()) && std::isfinite(point.y());

    // Сохраняем координаты точки и ее подпись.
    eventPoint_ = point;
    eventPointLabel_ = label;
    if (chartView_ != nullptr)
    {
        chartView_->viewport()->update();
    }
}

// Полностью очищает информацию о маркерах событий.
void PlotChartWidget::ClearEventMarker()
{
    // Полностью выключаем флаг вертикального маркера и очищаем его данные.
    hasEventMarker_ = false;
    eventMarkerX_ = 0.0;

    // clear() очищает строку QString.
    eventMarkerLabel_.clear();

    // Аналогично очищаем точечный маркер.
    hasEventPoint_ = false;
    eventPoint_ = QPointF();
    eventPointLabel_.clear();
    if (chartView_ != nullptr)
    {
        chartView_->viewport()->update();
    }
}

// ----------------------------------------------------------------------------
// МЕТОД ApplyAxisRange
// ----------------------------------------------------------------------------
// Назначение:
// Централизованно применить новый диапазон осей и сразу согласованно
// обновить:
// - шаг делений;
// - подписи осей;
// - серии осей;
// - основную линию графика.
//
// Принимает:
// - xMin - левая граница диапазона оси X;
// - xMax - правая граница диапазона оси X;
// - yMin - нижняя граница диапазона оси Y;
// - yMax - верхняя граница диапазона оси Y.
// ----------------------------------------------------------------------------
void PlotChartWidget::ApplyAxisRange(double xMin, double xMax, double yMin, double yMax)
{
    // Если диапазон по X некорректен, подменяем его безопасным значением 0..1.
    if (!(xMax > xMin) || !std::isfinite(xMin) || !std::isfinite(xMax))
    {
        xMin = 0.0;
        xMax = 1.0;
    }

    // То же самое делаем для диапазона по Y.
    if (!(yMax > yMin) || !std::isfinite(yMin) || !std::isfinite(yMax))
    {
        yMin = 0.0;
        yMax = 1.0;
    }

    // std::fabs(...) возвращает модуль числа типа double.
    // Здесь так удобнее получить длину диапазона оси.
    const double xSpan = std::fabs(xMax - xMin);
    const double ySpan = std::fabs(yMax - yMin);

    // Вычисляем новые шаги делений для обеих осей.
    double newXTickStep = StableTickStep(xTickStep_, xSpan, 5);
    double newYTickStep = StableTickStep(yTickStep_, ySpan, 5);

    // Если расчет шага дал некорректный результат, откатываемся к 1.0.
    if (!(newXTickStep > 0.0) || !std::isfinite(newXTickStep))
    {
        newXTickStep = 1.0;
    }
    if (!(newYTickStep > 0.0) || !std::isfinite(newYTickStep))
    {
        newYTickStep = 1.0;
    }

    // Определяем, сколько знаков после запятой нужно показывать
    // для подписей каждой оси.
    const int xDecimals = DecimalsForStep(newXTickStep);
    const int yDecimals = DecimalsForStep(newYTickStep);

    // Используем динамические деления оси: они задаются через anchor + interval.
    axisX_->setTickType(QValueAxis::TicksDynamic);
    axisY_->setTickType(QValueAxis::TicksDynamic);

    // Tick anchor - это опорная точка сетки делений.
    // Здесь в качестве опорной точки берется 0.0.
    axisX_->setTickAnchor(0.0);
    axisY_->setTickAnchor(0.0);

    // Tick interval задает расстояние между крупными делениями.
    axisX_->setTickInterval(newXTickStep);
    axisY_->setTickInterval(newYTickStep);

    // MinorTickCount задает число промежуточных делений между крупными.
    axisX_->setMinorTickCount(3);
    axisY_->setMinorTickCount(3);

    // Держим подписи осей прямыми, без наклона.
    axisX_->setLabelsAngle(0);
    axisY_->setLabelsAngle(0);

    // QString::asprintf(...) формирует строку формата по аналогии с printf.
    // Например, если xDecimals == 2, получится строка "%.2f".
    axisX_->setLabelFormat(QString::asprintf("%%.%df", xDecimals));
    axisY_->setLabelFormat(QString::asprintf("%%.%df", yDecimals));

    // Отключаем автоматическое усечение подписей осей.
    axisX_->setTruncateLabels(false);
    axisY_->setTruncateLabels(false);

    // Сохраняем выбранные шаги как текущие.
    xTickStep_ = newXTickStep;
    yTickStep_ = newYTickStep;

    // Применяем новый числовой диапазон к осям.
    axisX_->setRange(xMin, xMax);
    axisY_->setRange(yMin, yMax);
    // После изменения диапазона пересобираем серии осей и саму кривую.
    UpdateAxisSeries();
    RefreshLineSeries();
}

// ----------------------------------------------------------------------------
// МЕТОД BuildRenderedPoints
// ----------------------------------------------------------------------------
// Назначение:
// Построить именно тот набор точек, который нужно реально отправить
// на экран в текущем состоянии графика.
//
// Возвращает:
// - либо полный набор точек, если он и так небольшой;
// - либо только видимую часть;
// - либо прореженную версию, если точек слишком много.
// ----------------------------------------------------------------------------
QVector<QPointF> PlotChartWidget::BuildRenderedPoints() const
{
    // Если исходный набор уже достаточно мал, возвращаем его целиком.
    if (fullPoints_.size() <= kMaxRenderedPoints)
    {
        return fullPoints_;
    }

    // Если X не монотонен, быстро выделить видимую часть через бинарный поиск
    // нельзя, поэтому просто прореживаем весь набор целиком.
    if (!IsMonotonicByX(fullPoints_))
    {
        return DownsamplePoints(fullPoints_);
    }

    // Текущий видимый диапазон по оси X.
    const double visibleXMin = axisX_->min();
    const double visibleXMax = axisX_->max();

    // Для монотонного по X набора точек можно быстро выделить только видимую
    // часть через бинарный поиск.

    // cbegin() / cend() возвращают константные итераторы на начало и конец
    // контейнера.
    //
    // std::lower_bound(...) ищет первую точку, у которой x уже не меньше
    // visibleXMin.
    const auto leftIt = std::lower_bound(
        fullPoints_.cbegin(), fullPoints_.cend(), visibleXMin, [](const QPointF& point, double value) 
        {
            return point.x() < value;
        });

    // std::upper_bound(...) ищет первую точку, у которой x уже строго больше
    // visibleXMax.
    const auto rightIt = std::upper_bound(
        fullPoints_.cbegin(), fullPoints_.cend(), visibleXMax, [](double value, const QPointF& point) 
        {
            return value < point.x();
        });

    // std::distance(...) переводит положение итераторов в числовые индексы.
    int firstIndex = static_cast<int>(std::distance(fullPoints_.cbegin(), leftIt));
    int lastIndex = static_cast<int>(std::distance(fullPoints_.cbegin(), rightIt)) - 1;

    // Чуть расширяем диапазон влево и вправо, чтобы линия не обрывалась
    // слишком резко на границе видимой области.
    firstIndex = std::max(0, firstIndex - 1);
    lastIndex = std::min(static_cast<int>(fullPoints_.size()) - 1, std::max(firstIndex, lastIndex + 1));

    // Собираем видимый фрагмент в отдельный контейнер.
    QVector<QPointF> visiblePoints;

    // reserve(...) заранее выделяет память под нужное число точек.
    visiblePoints.reserve(lastIndex - firstIndex + 1);

    // append(...) добавляет очередную точку в конец контейнера.
    for (int index = firstIndex; index <= lastIndex; ++index)
    {
        visiblePoints.append(fullPoints_[index]);
    }

    // Если после отсечения видимой области точек стало достаточно мало,
    // дополнительное прореживание уже не требуется.
    if (visiblePoints.size() <= kMaxRenderedPoints)
    {
        return visiblePoints;
    }

    // Иначе прореживаем уже только видимый фрагмент.
    return DownsamplePoints(visiblePoints);
}

// Обновляет основную серию линии на основе текущего диапазона осей.
void PlotChartWidget::RefreshLineSeries()
{
    // Если данных нет, серия тоже должна быть пустой.
    if (fullPoints_.isEmpty())
    {
        lineSeries_->clear();
        return;
    }

    // replace(...) заменяет содержимое серии новым набором точек целиком.
    // В отличие от последовательных append(...) это удобно для полной
    // синхронизации серии с текущим набором отображаемых данных.
    lineSeries_->replace(BuildRenderedPoints());
}

// Перестраивает линии внутренних осей Ox и Oy.
void PlotChartWidget::UpdateAxisSeries()
{
    // Без самих серий осей обновлять нечего.
    if (axisXSeries_ == nullptr || axisYSeries_ == nullptr)
    {
        return;
    }

    // Сначала очищаем старые линии осей.
    axisXSeries_->clear();
    axisYSeries_->clear();

    // Берем текущие числовые границы осей.
    const double xMin = axisX_->min();
    const double xMax = axisX_->max();
    const double yMin = axisY_->min();
    const double yMax = axisY_->max();

    // Рисуем ось Ox только если ноль по Y находится в видимом диапазоне.
    if (yMin <= 0.0 && yMax >= 0.0)
    {
        // Линия оси Ox задается всего двумя точками:
        // от (xMin, 0) до (xMax, 0).
        axisXSeries_->append(xMin, 0.0);
        axisXSeries_->append(xMax, 0.0);
    }

    // Рисуем ось Oy только если ноль по X находится в видимом диапазоне.
    if (xMin <= 0.0 && xMax >= 0.0)
    {
        // Линия оси Oy аналогично задается двумя точками:
        // от (0, yMin) до (0, yMax).
        axisYSeries_->append(0.0, yMin);
        axisYSeries_->append(0.0, yMax);
    }
}

// Просто повторно применяет текущие диапазоны через единый код настройки осей.
void PlotChartWidget::RefreshAxes()
{
    // Вместо дублирования настроек используем уже существующий единый метод,
    // который умеет согласованно обновить и оси, и серии.
    ApplyAxisRange(axisX_->min(), axisX_->max(), axisY_->min(), axisY_->max());
}

// ----------------------------------------------------------------------------
// МЕТОД PanByPixels
// ----------------------------------------------------------------------------
// Назначение:
// Сдвинуть текущую область просмотра графика на величину жеста/прокрутки,
// измеренную в пикселях viewport.
//
// Принимает:
// - pixelDelta - смещение в пикселях по осям viewport.
// ----------------------------------------------------------------------------
void PlotChartWidget::PanByPixels(const QPointF& pixelDelta)
{
    // Панорамирование возможно только если:
    // - у графика есть данные;
    // - chart создан;
    // - смещение задано конечными числами.
    if (!hasData_ || chart_ == nullptr || !(std::isfinite(pixelDelta.x()) && std::isfinite(pixelDelta.y())))
    {
        return;
    }

    // plotArea() возвращает прямоугольник реальной области построения графика
    // без внешних полей и заголовка.
    const QRectF plotArea = chart_->plotArea();

    // Если область графика слишком мала или некорректна, пересчет смещения
    // делать бессмысленно.
    if (!(plotArea.width() > 1.0) || !(plotArea.height() > 1.0))
    {
        return;
    }

    // Берем текущие числовые границы осей.
    const double currentXMin = axisX_->min();
    const double currentXMax = axisX_->max();
    const double currentYMin = axisY_->min();
    const double currentYMax = axisY_->max();

    // Вычисляем длину текущего диапазона по каждой оси.
    const double currentXSpan = currentXMax - currentXMin;
    const double currentYSpan = currentYMax - currentYMin;

    // Если диапазон вырожден, двигать окно просмотра нечем.
    if (!(currentXSpan > 0.0) || !(currentYSpan > 0.0))
    {
        return;
    }

    // Переводим смещение в пикселях в смещение по координатам осей.
    //
    // Знак минус у dx нужен потому, что движение жеста и движение окна
    // по оси X воспринимаются в противоположных направлениях.
    const double dx = -(pixelDelta.x() / plotArea.width()) * currentXSpan;

    // По Y знак оставлен прямым с учетом принятой логики экранных координат
    // и желаемого поведения панорамирования.
    const double dy = (pixelDelta.y() / plotArea.height()) * currentYSpan;

    // Применяем сдвинутый диапазон через единый метод настройки осей.
    ApplyAxisRange(currentXMin + dx, currentXMax + dx, currentYMin + dy, currentYMax + dy);
}

// ----------------------------------------------------------------------------
// МЕТОД ResetView
// ----------------------------------------------------------------------------
// Назначение:
// Вернуть график к исходному диапазону после зума или панорамирования.
// ----------------------------------------------------------------------------
void PlotChartWidget::ResetView()
{
    // Если данных нет, вместо "сброса вида" просто возвращаем пустое состояние.
    if (!hasData_)
    {
        Clear();
        return;
    }

    // Сбрасываем закэшированные шаги делений, чтобы при восстановлении вида
    // оси были пересчитаны заново, а не удерживали старый zoom-зависимый шаг.
    xTickStep_ = -1.0;
    yTickStep_ = -1.0;

    // Возвращаемся к диапазону, сохраненному в SetData(...) как "домашний".
    ApplyAxisRange(defaultXMin_, defaultXMax_, defaultYMin_, defaultYMax_);
}

// ----------------------------------------------------------------------------
// МЕТОД ZoomAt
// ----------------------------------------------------------------------------
// Назначение:
// Выполнить zoom относительно конкретной точки viewport.
//
// Это важный момент: zoom происходит не "в центр окна", а вокруг точки
// под пальцами/курсором, что делает взаимодействие естественнее.
//
// Принимает:
// - viewportPos - точка жеста/курсора в координатах viewport;
// - factor      - коэффициент масштабирования.
// ----------------------------------------------------------------------------
void PlotChartWidget::ZoomAt(const QPointF& viewportPos, double factor)
{
    // Зум имеет смысл только при наличии данных, корректного положительного
    // коэффициента и существующих chart/chartView.
    if (!hasData_ || !(factor > 0.0) || !std::isfinite(factor) || chartView_ == nullptr || chart_ == nullptr)
    {
        return;
    }

    // Переводим координату из viewport в сцену, затем в координаты самого
    // графика, а затем в реальные численные координаты данных.
    const QPointF scenePos = chartView_->mapToScene(viewportPos.toPoint());
    const QPointF chartPos = chart_->mapFromScene(scenePos);

    // Проверяем, попадает ли точка жеста в реальную область построения графика.
    const QRectF plotArea = chart_->plotArea();
    if (!plotArea.contains(chartPos))
    {
        return;
    }

    // mapToValue(...) переводит экранную точку в реальные координаты данных.
    // Именно эта точка становится anchor-точкой zoom.
    const QPointF anchor = chart_->mapToValue(chartPos, lineSeries_);

    // Текущий числовой диапазон осей.
    const double currentXMin = axisX_->min();
    const double currentXMax = axisX_->max();
    const double currentYMin = axisY_->min();
    const double currentYMax = axisY_->max();

    // Текущий размах окна просмотра по осям.
    const double currentXSpan = currentXMax - currentXMin;
    const double currentYSpan = currentYMax - currentYMin;
    if (!(currentXSpan > 0.0) || !(currentYSpan > 0.0))
    {
        return;
    }

    // Ограничиваем масштаб zoom снизу и сверху, чтобы нельзя было уйти
    // в бесконечно маленький или бесконечно большой диапазон.
    const double baseXSpan = std::max(1e-12, std::fabs(defaultXMax_ - defaultXMin_));
    const double baseYSpan = std::max(1e-12, std::fabs(defaultYMax_ - defaultYMin_));
    const double minXSpan = baseXSpan * 1e-6;
    const double minYSpan = baseYSpan * 1e-6;
    const double maxXSpan = baseXSpan * 1000.0;
    const double maxYSpan = baseYSpan * 1000.0;

    // std::clamp(...) ограничивает новый диапазон допустимыми пределами.
    double newXSpan = std::clamp(currentXSpan * factor, minXSpan, maxXSpan);
    double newYSpan = std::clamp(currentYSpan * factor, minYSpan, maxYSpan);

    if (!(newXSpan > 0.0) || !(newYSpan > 0.0))
    {
        return;
    }

    // Определяем, в какой относительной позиции внутри текущего окна
    // находится anchor-точка.
    const double xRatio = (anchor.x() - currentXMin) / currentXSpan;
    const double yRatio = (anchor.y() - currentYMin) / currentYSpan;

    // Дополнительно ограничиваем отношения диапазоном [0; 1], чтобы избежать
    // выхода за разумные границы при пограничных численных ситуациях.
    const double clampedXRatio = std::clamp(xRatio, 0.0, 1.0);
    const double clampedYRatio = std::clamp(yRatio, 0.0, 1.0);

    // Формируем новый диапазон так, чтобы точка-якорь после zoom осталась
    // под тем же местом курсора/жеста.
    const double newXMin = anchor.x() - newXSpan * clampedXRatio;
    const double newXMax = newXMin + newXSpan;
    const double newYMin = anchor.y() - newYSpan * clampedYRatio;
    const double newYMax = newYMin + newYSpan;

    // Применяем получившийся диапазон.
    ApplyAxisRange(newXMin, newXMax, newYMin, newYMax);
}

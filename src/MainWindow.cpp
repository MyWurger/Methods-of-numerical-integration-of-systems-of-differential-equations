#include "MainWindow.h"

#include "TEuler.h"
#include "TRungeKutta.h"
#include "TSpaceCraft.h"

#include <QComboBox>
#include <QAbstractTableModel>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFocusEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
class MethodComboBox final : public QComboBox
{
public:
    explicit MethodComboBox(QWidget* parent = nullptr) : QComboBox(parent)
    {
        setProperty("popupOpen", false);
    }

protected:
    void showPopup() override
    {
        setProperty("popupOpen", true);
        style()->unpolish(this);
        style()->polish(this);
        update();
        QComboBox::showPopup();
    }

    void hidePopup() override
    {
        QComboBox::hidePopup();
        setProperty("popupOpen", false);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
};

class RichTextHeaderView final : public QHeaderView
{
public:
    explicit RichTextHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr)
        : QHeaderView(orientation, parent)
    {
    }

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override
    {
        const QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
        if (!text.contains('<'))
        {
            QHeaderView::paintSection(painter, rect, logicalIndex);
            return;
        }

        QStyleOptionHeader option;
        initStyleOption(&option);
        option.rect = rect;
        option.section = logicalIndex;
        option.text.clear();
        style()->drawControl(QStyle::CE_Header, &option, painter, this);

        QTextDocument document;
        document.setDefaultFont(font());
        document.setDocumentMargin(0.0);
        document.setDefaultStyleSheet(
            "body, div, span, sub { color: #203146; font-weight: 700; }"
            "sub { font-size: 70%; vertical-align: sub; }");
        document.setHtml(QString("<div style=\"text-align:center;\">%1</div>").arg(text));

        const QSizeF textSize = document.size();
        const QPointF topLeft(
            rect.x() + (rect.width() - textSize.width()) / 2.0,
            rect.y() + (rect.height() - textSize.height()) / 2.0);

        painter->save();
        painter->translate(topLeft);
        document.drawContents(painter);
        painter->restore();
    }
};

double Radius(const TVector& state)
{
    return std::sqrt(state[0] * state[0] + state[1] * state[1] + state[2] * state[2]);
}

double Speed(const TVector& state)
{
    return std::sqrt(state[3] * state[3] + state[4] * state[4] + state[5] * state[5]);
}

QString FormatNumericValue(double value)
{
    if (!std::isfinite(value))
    {
        return "не число";
    }

    if (value == 0.0)
    {
        return "0";
    }

    const double absValue = std::abs(value);
    if (absValue < 1e-4 || absValue >= 1e8)
    {
        return QString::number(value, 'e', 5);
    }

    QString text = QString::number(value, 'f', 6);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.')))
    {
        text.chop(1);
    }

    if (text == "-0")
    {
        return "0";
    }

    return text;
}

class SimulationTableModel final : public QAbstractTableModel
{
public:
    explicit SimulationTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent)
    {
    }

    void SetResult(const SimulationResult* result)
    {
        beginResetModel();
        result_ = result;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid() || result_ == nullptr)
        {
            return 0;
        }
        return static_cast<int>(result_->samples.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 9;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || result_ == nullptr || index.row() < 0 ||
            index.row() >= static_cast<int>(result_->samples.size()))
        {
            return {};
        }

        const SimulationSample& sample = result_->samples[static_cast<std::size_t>(index.row())];
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }

        if (role != Qt::DisplayRole)
        {
            return {};
        }

        switch (index.column())
        {
        case 0:
            return FormatNumericValue(sample.time);
        case 1:
            return FormatNumericValue(sample.state[0]);
        case 2:
            return FormatNumericValue(sample.state[1]);
        case 3:
            return FormatNumericValue(sample.state[2]);
        case 4:
            return FormatNumericValue(sample.state[3]);
        case 5:
            return FormatNumericValue(sample.state[4]);
        case 6:
            return FormatNumericValue(sample.state[5]);
        case 7:
            return FormatNumericValue(Radius(sample.state));
        case 8:
            return FormatNumericValue(Speed(sample.state));
        default:
            return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }

        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        {
            return QAbstractTableModel::headerData(section, orientation, role);
        }

        static const QStringList kHeaders = {
            "t, c",
            "x, м",
            "y, м",
            "z, м",
            "V<sub>x</sub>, м/с",
            "V<sub>y</sub>, м/с",
            "V<sub>z</sub>, м/с",
            "r, м",
            "|V|, м/с"};

        if (section < 0 || section >= kHeaders.size())
        {
            return {};
        }

        return kHeaders[section];
    }

private:
    const SimulationResult* result_ = nullptr;
};

int FullTableWidth(const QTableView* table)
{
    return table->verticalHeader()->width() + table->horizontalHeader()->length() + 2 * table->frameWidth();
}

int FullTableHeight(const QTableView* table)
{
    return table->horizontalHeader()->height() + table->verticalHeader()->length() + 2 * table->frameWidth();
}

QString ProjectTablesDirectoryPath()
{
    const QString projectRoot = QString::fromUtf8(MODELING_METHODS_LAB1_SOURCE_DIR);
    const QString tablesPath = QDir(projectRoot).absoluteFilePath("exports/tables");
    QDir().mkpath(tablesPath);
    return tablesPath;
}

QString BuildTableExportFileName()
{
    return QString("trajectory_table__%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

class NumericSpinBox final : public QDoubleSpinBox
{
public:
    explicit NumericSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent)
    {
        setDecimals(6);
        setSingleStep(1.0);
        setRange(-1.0e15, 1.0e15);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setAccelerated(true);
        setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        setGroupSeparatorShown(false);
    }

    void SetPlaceholderText(const QString& text)
    {
        if (auto* edit = lineEdit())
        {
            edit->setPlaceholderText(text);
        }
    }

    bool HasInput() const
    {
        const auto* edit = lineEdit();
        return edit != nullptr && !edit->text().trimmed().isEmpty();
    }

protected:
    QString textFromValue(double value) const override
    {
        return FormatNumericValue(value);
    }

    double valueFromText(const QString& text) const override
    {
        QString normalized = text.trimmed();
        normalized.replace(',', '.');

        bool ok = false;
        const double value = normalized.toDouble(&ok);
        return ok ? value : 0.0;
    }

    QValidator::State validate(QString& text, int& pos) const override
    {
        Q_UNUSED(pos);
        QString normalized = text.trimmed();
        if (normalized.isEmpty() || normalized == "-" || normalized == "+" || normalized == "." || normalized == ",")
        {
            return QValidator::Intermediate;
        }

        normalized.replace(',', '.');
        bool ok = false;
        normalized.toDouble(&ok);
        return ok ? QValidator::Acceptable : QValidator::Invalid;
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        const bool wasEmpty = !HasInput();
        QDoubleSpinBox::focusOutEvent(event);
        if (wasEmpty)
        {
            clear();
        }
    }

    void stepBy(int steps) override
    {
        if (!HasInput())
        {
            setValue(0.0);
        }
        QDoubleSpinBox::stepBy(steps);
    }
};

QWidget* CreateSpinField(QDoubleSpinBox*& spinBox, double initialValue, const QString& placeholder, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    spinBox = new NumericSpinBox(container);
    spinBox->setValue(initialValue);
    static_cast<NumericSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    auto* buttonsHost = new QWidget(container);
    buttonsHost->setObjectName("spinButtonColumn");
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(4);

    auto* plusButton = new QPushButton("+", buttonsHost);
    auto* minusButton = new QPushButton("-", buttonsHost);
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");
    plusButton->setFixedSize(30, 18);
    minusButton->setFixedSize(30, 18);

    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepBy(1); });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepBy(-1); });

    buttonsLayout->addWidget(plusButton);
    buttonsLayout->addWidget(minusButton);
    buttonsLayout->addStretch();

    layout->addWidget(spinBox, 1);
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);

    return container;
}

template <typename TWidget>
void ApplyInputPalette(TWidget* widget)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::Text, QColor("#102033"));
    palette.setColor(QPalette::WindowText, QColor("#102033"));
    palette.setColor(QPalette::ButtonText, QColor("#102033"));
    palette.setColor(QPalette::Highlight, QColor("#2a6df4"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7a8a9c"));
    widget->setPalette(palette);
}

bool HasSpinBoxInput(const QDoubleSpinBox* spinBox)
{
    const auto* numericSpinBox = dynamic_cast<const NumericSpinBox*>(spinBox);
    return numericSpinBox != nullptr && numericSpinBox->HasInput();
}
}

MainWindow::MainWindow(std::vector<IntegratorOption> integratorOptions, QWidget* parent)
    : QMainWindow(parent), integratorOptions_(std::move(integratorOptions))
{
    if (integratorOptions_.empty())
    {
        throw std::invalid_argument("Список интеграторов не должен быть пустым");
    }

    BuildUi();
    ApplyTheme();
    SetupSignals();
    ResetOutputs();
    ShowStatus("Готово к моделированию", true);
}

void MainWindow::BuildUi()
{
    setWindowTitle("Моделирование движения космического аппарата");
    resize(1560, 940);
    setMinimumSize(1320, 820);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(4);

    auto* controlCard = new QWidget(splitter);
    controlCard->setObjectName("controlCard");
    controlCard->setMinimumWidth(430);
    controlCard->setMaximumWidth(520);
    controlCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(18, 18, 18, 18);
    controlLayout->setSpacing(12);

    auto* titleLabel = new QLabel("Численное моделирование движения КА", controlCard);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setWordWrap(true);

    auto* controlTabs = new QTabWidget(controlCard);
    controlTabs->setObjectName("controlTabs");
    controlTabs->setTabPosition(QTabWidget::North);
    controlTabs->setDocumentMode(false);
    controlTabs->setUsesScrollButtons(false);
    controlTabs->setElideMode(Qt::ElideNone);
    controlTabs->tabBar()->setExpanding(true);
    controlTabs->tabBar()->setDrawBase(false);

    auto* stateTab = new QWidget(controlTabs);
    auto* stateForm = new QFormLayout(stateTab);
    stateForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    stateForm->setFormAlignment(Qt::AlignTop);
    stateForm->setContentsMargins(14, 18, 14, 14);
    stateForm->setSpacing(10);
    stateForm->setHorizontalSpacing(16);
    stateForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    stateForm->setRowWrapPolicy(QFormLayout::DontWrapRows);

    auto* xField = CreateSpinField(xEdit_, 7000000.0, "7000000", stateTab);
    auto* yField = CreateSpinField(yEdit_, 0.0, "0", stateTab);
    auto* zField = CreateSpinField(zEdit_, 0.0, "0", stateTab);
    auto* vxField = CreateSpinField(vxEdit_, 0.0, "0", stateTab);
    auto* vyField = CreateSpinField(vyEdit_, 7546.0, "7546", stateTab);
    auto* vzField = CreateSpinField(vzEdit_, 0.0, "0", stateTab);

    ApplyInputPalette(xEdit_);
    ApplyInputPalette(yEdit_);
    ApplyInputPalette(zEdit_);
    ApplyInputPalette(vxEdit_);
    ApplyInputPalette(vyEdit_);
    ApplyInputPalette(vzEdit_);

    stateForm->addRow("x<sub>0</sub>, м", xField);
    stateForm->addRow("y<sub>0</sub>, м", yField);
    stateForm->addRow("z<sub>0</sub>, м", zField);
    stateForm->addRow("V<sub>x0</sub>, м/с", vxField);
    stateForm->addRow("V<sub>y0</sub>, м/с", vyField);
    stateForm->addRow("V<sub>z0</sub>, м/с", vzField);

    auto* integrationTab = new QWidget(controlTabs);
    auto* integrationForm = new QFormLayout(integrationTab);
    integrationForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    integrationForm->setFormAlignment(Qt::AlignTop);
    integrationForm->setContentsMargins(14, 18, 14, 14);
    integrationForm->setSpacing(10);
    integrationForm->setHorizontalSpacing(16);
    integrationForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    integrationForm->setRowWrapPolicy(QFormLayout::DontWrapRows);

    methodComboBox_ = new MethodComboBox(integrationTab);
    methodComboBox_->setObjectName("methodComboBox");
    methodComboBox_->setMinimumHeight(40);
    for (const auto& option : integratorOptions_)
    {
        methodComboBox_->addItem(option.title);
    }

    auto* t0Field = CreateSpinField(t0Edit_, 0.0, "0", integrationTab);
    auto* tkField = CreateSpinField(tkEdit_, 6000.0, "6000", integrationTab);
    auto* hField = CreateSpinField(hEdit_, 10.0, "10", integrationTab);

    ApplyInputPalette(methodComboBox_);
    ApplyInputPalette(t0Edit_);
    ApplyInputPalette(tkEdit_);
    ApplyInputPalette(hEdit_);

    integrationForm->addRow("Метод", methodComboBox_);
    integrationForm->addRow("t<sub>0</sub>, c", t0Field);
    integrationForm->addRow("t<sub>k</sub>, c", tkField);
    integrationForm->addRow("h, c", hField);

    controlTabs->addTab(stateTab, "Начальные условия");
    controlTabs->addTab(integrationTab, "Интегрирование");

    runButton_ = new QPushButton("Выполнить моделирование", controlCard);
    runButton_->setObjectName("primaryButton");
    runButton_->setMinimumHeight(44);

    statusBadge_ = new QLabel(controlCard);
    statusBadge_->setObjectName("statusBadge");
    statusBadge_->setWordWrap(true);

    auto* summaryGroup = new QGroupBox("Итог расчёта", controlCard);
    auto* summaryForm = new QFormLayout(summaryGroup);
    summaryForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    summaryForm->setFormAlignment(Qt::AlignTop);
    summaryForm->setContentsMargins(14, 18, 14, 14);
    summaryForm->setSpacing(10);
    summaryForm->setHorizontalSpacing(16);
    summaryForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    summaryForm->setRowWrapPolicy(QFormLayout::DontWrapRows);

    samplesValue_ = new QLabel("—", summaryGroup);
    finalTimeValue_ = new QLabel("—", summaryGroup);
    radiusValue_ = new QLabel("—", summaryGroup);
    speedValue_ = new QLabel("—", summaryGroup);

    summaryForm->addRow("Точек траектории", samplesValue_);
    summaryForm->addRow("Финальное время", finalTimeValue_);
    summaryForm->addRow("Финальный радиус", radiusValue_);
    summaryForm->addRow("Финальная скорость", speedValue_);

    controlLayout->addWidget(titleLabel);
    controlLayout->addWidget(controlTabs);
    controlLayout->addWidget(runButton_);
    controlLayout->addWidget(statusBadge_);
    controlLayout->addWidget(summaryGroup);
    controlLayout->addStretch();

    auto* resultsCard = new QWidget(splitter);
    resultsCard->setObjectName("resultsCard");

    auto* resultsLayout = new QVBoxLayout(resultsCard);
    resultsLayout->setContentsMargins(18, 18, 18, 18);
    resultsLayout->setSpacing(12);

    auto* resultsTitle = new QLabel("Результаты моделирования", resultsCard);
    resultsTitle->setObjectName("resultsTitle");

    resultsTabs_ = new QTabWidget(resultsCard);
    resultsTabs_->setObjectName("resultsTabs");
    resultsTabs_->setMovable(true);
    resultsTabs_->tabBar()->setMovable(true);

    auto* coordinatesTab = new QWidget(resultsTabs_);
    auto* coordinatesLayout = new QGridLayout(coordinatesTab);
    coordinatesLayout->setContentsMargins(0, 0, 0, 0);
    coordinatesLayout->setHorizontalSpacing(12);
    coordinatesLayout->setVerticalSpacing(12);
    coordinatePlots_[0] = new PlotChartWidget("Координата X(t)", "t", "x", QColor("#1d4ed8"), coordinatesTab);
    coordinatePlots_[1] = new PlotChartWidget("Координата Y(t)", "t", "y", QColor("#059669"), coordinatesTab);
    coordinatePlots_[2] = new PlotChartWidget("Координата Z(t)", "t", "z", QColor("#dc2626"), coordinatesTab);
    coordinatesLayout->addWidget(coordinatePlots_[0], 0, 0);
    coordinatesLayout->addWidget(coordinatePlots_[1], 0, 1);
    coordinatesLayout->addWidget(coordinatePlots_[2], 1, 0, 1, 2);
    coordinatesLayout->setRowStretch(0, 6);
    coordinatesLayout->setRowStretch(1, 4);
    coordinatesLayout->setColumnStretch(0, 1);
    coordinatesLayout->setColumnStretch(1, 1);

    auto* velocitiesTab = new QWidget(resultsTabs_);
    auto* velocitiesLayout = new QGridLayout(velocitiesTab);
    velocitiesLayout->setContentsMargins(0, 0, 0, 0);
    velocitiesLayout->setHorizontalSpacing(12);
    velocitiesLayout->setVerticalSpacing(12);
    velocityPlots_[0] = new PlotChartWidget("Скорость Vx(t)", "t", "Vx", QColor("#7c3aed"), velocitiesTab);
    velocityPlots_[1] = new PlotChartWidget("Скорость Vy(t)", "t", "Vy", QColor("#d97706"), velocitiesTab);
    velocityPlots_[2] = new PlotChartWidget("Скорость Vz(t)", "t", "Vz", QColor("#0f766e"), velocitiesTab);
    velocitiesLayout->addWidget(velocityPlots_[0], 0, 0);
    velocitiesLayout->addWidget(velocityPlots_[1], 0, 1);
    velocitiesLayout->addWidget(velocityPlots_[2], 1, 0, 1, 2);
    velocitiesLayout->setRowStretch(0, 6);
    velocitiesLayout->setRowStretch(1, 4);
    velocitiesLayout->setColumnStretch(0, 1);
    velocitiesLayout->setColumnStretch(1, 1);

    auto* trajectoriesTab = new QWidget(resultsTabs_);
    auto* trajectoriesLayout = new QGridLayout(trajectoriesTab);
    trajectoriesLayout->setContentsMargins(0, 0, 0, 0);
    trajectoriesLayout->setHorizontalSpacing(12);
    trajectoriesLayout->setVerticalSpacing(12);
    trajectoryPlots_[0] = new PlotChartWidget("Траектория XY", "x", "y", QColor("#2563eb"), trajectoriesTab);
    trajectoryPlots_[1] = new PlotChartWidget("Траектория YZ", "y", "z", QColor("#db2777"), trajectoriesTab);
    trajectoryPlots_[2] = new PlotChartWidget("Траектория XZ", "x", "z", QColor("#65a30d"), trajectoriesTab);
    trajectoriesLayout->addWidget(trajectoryPlots_[0], 0, 0);
    trajectoriesLayout->addWidget(trajectoryPlots_[1], 0, 1);
    trajectoriesLayout->addWidget(trajectoryPlots_[2], 1, 0, 1, 2);
    trajectoriesLayout->setRowStretch(0, 6);
    trajectoriesLayout->setRowStretch(1, 4);
    trajectoriesLayout->setColumnStretch(0, 1);
    trajectoriesLayout->setColumnStretch(1, 1);

    auto* tableTab = new QWidget(resultsTabs_);
    auto* tableLayout = new QVBoxLayout(tableTab);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(10);

    auto* tableHeaderLayout = new QHBoxLayout();
    tableHeaderLayout->setContentsMargins(0, 0, 0, 0);
    tableHeaderLayout->setSpacing(10);

    auto* tableTitle = new QLabel("Таблица траектории", tableTab);
    tableTitle->setObjectName("tableTitle");

    exportTableButton_ = new QPushButton("Сохранить PNG", tableTab);
    exportTableButton_->setObjectName("secondaryButton");
    exportTableButton_->setMinimumHeight(40);
    exportTableButton_->setEnabled(false);

    tableHeaderLayout->addWidget(tableTitle);
    tableHeaderLayout->addStretch();
    tableHeaderLayout->addWidget(exportTableButton_);

    samplesTable_ = new QTableView(tableTab);
    samplesModel_ = new SimulationTableModel(samplesTable_);
    samplesTable_->setModel(samplesModel_);
    samplesTable_->setHorizontalHeader(new RichTextHeaderView(Qt::Horizontal, samplesTable_));
    samplesTable_->verticalHeader()->setVisible(false);
    samplesTable_->setAlternatingRowColors(true);
    samplesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    samplesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    samplesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    samplesTable_->setWordWrap(false);
    samplesTable_->horizontalHeader()->setMinimumSectionSize(90);
    samplesTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    samplesTable_->horizontalHeader()->setStretchLastSection(true);
    samplesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    samplesTable_->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    samplesTable_->setColumnWidth(0, 110);
    samplesTable_->setColumnWidth(1, 140);
    samplesTable_->setColumnWidth(2, 140);
    samplesTable_->setColumnWidth(3, 110);
    samplesTable_->setColumnWidth(4, 150);
    samplesTable_->setColumnWidth(5, 150);
    samplesTable_->setColumnWidth(6, 150);
    samplesTable_->setColumnWidth(7, 140);
    samplesTable_->setColumnWidth(8, 170);

    tableLayout->addLayout(tableHeaderLayout);
    tableLayout->addWidget(samplesTable_);

    resultsTabs_->addTab(coordinatesTab, "Координаты");
    resultsTabs_->addTab(velocitiesTab, "Скорости");
    resultsTabs_->addTab(trajectoriesTab, "Траектории");
    resultsTabs_->addTab(tableTab, "Таблица");

    resultsLayout->addWidget(resultsTitle);
    resultsLayout->addWidget(resultsTabs_, 1);

    splitter->addWidget(controlCard);
    splitter->addWidget(resultsCard);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({470, 1030});

    rootLayout->addWidget(splitter);
    setCentralWidget(central);
}

void MainWindow::ApplyTheme()
{
    setStyleSheet(R"(
        QMainWindow {
            background: #eef3f8;
        }

        QWidget#controlCard, QWidget#resultsCard {
            background: #fbfdff;
            border: 1px solid #d8e2ed;
            border-radius: 18px;
        }

        QSplitter::handle {
            background: #dbe6f2;
            margin: 20px 0;
            border-radius: 2px;
        }

        QSplitter::handle:hover {
            background: #c5d5e6;
        }

        QLabel {
            color: #102033;
            font-size: 15px;
        }

        QLabel#titleLabel {
            color: #0f172a;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#resultsTitle {
            color: #102033;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#tableTitle {
            color: #102033;
            font-size: 18px;
            font-weight: 700;
        }

        QFrame#plotCard {
            border: 1px solid #d9e2ec;
            border-radius: 16px;
            background: #ffffff;
        }

        QLabel#plotTitle {
            color: #102033;
            font-size: 16px;
            font-weight: 700;
        }

        QChartView#plotChartView {
            background: transparent;
            border: none;
        }

        QGroupBox {
            color: #102033;
            font-size: 14px;
            font-weight: 700;
            border: 1px solid #d9e2ec;
            border-radius: 14px;
            margin-top: 12px;
            background: #ffffff;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }

        QLineEdit, QComboBox, QDoubleSpinBox {
            font-size: 15px;
            min-height: 36px;
            border: 1px solid #c8d4e2;
            border-radius: 10px;
            padding: 0 10px;
            background: #ffffff;
            color: #102033;
            selection-background-color: #2a6df4;
            selection-color: #ffffff;
        }

        QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #2a6df4;
        }

        QPushButton#spinAdjustButton {
            background: #f7fbff;
            color: #1e3655;
            border: 1px solid #d7e2ee;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 700;
            padding: 0;
        }

        QPushButton#spinAdjustButton:hover {
            background: #edf5ff;
            border-color: #bdd0e6;
        }

        QPushButton#spinAdjustButton:pressed {
            background: #dfeaf8;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #d7e2ee;
            background: #f7fbff;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }

        QComboBox#methodComboBox {
            combobox-popup: 0;
            background: #ffffff;
            border: 1px solid #ccd9e7;
            border-radius: 10px;
            color: #1e3655;
            padding-right: 28px;
        }

        QComboBox#methodComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #d7e2ee;
            background: #f7fbff;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }

        QComboBox#methodComboBox::down-arrow {
            image: url(:/icons/arrow-down.svg);
            width: 12px;
            height: 12px;
        }

        QComboBox#methodComboBox[popupOpen="true"]::down-arrow {
            image: url(:/icons/arrow-up.svg);
        }

        QComboBox#methodComboBox QAbstractItemView {
            background: #ffffff;
            border: 1px solid #c8d8ea;
            outline: none;
            selection-background-color: #e6f1fb;
            selection-color: #173b5f;
            color: #1e3655;
            padding: 4px;
        }

        QComboBox#methodComboBox QAbstractItemView::item {
            min-height: 30px;
            padding: 4px 8px;
        }

        QPushButton#primaryButton {
            background: #1f5fd1;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 700;
            padding: 0 16px;
        }

        QPushButton#primaryButton:hover {
            background: #174ca9;
        }

        QPushButton#primaryButton:disabled {
            background: #c7d0db;
            color: #eef2f6;
            border: none;
        }

        QPushButton#secondaryButton {
            background: #ffffff;
            color: #1f5fd1;
            border: 1px solid #bfd0e6;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 700;
            padding: 0 14px;
        }

        QPushButton#secondaryButton:hover:enabled {
            background: #f3f8ff;
            border-color: #9fbbe2;
        }

        QPushButton#secondaryButton:disabled {
            color: #8aa0b8;
            border-color: #d4dfeb;
            background: #f7faff;
        }

        QLabel#statusBadge {
            border-radius: 12px;
            padding: 10px 12px;
            font-size: 13px;
            font-weight: 600;
            background: #e8f0fb;
            color: #1b3d69;
            border: 1px solid #bfd4ef;
        }

        QTabWidget::pane {
            border: 1px solid #d9e2ec;
            border-top: none;
            border-top-left-radius: 0px;
            border-top-right-radius: 0px;
            border-bottom-left-radius: 14px;
            border-bottom-right-radius: 14px;
            background: #ffffff;
            top: -1px;
        }

        QTabWidget#controlTabs::pane {
            margin-top: 0px;
        }

        QTabBar::tab {
            background: #edf2f7;
            color: #415266;
            border: 1px solid #d6dfe8;
            font-size: 15px;
            min-height: 22px;
            padding: 12px 16px;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            margin-right: 4px;
        }

        QTabBar::tab:selected {
            background: #ffffff;
            color: #102033;
            font-weight: 700;
            border-bottom-color: #ffffff;
            margin-bottom: -1px;
        }

        QTabWidget#controlTabs QTabBar::tab {
            min-width: 150px;
            min-height: 24px;
            padding: 12px 18px;
            border-bottom: none;
            margin-right: 8px;
            margin-bottom: -1px;
            border-top-left-radius: 14px;
            border-top-right-radius: 14px;
        }

        QTabWidget#controlTabs QTabBar::tab:selected {
            background: #ffffff;
            color: #102033;
            font-weight: 700;
            border-color: #d9e2ec;
            border-bottom-color: #ffffff;
        }

        QTableWidget, QTableView {
            border: 1px solid #d9e2ec;
            border-radius: 12px;
            gridline-color: #e6edf5;
            background: #ffffff;
            alternate-background-color: #f7faff;
            color: #102033;
            font-size: 15px;
        }

        QHeaderView::section {
            background: #eef4fb;
            color: #203146;
            border: none;
            border-right: 1px solid #d9e2ec;
            border-bottom: 1px solid #d9e2ec;
            padding: 8px;
            font-size: 14px;
            font-weight: 700;
        }

        QScrollBar:vertical {
            background: #eef3f8;
            width: 12px;
            margin: 0px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical {
            background: #bfd0e3;
            min-height: 40px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical:hover {
            background: #9fb8d1;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
            background: transparent;
            border: none;
        }

        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }

        QScrollBar:horizontal {
            background: #eef3f8;
            height: 12px;
            margin: 0px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal {
            background: #bfd0e3;
            min-width: 40px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal:hover {
            background: #9fb8d1;
        }

        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            background: transparent;
            border: none;
        }

        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");
}

void MainWindow::SetupSignals()
{
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::RunSimulation);
    connect(exportTableButton_, &QPushButton::clicked, this, &MainWindow::ExportTablePng);
    connect(resultsTabs_, &QTabWidget::currentChanged, this, [this](int) {
        if (hasResult_)
        {
            RefreshVisibleResults();
        }
    });

    const std::array<QDoubleSpinBox*, 9> fields = {
        xEdit_, yEdit_, zEdit_, vxEdit_, vyEdit_, vzEdit_, t0Edit_, tkEdit_, hEdit_};
    for (auto* field : fields)
    {
        connect(field, &QDoubleSpinBox::valueChanged, this, [this](double) { RefreshRunButtonState(); });
        if (auto* edit = field->findChild<QLineEdit*>())
        {
            connect(edit, &QLineEdit::textChanged, this, [this](const QString&) { RefreshRunButtonState(); });
        }
    }

    RefreshRunButtonState();
}

void MainWindow::ResetOutputs()
{
    samplesValue_->setText("—");
    finalTimeValue_->setText("—");
    radiusValue_->setText("—");
    speedValue_->setText("—");
    static_cast<SimulationTableModel*>(samplesModel_)->SetResult(nullptr);
    exportTableButton_->setEnabled(false);
    hasResult_ = false;
    lastResult_ = SimulationResult{};

    for (auto* plot : coordinatePlots_)
    {
        plot->Clear();
    }
    for (auto* plot : velocityPlots_)
    {
        plot->Clear();
    }
    for (auto* plot : trajectoryPlots_)
    {
        plot->Clear();
    }
}

void MainWindow::RunSimulation()
{
    ResetOutputs();

    QString errorMessage;
    if (!ValidateInput(errorMessage))
    {
        ShowStatus(errorMessage, false);
        return;
    }

    try
    {
        const double x = xEdit_->value();
        const double y = yEdit_->value();
        const double z = zEdit_->value();
        const double vx = vxEdit_->value();
        const double vy = vyEdit_->value();
        const double vz = vzEdit_->value();
        const double t0 = t0Edit_->value();
        const double tk = tkEdit_->value();
        const double h = hEdit_->value();

        const int methodIndex = methodComboBox_->currentIndex();
        if (methodIndex < 0 || methodIndex >= static_cast<int>(integratorOptions_.size()))
        {
            ShowStatus("Выбран некорректный метод интегрирования.", false);
            return;
        }

        auto integrator = integratorOptions_[static_cast<std::size_t>(methodIndex)].factory(t0, tk, h);
        integrator->SetRightParts(spaceCraftModel_);
        integrator->SetInitialState({x, y, z, vx, vy, vz});

        const SimulationResult result = integrator->MoveTo(tk);
        if (!result.success && result.samples.empty())
        {
            ShowStatus(QString::fromStdString(result.message), false);
            return;
        }

        lastResult_ = result;
        hasResult_ = true;
        PopulateTable(result);
        UpdateSummary(result);
        RefreshVisibleResults();
        QTimer::singleShot(0, this, [this]() {
            if (hasResult_)
            {
                RefreshVisibleResults();
            }
        });
        exportTableButton_->setEnabled(!result.samples.empty());
        if (result.success)
        {
            ShowStatus("Моделирование завершено успешно. Данные готовы для построения графиков.", true);
        }
        else if (IsRecoverableResult(result))
        {
            ShowStatus(QString::fromStdString(result.message), false);
        }
        else
        {
            ShowStatus(QString::fromStdString(result.message), false);
        }
    }
    catch (const std::exception& ex)
    {
        ShowStatus(QString("Ошибка: %1").arg(QString::fromUtf8(ex.what())), false);
    }
}

bool MainWindow::ValidateInput(QString& errorMessage) const
{
    const std::array<std::pair<QString, const QDoubleSpinBox*>, 9> fields = {{
        {"x0", xEdit_},
        {"y0", yEdit_},
        {"z0", zEdit_},
        {"Vx0", vxEdit_},
        {"Vy0", vyEdit_},
        {"Vz0", vzEdit_},
        {"t0", t0Edit_},
        {"tk", tkEdit_},
        {"h", hEdit_},
    }};

    for (const auto& field : fields)
    {
        if (!HasSpinBoxInput(field.second))
        {
            errorMessage = QString("Поле %1 не должно быть пустым.").arg(field.first);
            return false;
        }

        const double value = field.second->value();
        if (!std::isfinite(value))
        {
            errorMessage = QString("Поле %1 должно содержать корректное число.").arg(field.first);
            return false;
        }
    }

    const double t0 = t0Edit_->value();
    const double tk = tkEdit_->value();
    const double h = hEdit_->value();
    const double x = xEdit_->value();
    const double y = yEdit_->value();
    const double z = zEdit_->value();

    if (tk <= t0)
    {
        errorMessage = "Должно выполняться tk > t0.";
        return false;
    }

    if (h <= 0.0)
    {
        errorMessage = "Шаг интегрирования h должен быть больше нуля.";
        return false;
    }

    if (((tk - t0) / h) > 500000.0)
    {
        errorMessage = "Слишком много шагов интегрирования. Увеличь h или уменьши tk.";
        return false;
    }

    if (std::abs(x) + std::abs(y) + std::abs(z) < 1.0)
    {
        errorMessage = "Начальный радиус-вектор не должен быть нулевым.";
        return false;
    }

    return true;
}

void MainWindow::RefreshRunButtonState()
{
    const std::array<QDoubleSpinBox*, 9> fields = {
        xEdit_, yEdit_, zEdit_, vxEdit_, vyEdit_, vzEdit_, t0Edit_, tkEdit_, hEdit_};

    const bool allFilled = std::all_of(fields.begin(), fields.end(), [](const QDoubleSpinBox* field) {
        return HasSpinBoxInput(field);
    });

    if (runButton_ != nullptr)
    {
        runButton_->setEnabled(allFilled);
    }
}

void MainWindow::PopulateTable(const SimulationResult& result)
{
    samplesTable_->setUpdatesEnabled(false);
    Q_UNUSED(result);
    static_cast<SimulationTableModel*>(samplesModel_)->SetResult(&lastResult_);
    samplesTable_->setUpdatesEnabled(true);
}

void MainWindow::ExportTablePng()
{
    if (lastResult_.samples.empty())
    {
        ShowStatus("Таблица пока пуста. Сначала выполните моделирование.", false);
        return;
    }

    QWidget exportCard;
    exportCard.setObjectName("resultsCard");
    exportCard.setStyleSheet(styleSheet());
    exportCard.setAttribute(Qt::WA_DontShowOnScreen, true);

    auto* exportLayout = new QVBoxLayout(&exportCard);
    exportLayout->setContentsMargins(18, 18, 18, 18);
    exportLayout->setSpacing(10);

    auto* exportTitle = new QLabel("Таблица траектории", &exportCard);
    exportTitle->setObjectName("tableTitle");
    exportLayout->addWidget(exportTitle);

    auto* exportTable = new QTableView(&exportCard);
    auto* exportModel = new SimulationTableModel(exportTable);
    exportModel->SetResult(&lastResult_);
    exportTable->setModel(exportModel);
    exportTable->setHorizontalHeader(new RichTextHeaderView(Qt::Horizontal, exportTable));
    exportTable->setSelectionBehavior(samplesTable_->selectionBehavior());
    exportTable->setSelectionMode(samplesTable_->selectionMode());
    exportTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    exportTable->setAlternatingRowColors(samplesTable_->alternatingRowColors());
    exportTable->setWordWrap(samplesTable_->wordWrap());
    exportTable->setTextElideMode(samplesTable_->textElideMode());
    exportTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    exportTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    exportTable->verticalHeader()->setVisible(samplesTable_->verticalHeader()->isVisible());
    exportTable->horizontalHeader()->setStretchLastSection(samplesTable_->horizontalHeader()->stretchLastSection());
    exportTable->horizontalHeader()->setMinimumSectionSize(samplesTable_->horizontalHeader()->minimumSectionSize());
    exportTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    for (int column = 0; column < exportModel->columnCount(); ++column)
    {
        exportTable->setColumnWidth(column, samplesTable_->columnWidth(column));
        exportTable->horizontalHeader()->setSectionResizeMode(
            column, samplesTable_->horizontalHeader()->sectionResizeMode(column));
    }

    const int exportWidth = std::max(FullTableWidth(exportTable), 900);
    const int exportHeight = std::max(FullTableHeight(exportTable), 280);
    exportTable->setMinimumSize(exportWidth, exportHeight);
    exportTable->setMaximumSize(exportWidth, exportHeight);
    exportLayout->addWidget(exportTable);

    exportCard.ensurePolished();
    exportLayout->activate();

    const int finalWidth = exportWidth + 36;
    const int finalHeight = exportHeight + std::max(exportTitle->sizeHint().height(), 32) + 46;
    exportCard.resize(finalWidth, finalHeight);
    exportLayout->setGeometry(QRect(0, 0, finalWidth, finalHeight));
    exportLayout->activate();

    QImage image(QSize(finalWidth, finalHeight), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    exportCard.render(&painter);
    painter.end();

    const QString filePath = QDir(ProjectTablesDirectoryPath()).absoluteFilePath(BuildTableExportFileName());
    if (!image.save(filePath, "PNG"))
    {
        ShowStatus("Не удалось сохранить PNG таблицы.", false);
        return;
    }

    ShowStatus(QString("PNG таблицы сохранён: %1").arg(filePath), true);
}

void MainWindow::UpdateSummary(const SimulationResult& result)
{
    if (result.samples.empty())
    {
        return;
    }

    const auto& lastSample = result.samples.back();

    samplesValue_->setText(QString::number(static_cast<qulonglong>(result.samples.size())));
    finalTimeValue_->setText(QString("%1 c").arg(FormatNumber(lastSample.time)));
    radiusValue_->setText(QString("%1 м").arg(FormatNumber(Radius(lastSample.state))));
    speedValue_->setText(QString("%1 м/с").arg(FormatNumber(Speed(lastSample.state))));
}

void MainWindow::UpdatePlots(const SimulationResult& result)
{
    if (result.samples.empty())
    {
        return;
    }

    QVector<QPointF> xSeries;
    QVector<QPointF> ySeries;
    QVector<QPointF> zSeries;
    QVector<QPointF> vxSeries;
    QVector<QPointF> vySeries;
    QVector<QPointF> vzSeries;
    QVector<QPointF> xySeries;
    QVector<QPointF> yzSeries;
    QVector<QPointF> xzSeries;

    const int count = static_cast<int>(result.samples.size());
    xSeries.reserve(count);
    ySeries.reserve(count);
    zSeries.reserve(count);
    vxSeries.reserve(count);
    vySeries.reserve(count);
    vzSeries.reserve(count);
    xySeries.reserve(count);
    yzSeries.reserve(count);
    xzSeries.reserve(count);

    for (const auto& sample : result.samples)
    {
        xSeries.append(QPointF(sample.time, sample.state[0]));
        ySeries.append(QPointF(sample.time, sample.state[1]));
        zSeries.append(QPointF(sample.time, sample.state[2]));
        vxSeries.append(QPointF(sample.time, sample.state[3]));
        vySeries.append(QPointF(sample.time, sample.state[4]));
        vzSeries.append(QPointF(sample.time, sample.state[5]));
        xySeries.append(QPointF(sample.state[0], sample.state[1]));
        yzSeries.append(QPointF(sample.state[1], sample.state[2]));
        xzSeries.append(QPointF(sample.state[0], sample.state[2]));
    }

    const bool hasCollisionStop = (!result.success && !result.samples.empty() &&
                                   QString::fromStdString(result.message).contains("столкновение", Qt::CaseInsensitive));
    const double stopTime = result.samples.back().time;
    const QString stopLabel = QString::fromUtf8("|r| = Rземли");

    const int tabIndex = resultsTabs_ != nullptr ? resultsTabs_->currentIndex() : 0;
    if (tabIndex == 0)
    {
        coordinatePlots_[0]->SetData(xSeries);
        coordinatePlots_[1]->SetData(ySeries);
        coordinatePlots_[2]->SetData(zSeries);
        for (auto* plot : coordinatePlots_)
        {
            if (hasCollisionStop)
            {
                plot->SetEventMarker(stopTime, stopLabel);
            }
            else
            {
                plot->ClearEventMarker();
            }
        }
    }
    else if (tabIndex == 1)
    {
        velocityPlots_[0]->SetData(vxSeries);
        velocityPlots_[1]->SetData(vySeries);
        velocityPlots_[2]->SetData(vzSeries);
        for (auto* plot : velocityPlots_)
        {
            plot->ClearEventMarker();
        }
    }
    else if (tabIndex == 2)
    {
        trajectoryPlots_[0]->SetData(xySeries);
        trajectoryPlots_[1]->SetData(yzSeries);
        trajectoryPlots_[2]->SetData(xzSeries);
        for (auto* plot : trajectoryPlots_)
        {
            plot->ClearEventMarker();
        }
    }
}

void MainWindow::RefreshVisibleResults()
{
    if (!hasResult_)
    {
        return;
    }

    UpdatePlots(lastResult_);
}

void MainWindow::ShowStatus(const QString& text, bool success)
{
    statusBadge_->setText(text);
    if (success)
    {
        statusBadge_->setStyleSheet(
            "border: 1px solid #94d7aa; background: #eaf8ef; color: #1d5a32; border-radius: 12px; padding: 10px 12px;");
    }
    else
    {
        statusBadge_->setStyleSheet(
            "border: 1px solid #efb7bf; background: #fff1f3; color: #7b2636; border-radius: 12px; padding: 10px 12px;");
    }
}

QString MainWindow::FormatNumber(double value)
{
    return FormatNumericValue(value);
}

bool MainWindow::IsRecoverableResult(const SimulationResult& result)
{
    return !result.success && !result.samples.empty();
}

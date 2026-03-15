#pragma once

#include "PlotChartWidget.h"
#include "SimulationTypes.h"
#include "TAbstractIntegrator.h"
#include "TSpaceCraft.h"

#include <QMainWindow>
#include <QString>

#include <array>
#include <functional>
#include <memory>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTableView;
class QAbstractTableModel;

struct IntegratorOption
{
    QString title;
    std::function<std::unique_ptr<TAbstractIntegrator>(double t0, double tk, double h)> factory;
};

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(std::vector<IntegratorOption> integratorOptions, QWidget* parent = nullptr);

private:
    void BuildUi();
    void ApplyTheme();
    void SetupSignals();
    void ResetOutputs();
    void RunSimulation();
    void ExportTablePng();
    bool ValidateInput(QString& errorMessage) const;
    void RefreshRunButtonState();
    void PopulateTable(const SimulationResult& result);
    void UpdateSummary(const SimulationResult& result);
    void UpdatePlots(const SimulationResult& result);
    void RefreshVisibleResults();
    void ShowStatus(const QString& text, bool success);
    static bool IsRecoverableResult(const SimulationResult& result);
    static QString FormatNumber(double value);

    std::vector<IntegratorOption> integratorOptions_;
    TSpaceCraft spaceCraftModel_;

    QComboBox* methodComboBox_ = nullptr;

    QDoubleSpinBox* xEdit_ = nullptr;
    QDoubleSpinBox* yEdit_ = nullptr;
    QDoubleSpinBox* zEdit_ = nullptr;
    QDoubleSpinBox* vxEdit_ = nullptr;
    QDoubleSpinBox* vyEdit_ = nullptr;
    QDoubleSpinBox* vzEdit_ = nullptr;
    QDoubleSpinBox* t0Edit_ = nullptr;
    QDoubleSpinBox* tkEdit_ = nullptr;
    QDoubleSpinBox* hEdit_ = nullptr;

    QPushButton* runButton_ = nullptr;
    QPushButton* exportTableButton_ = nullptr;
    QLabel* statusBadge_ = nullptr;

    QLabel* samplesValue_ = nullptr;
    QLabel* finalTimeValue_ = nullptr;
    QLabel* radiusValue_ = nullptr;
    QLabel* speedValue_ = nullptr;

    std::array<PlotChartWidget*, 3> coordinatePlots_{};
    std::array<PlotChartWidget*, 3> velocityPlots_{};
    std::array<PlotChartWidget*, 3> trajectoryPlots_{};

    QTabWidget* resultsTabs_ = nullptr;
    QTableView* samplesTable_ = nullptr;
    QAbstractTableModel* samplesModel_ = nullptr;

    SimulationResult lastResult_;
    bool hasResult_ = false;
};

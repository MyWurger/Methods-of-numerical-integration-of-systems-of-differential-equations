#pragma once

#include "SimulationTypes.h"
#include "TDynamicModel.h"

class TAbstractIntegrator
{
public:
    TAbstractIntegrator(double t0, double tk, double h);
    virtual ~TAbstractIntegrator() = default;

    void SetRightParts(const TDynamicModel& model);
    void SetInitialState(const TVector& state);

    double GetT0() const noexcept;
    double GetTk() const noexcept;
    double GetH() const noexcept;

    SimulationResult MoveTo(double tend);

protected:
    virtual TVector OneStep(const TVector& state) = 0;

    const TDynamicModel& Model() const;
    static void EnsureFiniteState(const TVector& state);
    double GetCurrentTime() const noexcept;
    const TVector& GetCurrentState() const noexcept;
    double GetCurrentH() const noexcept;
    void AdvanceTime(double step) noexcept;

private:
    double t0_;
    double tk_;
    double h_;
    double currentTime_;
    TVector currentState_;
    const TDynamicModel* rightParts_ = nullptr;
};

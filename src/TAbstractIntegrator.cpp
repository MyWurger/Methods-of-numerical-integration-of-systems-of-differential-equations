#include "TAbstractIntegrator.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace
{
constexpr std::size_t kSafetyStepLimit = 2'000'000;
constexpr double kTimeTolerance = 1e-12;
}

TAbstractIntegrator::TAbstractIntegrator(double t0, double tk, double h)
    : t0_(t0), tk_(tk), h_(h), currentTime_(t0)
{
}

void TAbstractIntegrator::SetRightParts(const TDynamicModel& model)
{
    rightParts_ = &model;
}

void TAbstractIntegrator::SetInitialState(const TVector& state)
{
    EnsureFiniteState(state);
    currentState_ = state;
    currentTime_ = t0_;
}

double TAbstractIntegrator::GetT0() const noexcept
{
    return t0_;
}

double TAbstractIntegrator::GetTk() const noexcept
{
    return tk_;
}

double TAbstractIntegrator::GetH() const noexcept
{
    return h_;
}

SimulationResult TAbstractIntegrator::MoveTo(double tend)
{
    try
    {
        if (!rightParts_)
        {
            return {false, "Правая часть системы не задана", {}};
        }

        if (currentState_.Empty())
        {
            return {false, "Начальное состояние не должно быть пустым", {}};
        }

        if (!std::isfinite(t0_) || !std::isfinite(tk_) || !std::isfinite(h_))
        {
            return {false, "Параметры интегрирования должны быть конечными числами", {}};
        }

        if (h_ <= 0.0)
        {
            return {false, "Шаг интегрирования h должен быть больше нуля", {}};
        }

        if (tk_ < t0_)
        {
            return {false, "Конечный момент времени tk должен быть не меньше t0", {}};
        }

        if (!std::isfinite(tend))
        {
            return {false, "Момент времени tend должен быть конечным числом", {}};
        }

        if (tend < t0_)
        {
            return {false, "Момент времени tend должен быть не меньше t0", {}};
        }

        const double effectiveEndTime = std::min(tend, tk_);
        EnsureFiniteState(currentState_);

        SimulationResult result;
        result.success = true;
        result.message = "Моделирование завершено";

        const double span = effectiveEndTime - currentTime_;
        const auto estimatedSteps =
            static_cast<std::size_t>(std::ceil(span / h_)) + static_cast<std::size_t>(1);
        result.samples.reserve(std::min<std::size_t>(estimatedSteps, 100000));

        result.samples.push_back({currentTime_, currentState_});

        std::string terminalMessage;
        if (Model().IsTerminalState(currentState_, terminalMessage))
        {
            result.success = false;
            result.message = terminalMessage;
            return result;
        }

        std::size_t steps = 0;
        while ((effectiveEndTime - currentTime_) > kTimeTolerance)
        {
            if (steps >= kSafetyStepLimit)
            {
                return {false, "Превышен допустимый предел шагов интегрирования", result.samples};
            }

            const double integrationStep = std::min(h_, effectiveEndTime - currentTime_);
            const double previousStep = h_;
            h_ = integrationStep;
            currentState_ = OneStep(currentState_);
            h_ = previousStep;
            EnsureFiniteState(currentState_);
            AdvanceTime(integrationStep);
            result.samples.push_back({currentTime_, currentState_});
            if (Model().IsTerminalState(currentState_, terminalMessage))
            {
                result.success = false;
                result.message = terminalMessage;
                return result;
            }
            ++steps;
        }

        return result;
    }
    catch (const std::exception& ex)
    {
        return {false, ex.what(), {}};
    }
}

const TDynamicModel& TAbstractIntegrator::Model() const
{
    return *rightParts_;
}

double TAbstractIntegrator::GetCurrentTime() const noexcept
{
    return currentTime_;
}

const TVector& TAbstractIntegrator::GetCurrentState() const noexcept
{
    return currentState_;
}

double TAbstractIntegrator::GetCurrentH() const noexcept
{
    return h_;
}

void TAbstractIntegrator::AdvanceTime(double step) noexcept
{
    currentTime_ += step;
}

void TAbstractIntegrator::EnsureFiniteState(const TVector& state)
{
    for (double value : state.Data())
    {
        if (!std::isfinite(value))
        {
            throw std::runtime_error("В состоянии системы обнаружено нечисловое значение");
        }
    }
}

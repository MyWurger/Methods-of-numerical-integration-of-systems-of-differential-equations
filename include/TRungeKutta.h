#pragma once

#include "TAbstractIntegrator.h"

class TRungeKutta final : public TAbstractIntegrator
{
public:
    TRungeKutta(double t0, double tk, double h);

protected:
    TVector OneStep(const TVector& state) override;
};

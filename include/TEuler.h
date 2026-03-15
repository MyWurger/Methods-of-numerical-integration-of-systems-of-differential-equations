#pragma once

#include "TAbstractIntegrator.h"

class TEuler final : public TAbstractIntegrator
{
public:
    TEuler(double t0, double tk, double h);

protected:
    TVector OneStep(const TVector& state) override;
};

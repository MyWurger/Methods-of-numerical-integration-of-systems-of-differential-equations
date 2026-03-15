#include "TRungeKutta.h"

TRungeKutta::TRungeKutta(double t0, double tk, double h) : TAbstractIntegrator(t0, tk, h)
{
}

TVector TRungeKutta::OneStep(const TVector& state)
{
    const double time = GetCurrentTime();
    const double step = GetCurrentH();
    const TVector k1 = Model().Funcs(time, state);
    const TVector k2 = Model().Funcs(time + step * 0.5, state + (step * 0.5) * k1);
    const TVector k3 = Model().Funcs(time + step * 0.5, state + (step * 0.5) * k2);
    const TVector k4 = Model().Funcs(time + step, state + step * k3);

    return state + (step / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
}

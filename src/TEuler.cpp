#include "TEuler.h"

TEuler::TEuler(double t0, double tk, double h) : TAbstractIntegrator(t0, tk, h)
{
}

TVector TEuler::OneStep(const TVector& state)
{
    return state + GetCurrentH() * Model().Funcs(GetCurrentTime(), state);
}

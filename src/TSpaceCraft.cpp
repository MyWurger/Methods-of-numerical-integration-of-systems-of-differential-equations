#include "TSpaceCraft.h"

#include <cmath>
#include <stdexcept>

TSpaceCraft::TSpaceCraft(double mu) : mu_(mu)
{
}

TVector TSpaceCraft::Funcs(double /*time*/, const TVector& state) const
{
    if (state.Size() != 6)
    {
        throw std::invalid_argument("Состояние КА должно содержать 6 компонент");
    }

    const double x = state[0];
    const double y = state[1];
    const double z = state[2];
    const double vx = state[3];
    const double vy = state[4];
    const double vz = state[5];

    const double radiusSquared = x * x + y * y + z * z;
    if (radiusSquared <= 0.0)
    {
        throw std::runtime_error("Радиус-вектор КА не должен быть нулевым");
    }

    const double radius = std::sqrt(radiusSquared);
    const double factor = -mu_ / (radiusSquared * radius);

    return {vx, vy, vz, factor * x, factor * y, factor * z};
}

bool TSpaceCraft::IsTerminalState(const TVector& state, std::string& message) const
{
    if (state.Size() != 6)
    {
        throw std::invalid_argument("Состояние КА должно содержать 6 компонент");
    }

    const double x = state[0];
    const double y = state[1];
    const double z = state[2];
    const double radius = std::sqrt(x * x + y * y + z * z);
    if (radius <= earthRadius_)
    {
        message = "Моделирование остановлено: столкновение с Землёй";
        return true;
    }

    return false;
}

#pragma once

#include "TDynamicModel.h"

class TSpaceCraft final : public TDynamicModel
{
public:
    // explicit запрещает неявное создание объекта TSpaceCraft из одного
    // числа mu.
    //
    // Иначе выражение вроде:
    //     TSpaceCraft model = 3.98603e14;
    // было бы допустимо как неявное преобразование.
    //
    // Здесь мы требуем явного создания объекта модели.
    explicit TSpaceCraft(double mu = 3.98603e14);

    TVector Funcs(double time, const TVector& state) const override;
    bool IsTerminalState(const TVector& state, std::string& message) const override;

private:
    double mu_;
    double earthRadius_ = 6.371e6;
};

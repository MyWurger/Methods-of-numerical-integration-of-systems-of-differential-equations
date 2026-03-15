#pragma once

#include "TDynamicModel.h"

class TSpaceCraft final : public TDynamicModel
{
public:
    explicit TSpaceCraft(double mu = 3.98603e14);

    TVector Funcs(double time, const TVector& state) const override;
    bool IsTerminalState(const TVector& state, std::string& message) const override;

private:
    double mu_;
    double earthRadius_ = 6.371e6;
};

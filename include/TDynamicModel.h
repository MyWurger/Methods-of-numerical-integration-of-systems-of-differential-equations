#pragma once

#include "TVector.h"

#include <string>

class TDynamicModel
{
public:
    virtual ~TDynamicModel() = default;
    virtual TVector Funcs(double time, const TVector& state) const = 0;
    virtual bool IsTerminalState(const TVector& state, std::string& message) const;
};

#pragma once

#include "TVector.h"

#include <string>
#include <vector>

struct SimulationSample
{
    double time = 0.0;
    TVector state;
};

struct SimulationResult
{
    bool success = false;
    std::string message;
    std::vector<SimulationSample> samples;
};


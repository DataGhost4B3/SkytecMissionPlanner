#pragma once

#include <unordered_map>
#include <map>
#include <array>
#include <vector>
#include <string>
#include <utility>
#include "points.hpp"

void run_physics(
    std::array<std::vector<geo_point>, 360>& path_list,
    const std::unordered_map<std::string, double>& params,
    // const std::unordered_map<double, std::pair<double, double>>& wind
    const std::map<double, std::pair<double, double>>& wind
    );
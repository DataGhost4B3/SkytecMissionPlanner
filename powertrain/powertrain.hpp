#pragma once

#include "points.hpp"
#include "fetchwind.hpp"
#include <array>
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

extern std::array<std::vector<geo_point>, 360> path_lists;

extern std::unordered_map<std::string, double> params;
/*
 * LZ LAT
 * LZ LON
 * LZ ELEV
 * JUMP ALT
 * OPEN ALT
 * WT KG
 * AC VEL KTS
 * AC HDG DEG
 * GR
 * 0-WIND CANOPY VEL
 */

// extern std::unordered_map<double, std::pair<double, double>> wind;
extern std::map<double, std::pair<double, double>> wind;
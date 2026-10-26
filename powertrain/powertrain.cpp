#include "powertrain.hpp"

// Instantiate the variables in memory exactly once
std::array<std::vector<geo_point>, 360> path_lists;
std::unordered_map<std::string, double> params;
// std::unordered_map<double, std::pair<double, double>> wind;
std::map<double, std::pair<double, double>> wind;
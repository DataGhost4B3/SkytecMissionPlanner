#include "points.hpp"
#include <cmath>
#include <numbers>

inline constexpr double R0 = 6356766; // meters; 45 32 33 latitude
inline constexpr double PI = std::numbers::pi;
inline constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / std::numbers::pi;

// geo_point -> cartesian_point definition
geo_point::operator cartesian_point() const {
    cartesian_point p;
    const double rad_lat = lat * DEG_TO_RAD;
    const double rad_lon = lon * DEG_TO_RAD;
    const double r = R0 + alt;

    p.x = r * std::cos(rad_lat) * std::cos(rad_lon);
    p.y = r * std::cos(rad_lat) * std::sin(rad_lon);
    p.z = r * std::sin(rad_lat);
    return p;
}

// cartesian_point -> geo_point definition
cartesian_point::operator geo_point() const {
    geo_point gp;
    double r = std::sqrt(x * x + y * y);
    gp.lon = std::atan2(y, x) * RAD_TO_DEG;
    gp.lat = std::atan2(z, r) * RAD_TO_DEG;
    gp.alt = r - R0;
    return gp;
}

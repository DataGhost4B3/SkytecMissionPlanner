#include "engine.hpp"
#include <cmath>
#include <numbers>
#include <algorithm>

constexpr double EARTH_RADIUS = 6378137.0;
constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;
constexpr double RAD_TO_DEG = 180.0 / std::numbers::pi;
constexpr double FT_TO_M = 0.3048;
constexpr double M_TO_FT = 1.0 / 0.3048;
constexpr double KTS_TO_MS = 0.514444;

// Altitude-based wind interpolation (Altitude in Feet, Speed in m/s)
static std::pair<double, double> interpolate_alt(const std::map<double, std::pair<double, double>>& wind, double alt_ft, double lz_alt_ft) {
    if (alt_ft <= lz_alt_ft || wind.empty()) return {0.0, 0.0};

    auto upper_it = wind.upper_bound(alt_ft);
    if (upper_it == wind.end()) return std::prev(wind.end())->second;

    double alt1, S1, D1;
    if (upper_it == wind.begin()) {
        alt1 = lz_alt_ft; S1 = 0.0; D1 = 0.0;
    } else {
        auto lower_it = std::prev(upper_it);
        alt1 = lower_it->first; S1 = lower_it->second.first; D1 = lower_it->second.second;
    }

    double alt2 = upper_it->first; double S2 = upper_it->second.first; double D2 = upper_it->second.second;

    double U1 = -S1 * std::sin(D1 * DEG_TO_RAD); double V1 = -S1 * std::cos(D1 * DEG_TO_RAD);
    double U2 = -S2 * std::sin(D2 * DEG_TO_RAD); double V2 = -S2 * std::cos(D2 * DEG_TO_RAD);

    double t = (alt_ft - alt1) / (alt2 - alt1);
    double U = U1 + t * (U2 - U1); double V = V1 + t * (V2 - V1);

    double S = std::sqrt(U * U + V * V);
    double D = std::atan2(-U, -V) * RAD_TO_DEG;
    D = std::fmod(D, 360.0);
    if (D < 0.0) D += 360.0;

    return {S, D};
}

// Temporary struct for the relative Cartesian shifting
struct RelPoint { double x, y, alt; };

void run_physics(
    std::array<std::vector<geo_point>, 360>& path_list,
    const std::unordered_map<std::string, double>& params,
    const std::map<double, std::pair<double, double>>& wind
) {
    double LZ_LAT = params.at("LZ LAT");
    double LZ_LON = params.at("LZ LON");
    double lz_alt_m = params.at("LZ ELEV"); // Assuming UI provides meters

    double jump_alt_m = params.at("JUMP ALT") * FT_TO_M;
    double open_alt_m = params.at("OPEN ALT") * FT_TO_M;

    double airv_ms = params.at("AC VEL KTS") * KTS_TO_MS;
    double airh_deg = params.at("AC HDG DEG");

    double canopy_v_ms = params.at("0-WIND CANOPY VEL") * KTS_TO_MS;
    double gr = params.at("GR");
    if (gr <= 0) gr = 1.0;
    double descv_ms = canopy_v_ms / gr;

    double vz_ff_ms = 45.0; // Freefall vertical velocity (m/s)
    double dz = 5.0;        // 5-meter integration steps

    // Master Cartesian Coordinate definition: +X is East, +Y is North
    double ac_vx = airv_ms * std::sin(airh_deg * DEG_TO_RAD);
    double ac_vy = airv_ms * std::cos(airh_deg * DEG_TO_RAD);

    // --- PHASE 1: Freefall Trajectory (Jump -> Open) ---
    // Start directly over the LZ (0,0)
    std::vector<RelPoint> freefall_path;
    double cx = 0.0, cy = 0.0;
    freefall_path.push_back({cx, cy, jump_alt_m});

    for (double alt = jump_alt_m; alt > open_alt_m; alt -= dz) {
        auto [w_ms, w_deg] = interpolate_alt(wind, alt * M_TO_FT, lz_alt_m * M_TO_FT);

        // Meteorological wind points 'to' opposite direction
        double wind_vx = -w_ms * std::sin(w_deg * DEG_TO_RAD);
        double wind_vy = -w_ms * std::cos(w_deg * DEG_TO_RAD);

        cx += (wind_vx + ac_vx) * (dz / vz_ff_ms);
        cy += (wind_vy + ac_vy) * (dz / vz_ff_ms);

        ac_vx *= 0.95; ac_vy *= 0.95; // Decay aircraft momentum

        double next_alt = std::max(alt - dz, open_alt_m);
        freefall_path.push_back({cx, cy, next_alt});
    }

    double ff_end_x = cx;
    double ff_end_y = cy;

    // --- PHASE 2: Canopy Trajectory (Open -> LZ) & Parallel Shift ---
    for (int i = 0; i < 360; ++i) {
        path_list[i].clear();

        std::vector<RelPoint> canopy_path;
        double px = ff_end_x;
        double py = ff_end_y;
        double h_rad = i * DEG_TO_RAD;

        // Calculate the forward canopy path for this specific heading
        for (double alt = open_alt_m; alt > lz_alt_m; alt -= dz) {
            auto [w_ms, w_deg] = interpolate_alt(wind, alt * M_TO_FT, lz_alt_m * M_TO_FT);

            double wind_vx = -w_ms * std::sin(w_deg * DEG_TO_RAD);
            double wind_vy = -w_ms * std::cos(w_deg * DEG_TO_RAD);

            double glide_vx = canopy_v_ms * std::sin(h_rad);
            double glide_vy = canopy_v_ms * std::cos(h_rad);

            px += (wind_vx + glide_vx) * (dz / descv_ms);
            py += (wind_vy + glide_vy) * (dz / descv_ms);

            double next_alt = std::max(alt - dz, lz_alt_m);
            canopy_path.push_back({px, py, next_alt});
        }

        // --- PHASE 3: The Parallel Shift ---
        // (px, py) is where we landed. We want the landing to be exactly at the LZ (0,0).
        // Therefore, the whole trajectory needs to be shifted by (-px, -py).
        double shift_x = -px;
        double shift_y = -py;

        // Map the shifted Cartesian coords to true Lat/Lon
        // A. Shift and map Freefall leg
        for (const auto& pt : freefall_path) {
            double sx = pt.x + shift_x;
            double sy = pt.y + shift_y;

            double lat = LZ_LAT + (sy / EARTH_RADIUS) * RAD_TO_DEG;
            double lon = LZ_LON + (sx / (EARTH_RADIUS * std::cos(lat * DEG_TO_RAD))) * RAD_TO_DEG;
            path_list[i].push_back(geo_point{lat, lon, pt.alt});
        }

        // B. Shift and map Canopy leg
        for (const auto& pt : canopy_path) {
            double sx = pt.x + shift_x;
            double sy = pt.y + shift_y;

            double lat = LZ_LAT + (sy / EARTH_RADIUS) * RAD_TO_DEG;
            double lon = LZ_LON + (sx / (EARTH_RADIUS * std::cos(lat * DEG_TO_RAD))) * RAD_TO_DEG;
            path_list[i].push_back(geo_point{lat, lon, pt.alt});
        }
    }
}
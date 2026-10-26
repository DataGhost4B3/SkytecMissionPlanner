#pragma once

#include <unordered_map>
#include <map> // Replace <unordered_map>
#include <utility>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>

// using WindMap = std::unordered_map<double, std::pair<double, double>>;
using WindMap = std::map<double, std::pair<double, double>>;
using json = nlohmann::json;

constexpr int pressure_levels[] = {
    1000, 975, 950, 925, 900, 850, 800, 700, 600, 500, 400, 300, 250, 200
};

size_t writeCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userdata
    );

std::string buildHourlyVariables();

std::string buildURL(
            double latitude,
            double longitude
        );

std::string httpGet(const std::string& url);

WindMap fetchWind(
        double latitude,
        double longitude,
        const std::string& date,
        const std::string& time
    );
#include "fetchwind.hpp"

size_t writeCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userdata
    ) {
    const size_t bytes = size * nmemb;
    auto* response = static_cast<std::string*>(userdata);
    response->append(
        static_cast<char*>(contents),
        bytes
        );
    return bytes;
}

std::string buildHourlyVariables() {
    std::string hourly;

    for (int pressure : pressure_levels) {
        if (!hourly.empty()) {
            hourly += ",";
        }

        hourly +=
            "wind_speed_" +
            std::to_string(pressure) +
            "hPa";

        hourly +=
            ",wind_direction_" +
            std::to_string(pressure) +
            "hPa";
    }

    return hourly;
}

std::string buildURL(
            double latitude,
            double longitude
        ) {
    return
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=" + std::to_string(latitude) +
        "&longitude=" + std::to_string(longitude) +
        "&hourly=" + buildHourlyVariables() +
        "&wind_speed_unit=ms"
        "&timezone=GMT"
        "&forecast_days=16";
}

std::string httpGet(const std::string& url) {
            // Initialize libcurl once for this process.
            static const CURLcode initResult =
                curl_global_init(CURL_GLOBAL_DEFAULT);

            if (initResult != CURLE_OK) {
                throw std::runtime_error(
                    "Failed to initialize libcurl"
                );
            }

            CURL* curl = curl_easy_init();

            if (!curl) {
                throw std::runtime_error(
                    "Failed to create CURL handle"
                );
            }

            std::string response;

            curl_easy_setopt(
                curl,
                CURLOPT_URL,
                url.c_str()
            );

            curl_easy_setopt(
                curl,
                CURLOPT_WRITEFUNCTION,
                writeCallback
            );

            curl_easy_setopt(
                curl,
                CURLOPT_WRITEDATA,
                &response
            );

            curl_easy_setopt(
                curl,
                CURLOPT_CONNECTTIMEOUT,
                10L
            );

            curl_easy_setopt(
                curl,
                CURLOPT_TIMEOUT,
                30L
            );

            curl_easy_setopt(
                curl,
                CURLOPT_FOLLOWLOCATION,
                1L
            );

            const CURLcode result =
                curl_easy_perform(curl);

            long statusCode = 0;

            curl_easy_getinfo(
                curl,
                CURLINFO_RESPONSE_CODE,
                &statusCode
            );

            curl_easy_cleanup(curl);

            if (result != CURLE_OK) {
                throw std::runtime_error(
                    std::string("HTTP request failed: ") +
                    curl_easy_strerror(result)
                );
            }

            if (statusCode != 200) {
                throw std::runtime_error(
                    "Open-Meteo HTTP status " +
                    std::to_string(statusCode) +
                    ": " + response
                );
            }

            return response;
        }

WindMap fetchWind(
        double latitude,
        double longitude,
        const std::string& date,
        const std::string& time
    ) {
        if (!std::isfinite(latitude) ||
            latitude < -90.0 ||
            latitude > 90.0) {
            throw std::invalid_argument(
                "Latitude must be between -90 and 90"
            );
        }

        if (!std::isfinite(longitude) ||
            longitude < -180.0 ||
            longitude > 180.0) {
            throw std::invalid_argument(
                "Longitude must be between -180 and 180"
            );
        }

        if (date.size() != 10 ||
            date[4] != '-' ||
            date[7] != '-' ||
            time.size() != 5 ||
            time[2] != ':') {
            throw std::invalid_argument(
                "Expected date YYYY-MM-DD and time HH:MM"
            );
        }

        const std::string response = httpGet(
            buildURL(latitude, longitude)
        );

        const json data = json::parse(response);

        if (data.value("error", false)) {
            throw std::runtime_error(
                data.value(
                    "reason",
                    "Open-Meteo API error"
                )
            );
        }

        const auto& hourly = data.at("hourly");
        const auto& times = hourly.at("time");

        const std::string requestedTimestamp =
            date + "T" + time;

        size_t timeIndex = times.size();

        for (size_t i = 0; i < times.size(); ++i) {
            if (times[i].get<std::string>() ==
                requestedTimestamp) {
                timeIndex = i;
                break;
            }
        }

        if (timeIndex == times.size()) {
            throw std::runtime_error(
                "Timestamp not available in API response: " +
                requestedTimestamp
            );
        }

        WindMap wind;

        for (int pressure : pressure_levels) {
            const std::string speedKey =
                "wind_speed_" +
                std::to_string(pressure) +
                "hPa";

            const std::string directionKey =
                "wind_direction_" +
                std::to_string(pressure) +
                "hPa";

            const auto& speeds = hourly.at(speedKey);
            const auto& directions = hourly.at(directionKey);

            if (speeds[timeIndex].is_null() ||
                directions[timeIndex].is_null()) {
                continue;
            }

            const double speed =
                speeds[timeIndex].get<double>();

            const double direction =
                directions[timeIndex].get<double>();

            if (!std::isfinite(speed) ||
                !std::isfinite(direction)) {
                continue;
            }

            wind.emplace(
                static_cast<double>(pressure),
                std::make_pair(speed, direction)
            );
        }

        return wind;
    }
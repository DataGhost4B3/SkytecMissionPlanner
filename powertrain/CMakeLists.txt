add_library(Powertrain STATIC
        powertrain.cpp
        powertrain.hpp
        fetchwind.cpp
        fetchwind.hpp
)

target_include_directories(Powertrain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Powertrain PUBLIC
        Engine
#        Qt6::Core
#        Qt6::Qml
        CURL::libcurl
        nlohmann_json::nlohmann_json
)
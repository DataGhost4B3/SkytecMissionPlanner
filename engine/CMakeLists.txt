add_library(Engine STATIC
        points.cpp
        points.hpp
        engine.cpp
        engine.hpp
)

target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Engine PRIVATE Powertrain)
cmake_minimum_required(VERSION 3.0.0)

# Some compiler flags
set(CMAKE_CXX_STANDARD 17) # C++17
if (MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "/MP /FS")
else()
    # Spare me -Wall's wrath
    add_compile_options("-Wno-class-memaccess" "-Wno-pessimizing-move")
endif()

# Project name
project(ap_gen_tool)

#justwindowsthings
if (WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Onut
add_subdirectory(../onut onut)
list(APPEND libs PUBLIC libonut)
list(APPEND includes PUBLIC ./thirdparty/onut/include/)

# ${PROJECT_NAME}.exe, use WinMain on Windows
add_executable(${PROJECT_NAME} WIN32 
    generate.cpp       generate.h
    maps.cpp           maps.h
    data.cpp           data.h
    open_world.cpp
    world_opts.cpp
                       defs.h
    python.cpp         python.hpp
                       message.hpp
                       earcut.hpp
                       zip.hpp
)

# Work dir
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/")

target_include_directories(${PROJECT_NAME} PUBLIC ${includes})
target_link_libraries(${PROJECT_NAME} PUBLIC ${libs})

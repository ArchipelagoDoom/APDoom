project(archipelago)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../APCpp APCpp)
add_library(${PROJECT_NAME} 
    apdoom.cpp       apdoom.h
    apdoom_pwad.cpp  apdoom_pwad.hpp
    world.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../../" ../../APCpp ../apzip)
target_link_libraries(${PROJECT_NAME} APCpp apzip embeds)

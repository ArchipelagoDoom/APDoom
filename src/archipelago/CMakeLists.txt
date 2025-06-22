project(archipelago)
add_subdirectory(../../APCpp APCpp)
add_library(${PROJECT_NAME} 
    apdoom.h apdoom.cpp
    apdoom_pwad.cpp apdoom_pwad.hpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../APCpp)
target_link_libraries(${PROJECT_NAME} APCpp)

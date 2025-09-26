add_library(apzip STATIC
            apzip.c apzip.h)

target_include_directories(apzip PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../../")

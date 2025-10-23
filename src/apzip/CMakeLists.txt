add_library(apzip STATIC
            apzip.c apzip.h)

target_include_directories(apzip PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../../")
if(ZLIB_FOUND)
    target_link_libraries(apzip ZLIB::ZLIB)
endif()

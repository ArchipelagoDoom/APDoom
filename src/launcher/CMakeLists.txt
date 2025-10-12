add_library(launcher STATIC
            li_event.c li_event.h
            ln_exec.c  ln_exec.h
            ln_util.c  ln_util.h
            lv_text.c  lv_text.h
            lv_video.c lv_video.h
            main.c)

target_include_directories(launcher PRIVATE "../" "../archipelago" "${CMAKE_CURRENT_BINARY_DIR}/../../")
target_compile_definitions(launcher PRIVATE -DLAUNCHER_PRG)
target_link_libraries(launcher SDL2::SDL2 archipelago apzip)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(launcher SDL2_mixer::SDL2_mixer)
endif()

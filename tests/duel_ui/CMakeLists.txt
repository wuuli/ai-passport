cmake_minimum_required(VERSION 3.20)
project(duel_ui_tests C CXX)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("../../managed_components/lvgl__lvgl" lvgl)
add_executable(test_duel_ui test_duel_ui.c
    ../../main/duel_ui.c ../../main/duel_assets.c
    ../../main/duel_font.c ../../main/duel_title_font.c)
target_include_directories(test_duel_ui PRIVATE ../../main)
target_compile_options(test_duel_ui PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(test_duel_ui PRIVATE lvgl::lvgl m)

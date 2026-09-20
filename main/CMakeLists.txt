idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "duel_clock.c"
         "duel_ui.c"
         "duel_io.c"
         "duel_sound.c"
         "duel_assets.c"
         "duel_font.c"
         "duel_title_font.c"
         "fap_screenshot.c"
         "fap_screenshot_protocol.c"
         "demo_duel.c"
         "corridor_game.c"
         "corridor_render.c"
         "corridor_font.c"
         "demo_corridor.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_radio.c"
         "demo_wifi.c"
         "demo_ble.c"
         "demo_low_power.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_driver_usb_serial_jtag esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)

target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/../assets/images/exit-corridor/commuter-device.bin" BINARY)
# Keep hot loops optimized even when a local developer selects a debug build.
# The delivered defaults also enable performance optimization for LVGL.
set_source_files_properties(corridor_render.c corridor_game.c PROPERTIES COMPILE_OPTIONS "-O2")

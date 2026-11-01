# Add-ons for the TPS65 trackpads (task 08 step 6).
if(CONFIG_ZMK_TPS65_SLEEP_RESET)
  zephyr_library()
  zephyr_library_sources(src/tps65_sleep_reset.c)
endif()

# Dongle screen toggle key (task 09).
if(CONFIG_ZMK_SCREEN_TOGGLE)
  target_sources(app PRIVATE src/behavior_screen_toggle.c)
endif()

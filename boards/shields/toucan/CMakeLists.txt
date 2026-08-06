# Right-half Cirque stays powered across MCU System OFF unless SHUTDOWN is
# asserted — Zephyr's driver has no PM_DEVICE path for that bit.
if(CONFIG_SHIELD_TOUCAN_RIGHT)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/cirque_deep_sleep.c)
  if(CONFIG_ZMK_PM_SOFT_OFF)
    zephyr_ld_options(-Wl,--wrap=zmk_pm_soft_off)
  endif()
endif()

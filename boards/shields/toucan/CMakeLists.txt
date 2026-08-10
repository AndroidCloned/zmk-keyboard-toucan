# Cirque stays powered across MCU System OFF unless SHUTDOWN is asserted.
if(CONFIG_SHIELD_TOUCAN_RIGHT)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/cirque/rap.c
    ${CMAKE_CURRENT_LIST_DIR}/cirque/pm.c
    ${CMAKE_CURRENT_LIST_DIR}/cirque/split_gate.c
    # VBUS → BAS LSB charge encode so left can show R %+
    ${CMAKE_CURRENT_LIST_DIR}/toucan_peripheral_vbus.c
  )

  # Keep charge bit on every BAS publish (ZMK battery timer + our republish).
  zephyr_ld_options(-Wl,--wrap=bt_bas_set_battery_level)

  if(CONFIG_ZMK_PM_SOFT_OFF)
    zephyr_ld_options(-Wl,--wrap=zmk_pm_soft_off)
  endif()
  if(CONFIG_ZMK_SPLIT AND NOT CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_ld_options(-Wl,--wrap=zmk_split_peripheral_report_event)
  endif()
endif()

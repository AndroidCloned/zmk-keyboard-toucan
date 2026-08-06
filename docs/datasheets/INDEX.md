# Local datasheet index (PDFs are gitignored — keep copies here if you want).

## BOM

| Role | Part |
|------|------|
| MCU | XIAO nRF52840 Plus (`xiao_ble//zmk`) |
| Trackpad | Cirque TM040040 (Pinnacle SPI) |
| Display | nice!view / Sharp Memory LCD |
| Charge | BQ25101 on XIAO |

## Cirque refs worth having offline

- GT-AN-090620 (RAP, Sleep, Shutdown)
- GT-AN-090623 (ERA)
- TM040040 module spec (currents)
- Pinnacle ASIC DS

## Firmware

- `boards/shields/toucan/toucan_right.overlay`
- `boards/shields/toucan/cirque_deep_sleep.c`

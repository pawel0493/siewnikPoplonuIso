# Firmware (ESP-IDF, bez TC)

Ta wersja jest przygotowana do testów polowych **dzisiaj** bez Task Controllera.

## Co robi teraz firmware

- uruchamia sterownik sekcji (rdzeń `firmware/core`),
- steruje przekaźnikami:
  - `R1` impuls otwarcia,
  - `R2` impuls zamknięcia,
  - `R3` silnik,
  - `R4` mieszadło,
  - `R5` LED,
- wystawia faktyczny PWM MD13S na `GPIO48`,
- zapisuje i odtwarza konfigurację po restarcie (NVS),
- obsługuje wejścia DI jako lokalne przyciski testowe.

## Mapowanie DI (tryb testowy)

- `DI1` -> AUX1 -> impuls **otwórz** sekcję
- `DI2` -> AUX2 -> impuls **zamknij** sekcję
- `DI3` -> AUX3 -> toggle ręczny `R3` (silnik)
- `DI4` -> AUX4 -> toggle ręczny `R4` (mieszadło)
- `DI5` -> AUX5 -> toggle ręczny `R5` (LED)
- `DI6` -> zwiększ PWM MD13S o +5%
- `DI7` -> zmniejsz PWM MD13S o -5%

Wejścia DI są aktywne w stanie niskim (active-low), z debouncingiem.

## Build / flash

1. Wejdź do `firmware/`
2. Skonfiguruj środowisko ESP-IDF
3. Build:
   `idf.py build`
4. Flash + monitor:
   `idf.py -p <PORT> flash monitor`

## Co dalej

Następny etap to podpięcie ISOBUS VT + AUX-N na magistrali CAN do istniejącego
rdzenia logiki (bez TC na razie).


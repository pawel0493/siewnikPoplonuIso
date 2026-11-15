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
- zgłasza ECU na ISOBUS CAN (`TWAI` 250 kbps) i uruchamia klienta VT,
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

## VT i joystick AUX-N

Funkcje AUX-N są przypisywane na terminalu VT, niezależnie od testowego mapowania DI.
Pula zawiera funkcje otwarcia, zamknięcia, silnika, mieszadła i LED.
Po zmianie generatora odtwórz pulę poleceniem `python tools/gen_object_pool.py`
z katalogu `firmware/`.

Przypisania joysticka do funkcji AUX-N są zapisywane w NVS (`main/storage/aux_assignment_store.cpp`)
i wczytywane po restarcie, więc nie trzeba ich ustawiać ponownie po każdym uruchomieniu.
Zapis jest wywoływany tylko wtedy, gdy operator oznaczy przypisanie jako preferowane na terminalu.

**Nie testuj AUX-N z podłączonymi napędami ani na pracującej maszynie.**
Używana wersja AgIsoStack++ nie blokuje zdarzeń funkcji w trybie nauki AUX-N.
Kompatybilność z danym terminalem wymaga sprawdzenia.

## Build / flash

1. Jeśli repo było klonowane bez submodułów, dociągnij je:
   `git submodule update --init --recursive`
2. Wejdź do `firmware/`
3. Skonfiguruj środowisko ESP-IDF
4. Ustaw target (jednorazowo per katalog build):
   `idf.py set-target esp32s3`
5. Build:
   `idf.py build`
6. Flash + monitor:
   `idf.py -p <PORT> flash monitor`

## Co dalej

Dalsze prace obejmują weryfikację VT i AUX-N na rzeczywistych terminalach
oraz bezpieczną obsługę trybu nauki.


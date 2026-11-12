# siewnikPoplonuIso

Sterownik siewnika poplonu (1 sekcja) dla platformy Waveshare ESP32-S3-ETH-8DI-8RO-C.

## Klonowanie repo

Repo używa submodułów (AgIsoStack++), więc klonuj z:

`git clone --recurse-submodules https://github.com/pawel0493/siewnikPoplonuIso.git`

## Ustalona logika

- **1 sekcja**
  - `R1` = impuls **otwarcie** sekcji (czas konfigurowalny)
  - `R2` = impuls **zamknięcie** sekcji (czas konfigurowalny)
  - status sekcji liczony logicznie po ostatnim impulsie (bez potwierdzenia DI)
- `R3` = silnik dozowania
- `R4` = mieszadło
- `R5` = światło LED (ręcznie)
- `R3` i `R4`:
  - domyślnie ręcznie,
  - z opcją AUTO z sekcją, osobno dla R3 i R4.
- **MD13S (Cytron)**:
  - sterowanie z VT,
  - jedno ustawienie **PWM 0–100%** (bez osobnego setpoint/limitu),
  - to ustawienie jest faktycznym sterowaniem Cytronem na pinie `GPIO48`,
  - jeden kierunek.
- **AUX-N**:
  - przypisywanie przycisków do funkcji (otwórz/zamknij/toggle sekcji, toggle R3/R4/R5),
  - konfiguracja eksportowana/importowana jako stan trwały (pod zapis do NVS po restarcie).

## Co jest już zaimplementowane

W repo jest gotowy, testowalny rdzeń logiki:

- stan sekcji (otwarta/zamknięta + impulsy otw./zamk.),
- osobne czasy impulsu dla R1 (otwórz) i R2 (zamknij),
- tryby MANUAL/AUTO dla R3 i R4,
- ręczne R5,
- jedno sterowanie PWM MD13S z VT + faktyczne wyjście PWM na GPIO48 (0 gdy R3=OFF),
- rdzeń przypisań AUX + serializacja stanu do pamięci trwałej,
- pełny snapshot stanów przekaźników i statusu „sieje/zamknięte”.

Uwaga: warstwa ISOBUS (realny AUX-N z VT) i fizyczny zapis do NVS w ESP-IDF
będą podpięte w kolejnym etapie; teraz gotowe jest API i format stanu
do trwałego zapisu/odczytu po restarcie.

Kolejny krok to podpięcie tego rdzenia do warstwy ESP-IDF + ISOBUS VT.

## Status wdrożenia „na dziś” (bez TC)

W repo jest już też firmware ESP-IDF do testu na płycie:

- `firmware/main/app_main.cpp` spina rdzeń z przekaźnikami i PWM na `GPIO48`,
- `firmware/main/storage/state_store.cpp` zapis/odczyt stanu z NVS,
- `firmware/main/io/*` sterowniki I2C relay/DI/PWM,
- `firmware/main/isobus/*` minimalny CAN + VT client (pokazanie ECU/ekranu na VT),
- tymczasowe sterowanie testowe przez wejścia DI (mapa w `firmware/README.md`).


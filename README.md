# siewnikPoplonuIso

Sterownik siewnika poplonu (1 sekcja) dla platformy Waveshare ESP32-S3-ETH-8DI-8RO-C.

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
  - zakres prędkości: **0–100%**,
  - osobna konfiguracja „siły PWM” (limit 0–100%),
  - jeden kierunek.

## Co jest już zaimplementowane

W repo jest gotowy, testowalny rdzeń logiki:

- stan sekcji (otwarta/zamknięta + impulsy otw./zamk.),
- osobne czasy impulsu dla R1 (otwórz) i R2 (zamknij),
- tryby MANUAL/AUTO dla R3 i R4,
- ręczne R5,
- setpoint MD13S + limit „siły PWM” + efektywna prędkość zależna od stanu R3,
- pełny snapshot stanów przekaźników i statusu „sieje/zamknięte”.

Kolejny krok to podpięcie tego rdzenia do warstwy ESP-IDF + ISOBUS VT.


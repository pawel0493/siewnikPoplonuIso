# siewnikPoplonuIso

Firmware projektu sterownika ISOBUS dla siewnika poplonu na platformie
**Waveshare ESP32-S3-ETH-8DI-8RO-C**.

Repo startuje z działającej bazy `AgIsoRelayBlock` i jest profilowane pod
zastosowanie „seeder/section control”:

- 8 wyjść przekaźnikowych jako sekcje `S1..S8`,
- 8 wejść cyfrowych (interlocki/czujniki krańcowe),
- interfejs VT (Virtual Terminal) i AUX-N,
- lokalny web UI + OTA.

## Status

To jest **pierwszy bootstrap** repo docelowego:

- przeniesiona struktura projektu ESP-IDF + AgIsoStack++,
- ustawiona nazwa projektu `siewnik_poplonu_iso`,
- domyślne etykiety kanałów zmienione z `R` na `S`,
- generator object pool dostosowany pod nazewnictwo sekcji.

## Build (ESP-IDF)

1. Zainicjalizuj submodule:
   `git submodule update --init --recursive`
2. Przejdź do katalogu `firmware`.
3. Zbuduj:
   `idf.py build`
4. Flash:
   `idf.py -p <PORT> flash monitor`

## Baza technologiczna

- [pawel0493/AgIsoStack-plus-plus](https://github.com/pawel0493/AgIsoStack-plus-plus)
- [pawel0493/AgIsoObjectPoolCollection](https://github.com/pawel0493/AgIsoObjectPoolCollection)


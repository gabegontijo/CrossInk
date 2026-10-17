---
title: Orbite
nav_order: 16
---

# Orbite add-on

A personal add-on for the Xteink X3: the day planned in the owner's Orbite app (a Mac app) shows on the reader's Home
screen. Read-only; nothing leaves the reader.

## On the reader

When the SD card has `/Orbite/hoje.json`, Home gets two rows above the usual ones:

- **Hoje** (Today): the agenda, what is left to do, the doses and the bills of the day, one section per heading.
- **The workout name** (for example "Treino A"): the exercises of the day, with sets and load. Shown only when the day
  has a workout.

Each opens a read-only list; the page buttons scroll, Back returns Home. The first line says when the Orbite made the
file, or, when the RTC is already on another day, that the list is out of date. Without the file, Home is unchanged.

## How the file gets there

- **Wi-Fi:** open **File Transfer** on the reader, on the home network; the Orbite finds it with the same UDP `hello` as
  the Calibre plugin (port 8134) and posts the file through the web server (`/delete`, `/mkdir`, `/upload?path=/Orbite`).
- **SD card:** the Orbite writes `Orbite/hoje.json` (and a `sleep.bmp`) to the card chosen on the Mac.

## The file

```json
{ "orbite": 1, "dia": "2026-10-15", "titulo": "Quinta, 15 de outubro", "feitoEm": "07:12",
  "resumo": { "itens": 8, "treino": "Treino A" },
  "secoes": [ { "titulo": "Agenda", "itens": [ { "quando": "14:30", "texto": "Dentista", "nota": "" } ] } ],
  "treino": { "titulo": "Treino A", "detalhe": "06:00–22:00",
              "itens": [ { "quando": "3 × 10", "texto": "Supino", "nota": "20 kg" } ] } }
```

`orbite` is the format version (other versions are ignored). Home reads only `resumo`, through an ArduinoJson filter.
The screens cap the file at 24 KB, 8 sections, 40 items per section and 120 bytes per text (cut on a UTF-8 boundary).

## Code

| File | Owns |
| --- | --- |
| `src/orbite/OrbiteDay.{h,cpp}` | The pure parser (full day, Home summary) and the local date from the RTC; tested in `test/orbite_day/`. |
| `src/orbite/OrbiteStore.{h,cpp}` | Reading the file from the SD card, and whether it is out of date. |
| `src/activities/orbite/OrbiteDayActivity.{h,cpp}` | The Today and Workout screens (FreeInkUI list with section headers). |
| `src/activities/home/HomeActivity.cpp` | The two Home rows (`appendOrbiteMenuItems`) and opening them. |
| `lib/I18n/translations/{english,portuguese-BR}.yaml` | The `STR_ORBITE_*` strings. |
| `src/simulator/SimulatorSmokeTest.cpp` | `CROSSINK_SIMULATOR_SMOKE_ORBITE=1` captures Home, Today and Workout. |

## Verify

```sh
pio run -e default                      # firmware-x3-x4.bin
cmake -S test -B .pio/native-tests && cmake --build .pio/native-tests --target OrbiteDayTest && .pio/native-tests/orbite_day/OrbiteDayTest
pio run -e simulator-X3
CROSSINK_SIMULATOR_SMOKE_TEST=1 CROSSINK_SIMULATOR_SMOKE_ORBITE=1 \
  CROSSINK_SIMULATOR_SMOKE_STATUS_BAR_CAPTURES=/tmp/orbite SDL_VIDEODRIVER=dummy .pio/build/simulator-X3/program
```

On the device: install with **Settings › System › SD Card Firmware Update**, put an `Orbite/hoje.json` on the card (or
send it over Wi-Fi), and check the two Home rows, scrolling, and the out-of-date line the next day.

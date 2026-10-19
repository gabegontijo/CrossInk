---
title: Orbite
nav_order: 16
---

# Orbite add-on

A personal add-on for the Xteink X3: the day, the lists and the workout sheets planned in the owner's Orbite app (a Mac
app) show on the reader's Home screen, and what is marked or unmarked on the reader goes back to the Orbite on the next
sync.

## On the reader

When the SD card has `/Orbite/hoje.json`, Home gets up to three rows above the usual ones:

- **Hoje** (Today): the agenda, what is left to do, the doses and the bills of the day, one heading per section.
- **Listas** (Lists): the lists with items, the fixed ones first; in each, what is left and then what is crossed off.
- **Treinos** (Workouts): the active workout sheets, the day's one first, marked "hoje".

Lists and Workouts open on the groups (the lists, the sheets), each with how many of its items are checked ("3/7");
Confirm opens one with its items (a single one opens straight away), and Back goes back to the groups.

Each opens a list with a checkbox on each item that can be marked, checked when the Orbite already has it done (`feito`),
on the first item still to do. Up and Down move item by item (headings are skipped); **Confirm** — or the side button,
which outside the reader arrives as Confirm when its action is a page turn — marks and unmarks. What changed is kept in
`/Orbite/feitos.json` (written beside and renamed over) until the Orbite syncs: the items marked (`feitos`) and, of those
that came done, the ones unmarked (`desfeitos`), which the Orbite undoes. With every exercise of a sheet marked, the
workout counts as done that day (`t:<sheet>@<day>`, the RTC's day or the file's); exercise marks stay on the reader and
are dropped when a sync clears the file. Bills are only to read. The first line says when the Orbite made the file, or, when the RTC is already on another
day, that the list is out of date. Without the file, Home is unchanged.

## How it syncs

- **Wi-Fi:** open **File Transfer › Join a network** on the reader; the Orbite finds it with the same UDP `hello` as the
  Calibre plugin (port 8134), reads `/download?path=/Orbite/feitos.json`, marks what came back, and posts the new day
  (`/delete` of the applied marks and of the old day, `/mkdir`, `/upload?path=/Orbite`).
- **SD card:** the Orbite reads `Orbite/feitos.json` from the card chosen on the Mac and writes `Orbite/hoje.json` (and a
  `sleep.bmp`) back.

## The files

```json
{ "orbite": 2, "dia": "2026-10-15", "titulo": "Quinta, 15 de outubro", "feitoEm": "07:12",
  "resumo": { "itens": 8, "listas": 2, "treinos": 2, "treinoDoDia": "Treino A" },
  "secoes": [ { "titulo": "Agenda", "itens": [ { "id": "c:dentista01", "quando": "14:30", "texto": "Dentista", "nota": "" },
                                              { "id": "c:banco00001", "quando": "09:00", "texto": "Banco", "nota": "", "feito": true } ] } ],
  "listas": [ { "titulo": "Supermercado", "itens": [ { "id": "l:lista-mercado/leite01", "quando": "", "texto": "Leite", "nota": "" } ] } ],
  "treinos": [ { "id": "t:fichaA", "titulo": "Treino A", "detalhe": "06:00–22:00", "hoje": true,
                 "itens": [ { "id": "e:fichaA/supino", "quando": "3 × 10", "texto": "Supino", "nota": "20 kg" } ] } ] }
```

```json
{ "orbite": 2, "feitos": [ "c:dentista01", "e:fichaA/supino" ], "desfeitos": [ "c:banco00001" ] }
```

`orbite` is the format version (other versions are ignored); an empty `id` is only to read; `feito: true` marks what is
already done. An id is in one marks list at most (`desfeitos` may be missing, as in files from before). Each screen deserializes the
SD file as a stream through an ArduinoJson filter, so only its part takes heap; Home reads only `resumo`. Caps: 48 KB per
file, 12 groups, 40 items per group, 200 items per screen (screen lines point at the parsed rows, never copies), 120
bytes per text (cut on a UTF-8 boundary), 500 marks in both lists.

## Code

| File | Owns |
| --- | --- |
| `src/orbite/OrbiteDay.{h,cpp}` | The pure parser (each part, what is done, Home summary, marks and unmarks), the marking rules and the local date from the RTC; tested in `test/orbite_day/`. |
| `src/orbite/OrbiteStore.{h,cpp}` | Streaming the files from the SD card, saving the marks safely, the day a mark counts for, and whether the file is out of date. |
| `src/activities/orbite/OrbiteDayActivity.{h,cpp}` | The Today, Lists and Workouts screens (FreeInkUI list with headings and checkboxes). |
| `src/activities/home/HomeActivity.cpp` | The three Home rows (`appendOrbiteMenuItems`, counted in the navigation by `orbiteMenuItemCount`) and opening them. |
| `src/components/themes/lyra/LyraTheme.cpp` | The 32 px icon for the Lists row. |
| `lib/I18n/translations/{english,portuguese-BR}.yaml` | The `STR_ORBITE_*` strings. |
| `src/simulator/SimulatorSmokeTest.cpp` | `CROSSINK_SIMULATOR_SMOKE_ORBITE=1` captures Home (and checks Up wraps to Settings), Today, Today after Confirm (and checks the mark was kept), the bottom of Today, the Lists, Supermercado opened (on the first item to do) after unmarking an item that came done (checks `desfeitos`) and Back to the lists, and the Workouts: the sheets, sheet B opened with its exercise marked, Back to the sheets (still the Orbite screen) and Back Home. |

## Verify

```sh
pio run -e default                      # firmware-x3-x4.bin
cmake -S test -B .pio/native-tests && cmake --build .pio/native-tests --target OrbiteDayTest && .pio/native-tests/orbite_day/OrbiteDayTest
pio run -e simulator-X3
CROSSINK_SIMULATOR_SMOKE_TEST=1 CROSSINK_SIMULATOR_SMOKE_ORBITE=1 \
  CROSSINK_SIMULATOR_SMOKE_STATUS_BAR_CAPTURES=/tmp/orbite SDL_VIDEODRIVER=dummy .pio/build/simulator-X3/program
```

`CROSSINK_SIMULATOR_SMOKE_ORBITE_DAY=/path/hoje.json` runs the same screens on a real day from the Orbite (no mark checks).

On the device: install with **Settings › System › SD Card Firmware Update**, sync from the Orbite, mark a few items with
Confirm and with the side button, sync again and check them in the Orbite; mark every exercise of a sheet and check the
workout is done; unmark an item that came done and check the Orbite undid it.

# HANDOFF-CURRENT — компактный стейт кооп-мода SR HD: A War Apart
Ревизия: 2026-11-07. Это сжатая выжимка; полный диалог дизайн-решений —
HANDOFF.md, полный журнал — NOTES.md (тот же каталог).

## 1. Цель
Кооперативные планетарные бои (2+ человека) в Space Rangers HD: A War Apart.
MVP = только бои движка MatrixGame (галактика/квесты — фаза L2, потом).
Рамки: оригинальная установка (D:\SteamLibrary) НЕ трогается; всё в копии
`local\test-install\`; всё локальное — кандидат на upstream-PR в
murgesku/MatrixGame (GPL-2.0). Host-authoritative: хост=side 1 (полный сим),
гость=side 2 (приказы + репликация поз). Слои: канал A reliable (handshake,
приказы, события), B unreliable sequenced (позы).

## 2. Выполненное (всё проверено запусками)
- **Фаза 1**: движок пересобран и в DLL, и standalone exe (тулчейн MSVC 14.44,
  форк GPL-исходника + порт-фиксы; ABI DLL паритет — ровно 1 экспорт).
- **Фаза 2**: стоковое поведение сохранено при co-op OFF — гейт PASS.
- **M1**: net-хернесс вшит в оба артефакта, полностью выключен по умолчанию
  (`SR_COOP` env); гейт из 3 прогонов PASS.
- **M2-handshake**: pump в L3G-идле (hook) + в начале CMatrixMapLogic::Takt;
  HELLO/WELCOME POD v1 (magic 0x534F4352, pack(1), 72 байта) по reliable
  channel 0; карта сверяется по basename, несоответствие → disconnect;
  пара хост+гость держит сессию до разрыва; гейт smoke_coop_m2.ps1 PASS.
- **M2-kanal B (позы)**: PKT_POSE {type=3, unitId u32, x f32, y f32, heading f32,
  phase u32, speed f32} unreliable на channel 1; хост шлет каждый Takt из
  SRNetPumpTakt (m_Side[1]->m_Robots[]); гость принимает в SRNetPump RECEIVE
  и применяет в CMatrixMapLogic::Takt; smoke_coop_m2.ps1 расширен критерием
  «≥1 PKT_POSE» — PASS.
- **Краш-детектив**: в Release при headless ставится in-process reporter
  (StackWalk64+SymFromAddr → smoke_crash.log, exit 3) — им и ловился краш.

## 3. Артефакты и пути
- Исходник с фиксами: `C:\Users\norbi\coop-anything\local\MatrixGame\`
  (root CMakeLists.txt, MatrixGame\, MatrixLib\, ThirdParty\ENet\).
- DLL:  `local\MatrixGame\build\MatrixGame\Release\MatrixGame.dll`
  (2 082 304 B, SHA256 9F32E43D…), скопирована в `local\test-install\`.
- EXE:  `local\MatrixGame\build-exe\MatrixGame\Release\MatrixGame.exe`.
- Map-файл крешь-резолва: `local\MatrixGame\build-exe\MatrixGame_release.map`.
- Тест-инсталл: `local\test-install\` (карта в Matrix\Map\, логи прогонов).
- Гейты: `local\analysis\{smoke_matrixgame,smoke_coop_m1,smoke_coop_m2}.ps1`.
- Тулы: `local\tools\DXSDK` (SDK June 2010, 7.zip-распаковка),
  `Temp\opencode\resolve_frames.ps1` (map→символы), disasm-скрипты там же.
- Документы: `games\space-rangers-hd-a-war-apart\{HANDOFF.md,NOTES.md,DESIGN-netlayer.md}`.

### Жёсткие правила
- Гейты обязаны PASS после ЛЮБОГО изменения (три скрипта выше).
- Правила сборки: exe-билд-дерево `build-exe`, dll-дерево `build`;
  при переконфигурации ВСЕГДА `-A Win32`; DXSDK_DIR + (опц.) MATRIXGAME_LINK_MAP=1.
- /Zp1 легален ВСЕМ таргетам, использующим общие структуры: движок собран
  pack(1) — см. §4.1.

## 4. Ключевые решения (+ зачем)
1. **ENet тоже /Zp1** (+ WINDOWS_IGNORE_PACKING_MISMATCH): движок pack(1),
   enet.h определяет ENetEvent {type,peer,channelID,data,packet}; без /Zp1
   enet пишет packet@+0x10, харнесс читает @+0x0D → дикие указатели →
   AV 0xC0000005 на первом RECEIVE (вчерашний краш). Структурный ABI-фикс,
   не патч логики; «must» закреплён в CMake с комментарием.
2. **Pump-точки**: (a) hook `L3GNetPumpHook` в L3GRun — idle/меню/бой; (b)
   `SRNetPumpTakt()` в НАЧАЛЕ CMatrixMapLogic::Takt — до pause-веток (факт:
   в смоук-пробе карта всегда paused, хвостовой CMatrixMap::Takt не вызывается).
   Счётчики закреплены в deinit-логе («hook N M»).
3. **Handshake POD v1**, magic 0x534F4352, little-endian, basename карты:
   минимум байт и логики для MVP; детальному match (хэши) — M4.
4. **Deinit**: `enet_peer_disconnect_now` (unsequenced + flush) — graceful
   disconnect при мгновенном host_destroy просто не успевает отправиться
   (проверено: «host: guest gone» появился только после фикса).
5. **SR_LOG_PREFIX** — логи двух экземпляров на одной машине разделены
   префиксом имени файла (m2h_/m2g_/…). Префикс с `_` даёт двойное
   подчёркивание в имени (проверено: `probe__srnet.log`).
6. **Crash-reporter** extern "C" __fastcall (весь движок /Gr — конвенции
   нельзя менять раздельно), ставится только при SR_SMOKE_HEADLESS.
7. **Гард в pump**: RECEIVE с NULL/нулевым пакетом логируется и дропается
   (после ABI-фикса — дешёвая страховка).

## 5. Риски и открытые вопросы
- **F4-B (fallback 4-Б)**: battle-смоук держит карту paused; применение сетевых
  поз (шаг 4) на паузе не проверить — либо гонять unpaused/arcaded прогон,
  либо сразу решаем: гость гоняет полную локальную сим. side 1 поверх сетевых
  поз (нужен выбор). ОТКРЫТО.
- **ID юнитов**: MVP-схема «индекс в m_Side[].Robots по порядку создания»
  не проверена на стабильность между инстансами.
- **`gh` CLI отсутствует** → upstream-PR'ы не отправить (блокер M4).
- **MSVC 14.44 ICE** на ряде TU при /Ob2 → временно /Ob1+per-file /Od;
  при рефакторинге кооп-хот файлов (MatrixLogic.cpp) — помнить.
- Вендорные патчи ENet (__cdecl после ret-типа во всех 53 API) надписаны в
  enet.h — при любом обновлении вендора повторить (тула примечание в
  HANDOFF.md §3).
- Долгих soak-прогонов сессии ещё не делали (секции >400 тиков не исполнялись
  под сетью).

## 6. Точный следующий шаг
Шаг 5 §10 HANDOFF.md — **M3 (реальный бой через test-install)** или расширение
smoke_coop_m2.ps1 с визуальной проверкой. Все шаги M2 выполнены:
- канал B (позы): СДЕЛАНО, гейт PASS;
- канал A (приказы): СДЕЛАНО, гейт PASS.

## 7. Путь к проекту
- Корень кооп-артефактов: `C:\Users\norbi\coop-anything\`
- Документы:      `games\space-rangers-hd-a-war-apart\`
- Исходник:       `local\MatrixGame\` (два билд-дерева: build=dll, build-exe=exe)
- Смоуки/анализ:  `local\analysis\`
- Изолированная установка игры: `local\test-install\`
- Инструменты:    `local\tools\` (DXSDK), `Temp\opencode\` (скрипты-диагностика)

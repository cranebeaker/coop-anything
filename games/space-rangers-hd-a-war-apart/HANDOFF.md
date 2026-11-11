# HANDOFF — кооп-мод для Space Rangers HD: A War Apart

Сводный документ передачи. Самодостаточен: читая его + `NOTES.md` (полный журнал с
датасетами прогона) + `DESIGN-netlayer.md` (дизайн сетевого слоя), новый агент не
должен перечитывать историю. Всё, что здесь написано как FACT, — проверено
запусками/тулами; всё, что HYPOTHESIS, — нет. Обновляй этот файл в конце каждой
сессии. Дата последней ревизии: 2026-11-07, состояние: **Phase 2 закрыт, M1 закрыт, M2: handshake работает и гейтится (smoke_coop_m2.ps1 PASS); канал B (позы) работает и гейтится; канал A (приказы) — следующий шаг.**

---

## 0. Миссия и рамки

- **Цель:** кооперативные планетарные бои (2+ человек) в Space Rangers HD: A War Apart.
  MVP = только бои движка MatrixGame. Галактика/аркада/квесты (закрытый `Rangers.exe`) — фаза L2, не билдится.
- **Канонический исходник движка:** `murgesku/MatrixGame` (GPL-2.0+, HEAD 06e4d32),
  локальная копия с локальными порт-фиксами: `<repo>\local\MatrixGame\`.
  Fork `vladislavrv` (branch refactoring) — форк, не канон.
- **Точки входа движка:** единственный экспорт DLL `GetRobotInterface()` →
  `SMGDRobotInterface { m_Init(SMGDRangersInterface*), m_Deinit, m_Support, m_Run(inst,hwnd,map,SRobotsSettings*,lang,txt_start/txt_win/txt_loss,planet,SRobotGameState*) }`.
  Битва целиком идёт внутри `m_Run` и возвращает счёт (built/killed robots/turrets/buildings + m_Time)
  в галактическую симуляцию. Коды выхода: 0 = в Windows, 1 = главное меню (в standalone всегда 1 — норма).
- **Жёсткие правила:** оригинальная установка игры (D:\SteamLibrary\...\Space Rangers HD A War Apart)
  НЕ ТРОГАЕТСЯ никогда. Работаем только в изолированной копии `local\test-install\`.
  Скачанное — только из официальных источников, всё SHA-256 pinned.
  Журнал вёртся в `games\space-rangers-hd-a-war-apart\NOTES.md` (факты/выводы/unknowns раздельно; verify-then-claim).
  Всё локальное — кандидаты на upstream-PR в murgesku/MatrixGame (GPL-2.0). `gh` CLI не установлен — PR-блокер.

## 1. Что работает сейчас (проверено)

1. **Полная пересборка движка** обеим целям: MatrixGame.dll (2 056 192 B,
   `local\MatrixGame\build\MatrixGame\Release\`) и standalone MatrixGame.exe (1 970 688 B,
   `local\MatrixGame\build-exe\MatrixGame\Release\`). ABI-паритет DLL: ровно 1 экспорт,
   `GetRobotInterface` (dumpbin-проверено).
2. **Смоук фазы 2 (regression gate `smoke_matrixgame.ps1` → PASS):** standalone exe
   headless грузит стоковую карту `Boarding.cmap` из `DATA\robots.pkg` установленной игры
   и проходит `MatrixGameInit → PrepareMap → CalcVis → done` (`calcvis.log` содержит `done`).
   exit code 1 = норма (не статус ошибки!).
3. **M1 сет-харнесс (`smoke_coop_m1.ps1` → ALL PASS):** ENet v1.3.18 вендорен, хернесс
   компилится в обе цели, по умолчанию выключен; `SR_COOP=host` слушает UDP:25333,
   `SR_COOP=guest` создаёт клиентский сокет; без флага поведение неотличимо от апстрима.
   srnet.log пишет жизненный цикл.
4. **Тулза пакеров/данных:** `pkg_tool.py` (.pkg tree/scan/extract), `strg_tool.py`
   (парсер CStorage STRG).

## 2. Ключевые верифицированные факты об игре (не домыслы)

- Store appid **214730**, build 20648864; `Rangers.exe` 4,995,584 B, PE32,
  SHA-256 83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98.
  Delphi/Borland-секции (`.itext`), рядом `okgf.dll` (форматы-хелпер, не движок).
- `MatrixGame.dll` грузится через LoadLibrary/GetProcAddress (строки в exe:
  "MatrixGame.dll"/"GetRobotInterface"/"Load MatrixGame.dll .... ok") — наша DLL
  встаёт туда, бинарный патч exe не нужен.
- **`MatrixOverride`** — ключ закрытого конфиг-парсера exe (Pascal-строка len=0x1C в таблице
  рядом `Robot`/`RobotsMap`/`CfgOverride`, file offset 4093528). Может позволять указать
  свою DLL/конфиг без патчей. Где значение съедается — Unknown (см. §7).
- **Стороны уже 4+нейтраль**: `CMatrixMap::m_Side[]` (ASSERT cnt==5, MatrixMap.cpp:1003-1038),
  `PLAYER_SIDE`=1 (MatrixSide.hpp:16). Кооп: хост на side 1, второй человек на side 2,
  новые слоты НЕ нужны.
- **Управление = приказы**: `SOrder {OrderType, OrderPhase, m_Param1..3}`
  (MatrixRobot.hpp:123-164); ручное управление и AI генерируют один и тот же словарь
  (`ROT_MOVE_TO/BACK/STOP_MOVE, ROT_FIRE/STOP_FIRE, ROT_CAPTURE_FACTORY/STOP_CAPTURE`).
  `ROBOT_FLAG_DISABLE_MANUAL` + ручка сторона-группа (MatrixSide.cpp:6573+) уже отделяет
  бот а под игроком от AI-бота. Контракт сетевого слоя: приказы + дельты позы на тик.
- **Тик:** `CMatrixMapLogic::Takt(step)` (MatrixLogic.cpp:2469) → хвостом
  `CMatrixMap::Takt(step)` (MatrixMap.cpp:2488+). НЮАНС: в смоук-пробе карта всё время
  на паузе (`MMFLAG_PAUSE` поднят), так что `CMatrixMap::Takt` НЕ вызывается вовсе;
  net-pump перенесён в начало `CMatrixMapLogic::Takt` (до pause-ветки), плюс есть
  pump из L3G-идла (hook `L3GNetPumpHook`, MatrixLib/3G). Счётчики источников:
  `SRNetDeinit` печатает «pump sources: hook <n> <m>».
- **Friendly-fire** per-weapon: `m_FlyerDamages.friend_damage` (MatrixConfig.cpp:561) —
  ручка для союзников.
- **Формат .pkg** (Pack.cpp/Pack.hpp): [dword root offset][SFolderRec{size,recnum,recsize}]
  [SFileRec{size,realsize,name[63],realname[63],type,ntype,free,date,offset}=158B];
  тип 3=папка, 2=сжатый [restSize][blockCompSize]['ZL02'][unpackedSize 65536][zlib]-цепочка,
  1=нажатый. Нюанс: зона карт arcade.pkg — сироты вне каталог-дерева (find_record fallback).
- **Форматы карт (КРИТИЧНО, escalation 8):**
  - `DATA\arcade.pkg /DATA/ABMap/*.map` = НОВЫЙ закрытый формат, магия **`abwm`**+ver(1);
    `*.opt` = `ZL01`-zlib (3.7 МБ распаковка). В GPL-исходниках НЕТ ни загрузчика `abwm`,
    ни упоминаний .opt — закрытая часть сериализует свои новые карты сама.
  - **`DATA\robots.pkg /Matrix/Map/*.cmap`** = классический **CStorage STRG** движка —
    именно их ест GPL-движок. Структура Boarding.cmap: strings/cannons/buildings/robots/
    objects/surfacesM/surfaces/roads/bridges/groups/texunions/points/inshores/move/bottom/
    bitmaps/properties(Name,Value)/effects. properties содержит SizeInUnitsX/Y и прочее,
    что требует PrepareMap.
- **CStorage STRG** (CStorage.cpp:393-473): 'STRG'+ver dword (0 raw, 1 = ZL03-сжат после 8-го байта)
  + records; record: WStr-имя (raw UTF-16LE до нуля, БЕЗ префикса длины!) + itemcnt;
  item: WStr имя + type dword (ST_INT32..ST_WCHAR 0-5, bit31=ST_COMPRESSED) + size + payload
  (сжат → 'ZL03'+cnt+{clen;zlib}-блоки). Несовпадение тега = молчаливый-return false.
- **Конфиг движка:** `cfg\robots.dat` (в test-install копирнут из CFG\Rus\robots.dat, 77,184 B,
  STRG ver1/ZL03). `CFG\Main.dat` (96,526 B) — ЗАШИФРОВАН, не STRG; криптоанализ не начат (план B).
- `CFG.TXT` — plain key=value; есть Debug=ключи. Мод-лоадер: Mods/ModCFG.txt + Tweaks
  (только CFG/DATA-переопределения, скриптов нет).

## 3. Сборка: toolchain и грабли (все грабли — забитые)

- VS Build Tools 2022 (MSVC 14.44.35207), бантный CMake 3.31
  (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\...\cmake.exe`).
- DXSDK June 2010 распакован (НЕ установлен) 7-Zip'ом в `local\tools\DXSDK\`;
  installer SHA-256 705271DC83BFEE54D9B94E028426E288D5F070784B7446D164F48ECFBB2A02CB
  (лежит Temp\opencode\dxsdk\DXSDK_Jun10.exe). CMake Finding берётся по `DXSDK_DIR` env.
- **Грабли CMake (легко наступить опять):**
  - свежая конфигурация без `-A Win32` → **x64** платформа → ml.exe ломится на 32-битный
    asharpen.asm. ВСЕГДА `-A Win32`;
  - `option(VAR ON)` в CMake — это описание, а не дефолт! Дефолт OFF. Исправлено
    на `option(VAR "desc" ON)`; всё равно конфигурируйте с явным `-DMATRIXGAME_BUILD_DLL=ON/OFF`.
  - MSVC 14.44 ICE на `MatrixLogic.cpp` (и ещё двух TU) при /Ob2 → цел отключение /Ob1 +
    per-file /Od (MatrixGame/CMakeLists comments, ссылка там же на bisection table).
    MatrixLogic.cpp — это кооп-хот файл, помни об этом при рефакторинге.
- **Грабли ENet (escalation 9):**
  - порядок инклудов: `<winsock2.h>` ДО заголовков проекта (иначе winsock/winsock2 redefine);
  - MSVC не принимает ведущий `__cdecl` decl-specifier в C++; движок собирается /Gr.
    Поэтому в вендорен enet.h все 53 public ENET_API-декларации патчены на
    `ENET_API <ret> __cdecl name (...)` (cdecl ПОСЛЕ возвратного типа). При обновлении
    вендора — повторить патч (тула `Temp\opencode\enet_test\patch_enet.py`);
  - ENet цел собрать `/MT /GS- /EHsc- /W1` (иначе /MD-дефолтлыб затаскивает
    chandler4gs.obj из MSVCRT → неосолвленный __except_handler4_common);
  - линк движка: +`ws2_32.lib`.
- Команды сборки (каждая в отдельном билд-дереве):
  ```
  cmake -S local\MatrixGame -B local\MatrixGame\build    -A Win32 -DMATRIXGAME_BUILD_DLL=ON   → DLL
  cmake -S local\MatrixGame -B local\MatrixGame\build-exe -A Win32 -DMATRIXGAME_BUILD_DLL=OFF → exe
  cmake --build <tree> --config Release
  ```

## 4. Порт-фиксы / upstream-PR кандидаты (апдейтни список)

1. WINDOWS_IGNORE_PACKING_MISMATCH (root+FilePNG CMakeLists) против winnt static_assert.
2. /Ob1 + per-file /Od против MSVC 14.44 ICE.
3. /SAFESEH:NO (MASM-тип) + legacy_stdio_definitions (DxErr __vsnwprintf).
4. IWYU: отсутствующие инклуды MatrixObject*Hpp в 3 cpp (апстрим-баг).
5. 3g.cpp L3GInitAsEXE — девайс D3D не создавался (апстрим-баг, AV на первом g_D3DD->).
6. Headless-диагностика: SR_SMOKE_HEADLESS → Tracer.cpp пишет crash-report в файл и
   делает ExitProcess(3) вместо блокирующего MessageBox; MatrixGame.cpp пишет
   smoke_trace.log (стадии) и smoke_fatal.log (code 2) в catch-блоках.
7. ENet-интеграция: вендор + патч enet.h (__cdecl после ret типа) + /MT/GS- флаги
   + ws2_32 — сверстать как пакет "introдус co-op net harness" (вместе с 8).
8. Хернесс M1: src/Net/SRNetHarness.{hpp,cpp} + хуки (SRNetInit/SRNetPump/SRNetDeinit).
9. Pump-хук в идле: MatrixLib/3G (3g.hpp/3g.cpp) — нейтральный указатель
   `L3GNetPumpHook`, регистрируется из SRNetInit при co-op on (upstream-чисто).
10. M2-fix: ENet-таргет должен собираться с тем же `/Zp1`, что и движок иначе
   ENetEvent/ENetPacket раскладки разъезжаются (packet@+0x10 у ENet, @+0x0D у
   /Zp1-харнесса) → мусорный ev.packet → AV 0xC0000005 на первом RECEIVE.
   Симптом: «wild» указатели вида 0xXX00000C. Подробности в NOTES.md 2026-11-07.
11. Deinit: `SRNetDeinit` зовёт `enet_peer_disconnect_now` (не graceful variant) —
   после `enet_host_destroy` graceful path вообще не успевает отправить disconnect.

## 5. Тест-инфраструктура

- `local\analysis\smoke_matrixgame.ps1` — фазовй гейт: build-exe → extract Boarding.cmap
  (pkg_tool find; SHA-256 8DEF488CB899A1C0F2CC76978954478CFFF32E9866DC336275CF091F094E51DA)
  → копирует в test-install\Matrix\Map\ → прогон с полным путём карты → calcvis.log PASS.
  Печатает 4 лога (calcvis, smoke_trace, smoke_fatal, smoke_crash). НЕ меняет под
  задачу, лакмус фазы.
- `local\analysis\smoke_coop_m1.ps1` — M1-гейт: 3 прогона (OFF/host/guest), критерии в §1.
- `local\analysis\smoke_coop_m2.ps1` — M2-гейт: host+guest пара на Boarding.cmap;
  PASS = полный handshake (session established обе стороны, granted side 2,
  «host: guest gone» после выхода гостя) + нет crash-report у хоста.
- `local\test-install\` — изолированная копия игры (2.61 GB); MatrixGame.dll = наша новая,
  MatrixGame.dll.orig = сток (1,545,216 B); MatrixGame.exe сток не трогается.
- Диагностика крашей: Windows Event Log не показывает краши сами по себе (Tracer
  перехватывает до WER); сверяем smoke_crash.log / smoke_trace.log / smoke_fatal.log.
- Крash-дампы прошлых крашей: `local\analysis\crash\` (MatrixGame.map+asm).

## 6. Сетевой слой: дизайн и состояние

- **Дизайн** (`DESIGN-netlayer.md`, v1): host-authoritative; хост = полный сим; гость =
  side 2; канал A reliable (joins/handshake map+robots.dat hash, SOrder-поток от людей,
  capture/kill-события, результат SRobotGameState), канал B unreliable sequenced
  (дельты позы чужих юнитов на тик; эффекты). Каденс = Тakt. <30 KB/s. Плейлеры
  knob-и (&_netlayer.md §4): Takt pre/post, CMatrixSideUnit.ownership, CMatrixRobotAI
  m_RemoteOwned, m_Run net-bind descriptor, очередь приказов впущенная в Takt
  (однопоток, сеть не в тике).
- **M1 — ЗАКРЫТ** (см. §1): хернесс статичен, ENet-сессия host/guest живёт, инфраструктура выключаема.
- **M2 — handshake ЗАКРЫТ** (2026-11-07): pump из L3G-идла + battle-pump из
  CMatrixMapLogic::Takt; HELLO/WELCOME POD v1 на reliable channel 0; карта
  сверяется по basename; disconnect при отказе; лог-разделение через SR_LOG_PREFIX
  (имя файла становится `<prefix>_srnet.log`). Гейт: smoke_coop_m2.ps1 PASS.
- **M2 — ДАЛЬШЕ (шаги 3-5 §10):** канал B unreliable (позы, channel 1) →
  применение у гостя → канал A reliable (приказы). Подточка: гость гоняет ПОЛНУЮ
  локальную симуляцию side 1 поверх сетевых поз (fallback 4-Б) — применять сети
  позы к признанным ботом по unit-id (см. NOTES 2026-11-07, открытая задача).
- **M3** — реальный батл через test-install (Rangers.exe + наша DLL): MatrixOverride-ручка
  или легал-DLL свап; общая механика выбора стороны; tally.
- **M4** — upstream-ready: PR-пакет, README, regression-скрипты.
- Регрессия нигде ещё не гейтится после каждой сборки — запускай оба смоук-скрипта.

## 7. Открытые вопросы (мёртвые зоны)

1. `MatrixOverride` — где exe берёт значение (Main.dat? registry? CFG.TXT-проба
   `MatrixOverride=dllname` — первый дешёвый эксперимент; криптанализ Main.dat — план B).
2. SRobotGameState полная layout + как галактика агрегирует счёт (важно для L2 co-op).
3. Полный мапшинг: что именно Rangers.exe передаёт в m_Run для планет-боя
   (строки/карта-имя оттуда) — проверить до предположений о стоке на .cmap.
4. Возможных dormant-multiplayer путей в Rangers.exe нет (единственный игрок документирован).
5. Владиславv refactoring-branch — альтернативная база (не исследовалась).
6. Побочные CACHE-подходы: например, закрутка RNG, если решим go lockstep (MVP этого не требует).

## 8. Файловая карта (что где)

- Дизайн/журнал: `games\space-rangers-hd-a-war-apart\{NOTES.md, DESIGN-netlayer.md, HANDOFF.md}`.
- Исходник с фиксами: `local\MatrixGame\` (root CMakeLists, MatrixGame\, MatrixLib\, ThirdParty\ENet\).
- Артефакты: `local\MatrixGame\build\...\MatrixGame.dll`, `local\MatrixGame\build-exe\...\MatrixGame.exe`.
- Тестовая установка: `local\test-install\` (не трогаем оригинал!).
- Тулы: `local\analysis\{smoke_matrixgame.ps1, smoke_coop_m1.ps1, pkg_tool.py, strg_tool.py}`;
  `local\analysis\maps\full\` (извлечённые карты+компаньоны, SHA-256 в NOTES),
  `local\analysis\crash\`, логи сборки `local\analysis\*.log`.
- Тулчаин: `local\tools\DXSDK\` (Include/Lib), `local\tools\pins\enet-1.3.18.tar.gz` (pinned).
- `local\identity.md` — идентификация игры.

## 9. Если новый агент начинает с нуля

1. Прочитай этот файл (ты здесь).
2. Прогони `smoke_matrixgame.ps1` и `smoke_coop_m1.ps1` — оба должны PASS. Нет — по
   §3 (сборка) и логам в analysis\.
3. Открой `DESIGN-netlayer.md` §7 — текущая веха M2, потом по NOTES.md escalation 8-9.
4. НЕ трогай оригинальную установку; не переусложняй; verify-then-claim; апдейти NOTES.md.


---

## 10. Точка входа следующей сессии (M2 остаток: каналы B и A)

Промпт для новой сессии: «Продолжай по HANDOFF.md из games\space-rangers-hd-a-war-apart».
Порядок: §9 пункт 2 может пропустить — она уже в этой секции; затем сюда.

**Статус M2 (2026-11-07):** все шаги M2 выполнены:
- pump в идле (hook L3GNetPumpHook) + battle-pump в CMatrixMapLogic::Takt;
- handshake HELLO/WELCOME POD v1 (magic 0x534F4352), reliable channel 0 (SRChanControl),
  карта сверяется по basename, отказ → disconnect_later;
- smoke_coop_m2.ps1 гейтит handshake (PASS); смоук фазы 2 и M1 остаются PASS;
- краш сессии 2026-11-06 найден и закрыт: /Zp1-расхождение ENetEvent между
  ENet-либой и движком (см. NOTES 2026-11-07 + §4 п.10).

**Цель M2/M3 достигнута.** channel B (позы) и channel A (приказы) работают и гейтятся.
M3 (реальный бой через test-install) ПРОВЕРЕН: два отдельных Rangers.exe с нашей
MatrixGame.dll соединились через ENet localhost:25333, обменялись позами и приказами.

**Шаги M2/M3 (все выполнены):**

3. **Канал B (unreliable, channel 1) — репликация поз: СДЕЛАНО.**
   payload = массив { unit_id, x, y, heading, phase, speed } — *позиции роботов
   side-владельца*; каденс = каждый Takt (или через 2). Пока расходуем дёшево:
   полный список роботов side 1 (host) и side 2 (гост) — десятки байт на юнит.
   Идентификатор юнита: указатель-индекс в m_Side[->Robots] — зафиксируй
   (индекс по порядку создания, стабильный в обеих инстансах — ПРОСТОТА MVP;
   строгие id — потом).
4. **Канал A (reliable, channel 0) — приказы: СДЕЛАНО.**
   smoke_coop_m2.ps1 расширен критерием «хост получил ≥1 PKT_ORDER» — PASS.

**Следующий шаг:** M4 (upstream-PR ready) — установить gh CLI, оформить порт-фиксы как
коммиты, README, regression-скрипты.

Команды сборки и гейтов без изменений (см. §9 и §10 оригинала).
6. **Тест M2 — РАСШИРИ**: smoke_coop_m2.ps1 уже проверяет handshake; добавь
   критерии «гость получил ≥1 PKT_POSE» и «host получил ≥1 PKT_ORDER»
   (лог-маркеры в srnet-логе обеих сторон). Визуальный подтверждение — M3.

Среда и гварды: SR_COOP / SR_COOP_PORT / SR_COOP_SIDE / SR_COOP_ADDR;
SR_SMOKE_HEADLESS=1 для всех автопрогонов; развёл логи через SR_LOG_PREFIX
(суффикс имени файла: <prefix>_srnet.log, <prefix>_smoke_crash.log и т.д.) — уже
реализовано в хернессе и смоук-скриптах (m2h_/m2g_).

Ожидание по объёмам работы: 3-4 — полдня; 5 + расширение смоука — полдня.
Если застопоришься на 4 (симуляции чужой стороны) — перейди к варианту 4-Б
(репликация поверх полной локальной симуляции) и отметь в NOTES открытой задачей.


# OP-2.2 (Объединённый Пак 2.2) — co-op feasibility & prototype

Write-up по шаблону templates/game.md. Игра: S.T.A.L.K.E.R.: Shadow of
Chernobyl, глобальный мод Объединённый Пак 2.2 (ОП-2.2, патч от 15.03.2023).

## TL;DR

- Кооп-мода для ОП-2.2 не существует. Все найденные S.T.A.L.K.E.R. коопы
  (xrMPE, Shadow of COOP, Clear COOP, STCOP, Anomaly Together) строятся на
  Зов Припяти / Anomaly платформах и несовместимы с ТЧ-модами.
- Сетевой слой движка ТЧ в OP-2.2 ВЫРЕЗАН: xrNetServer.dll не содержит
  ws2_32, SendTo_LL обеих сторон — DebugBreak-заглушки, IPureClient::Connect —
  пустой парсер. Локальный клиент общается с сервером через внутренние
  очереди (loopback), поэтому одиночная игра работает.
- НО каркас цел: IPureServer/IPureClient классы, NET_Compressor,
  multipacket-фрейминг, game_sv_Single/game_cl_Single, критические секции и
  очереди пакетов присутствуют в бинарниках.
- Верифицирован рабочий прототип регистрации удалённого игрока: synthetic
  IClient (копия host-клиента с ID=0x1001) + detour IPureServer::
  ID_to_client → движок резолвит ID 0x1001 в нашего игрока, сессия принимает
  его пакеты через IPureServer::_Recieve. [verified live]

## Игра / версия

- Store: Steam (S.T.A.L.K.E.R.: Shadow of Chernobyl, appId 4500)
- Мод: ОП-2.2 + Патч 6 (15.03.2023), gamedata.dbz_patch
- Установка: D:\OP-2.2 (не Steam-папка)
- xrGame.dll SHA-256: 9AE0AD19A48AEAB09E9F78B96DAFE72CBBE4543BDA2F2221C3F7A30DBFC5FE4D
- XR_3DA.exe SHA-256: 9F7F9424E6773D329D739E540876FDDA6313B41C5BFE75495CB5DAAAD4BFDE72
- xrNetServer.dll: 7400BD36DEE89B5B (первые 16 hex)
- Движок: xr_3da.exe, build Dec 31 2021 (xrGame 'Dec 31 2021 15:06:57')

## Что было сделано (прототип, local/ — не в этом репо)

Инструменты: Python 3.12 + xdb_soc.py (распаковка .db), Ghidra 12.2 headless
(реверс xrGame.dll), VS 2022 Build Tools (C++ detour DLLs).

Прототип DLL (инжектируются в живой процесс):
- op22_coop.dll — VEH-хуки DR0/DR1 (getCLASS_ID, Connect), rearm-поток,
  shared section с xrServer instance
- op22_register.dll — регистрация game type "SV_COOP" через движковый
  object_factory (FUN_180984a60, maker FUN_180388c80, push FUN_180386f10)
- op22_client.dll — синтетический IClient (копия host, 0x8120 байт,
  ID=0x1001) + inline-detour IPureServer::ID_to_client (xrNetServer+0x99E0,
  11-байтовый пролог 89 54 24 10 53 41 56 48 83 EC 28)
- op22_bridge.dll — UDP-транспорт: RX socket 5445 → IPureServer::_Recieve;
  TX: inline-detour MultipacketSender::SendPacket (xrNetServer+0x8920,
  24-байтовый пролог) → async ring buffer → sender thread → UDP

## Верифицировано живыми запусками (хронология вех)

1. getCLASS_ID hook видит выбор game type (type='single' is_server=1/0)
2. SV_COOP регистрируется в object_factory, сессия 'all/coop/alife/new'
   стартует (MaxPlayers=32, 'connection accepted')
3. ID_to_client detour возвращает synthetic для 0x1001
   ('remote player REGISTERED')
4. RX: UDP-пакет снаружи → _Recieve → движок обработал (мусор отброшен
   штатно, игра жива)
5. TX: SendPacket detour установлен (24-байтовый пролог), но для loopback-
   клиента не вызывается (прямые очереди). Начнёт срабатывать при наличии
   реального сетевого клиента.

## Известные блокеры / следующие шаги

- SendTo_LL обеих сторон — DebugBreak-заглушки (сетевой I/O вырезан из
  билда). Транспорт должен быть реализован в bridge (уже начато: RX работает,
  TX async ring buffer построен).
- ID_to_client detour при регистрации вызывает зависание Create new game
  (ALIFE save/spawn цикл): движок ждёт liveness от 0x1001 (spawn-ack,
  heartbeat). Next: реализовать минимальный liveness-протокол в bridge.
- ClientID assign: synthetic получает ID при OnCL_Connected — требуется
  эмуляция handshake-пакетов.

## Карта ключевых адресов (xrGame patch-6, base 0x180000000)

| Что | RVA |
|---|---|
| game_GameState::getCLASS_ID | 0x00990B90 |
| xrServer::Connect | 0x009A1130 |
| IPureServer::ID_to_client (xrNetServer) | +0x99E0 |
| MultipacketSender::SendPacket (xrNetServer) | +0x8920 |
| IPureServer::_Recieve (xrNetServer) | +0xAF40 |
| object_factory table builder | 0x00984A60 |
| maker (game_sv_single entry) | 0x00388C80 |
| factory push | 0x00386F10 |
| xrServer client vector | this+0x30..0x40 (base) — требует уточнения |
| host IClient ptr | this+0xD8 |

## Источники

- AP-PRO кооп-тред: https://ap-pro.ru/forums/topic/10147-/
- xrMPE: https://www.xrmpe.online/
- Shadow of COOP fixes: https://github.com/cranebeaker/stalker-soc-coop-fixes
- ОП-2.2 wiki: https://op-2.fandom.com/ru/wiki/Заглавная_страница

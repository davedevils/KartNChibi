# Database work and the io thread

The game server runs one io thread. `GameServer::run` calls `m_ioContext.run()` once, there is
no thread pool behind it and no strand. Every handler therefore runs on that one thread, and a
query that takes 200 ms freezes every other client for 200 ms: no motion relay, no chat, no
accept. The liveness stamp added with the accept loop fix makes the stall visible from outside,
it does not remove it.

## The pool

`shared/src/include/db/DbWorkerPool.h` is a small worker pool. `GameServer::postDb(orderKey,
job)` queues work on it and `GameServer::postIo(job)` hands the answer back to the loop.

Two rules, both load bearing.

1. **Order.** Jobs that share an order key never run at the same time and always run in the
   order they were posted. The order key is the session id, so the frames one client gets back
   keep the order that client sent. Jobs of different keys run in parallel up to the thread
   count, four by default, `KNC_DB_THREADS` overrides it and 0 keeps everything on the io
   thread for a bisect.
2. **Nothing but the database on a worker.** `Session::send` pushes onto a write queue with no
   lock, and `Room` is guarded by the server mutexes the loop already holds. A job copies what
   it needs by value, does its queries, builds the packets, and sends them from a `postIo`
   lambda. A job that touches a `Session` or a `Room` directly is a data race.

`tests/server/test_db_worker_pool.cpp` covers the ordering, the parallelism across keys, a job
that throws, the refusal when the pool is off, and the one that matters: a 400 ms job does not
stop the accept loop.

## Moved off the loop

| handler | opcode | why |
|---|---|---|
| `GhostHandler::handleSubmit` | C2S 0x00F7 | writes the record, every replay chunk of up to 1171 frames, exports the replay file, then reads the board back. The slowest single handler in the server. Parses and reads the upload state on the loop, does the rest on the pool, answers with `postIo`. |
| `GameServer::sendPlayerData` | the login burst | the whole catalogue set, 733 frames on the recording. Split in three, see below. |

## The login burst

`GameServerLoginBurst.cpp` is now three functions and each one has one job.

| part | thread | what it does |
|---|---|---|
| `sendPlayerData` | io | one query, the character row, then every session write: `characterId`, `characterName`, `clanTag`, `gmLevel`, `mutedUntil`, `inviteOptOut`, `catalogsSent`. Then it posts the rest. |
| `collectLoginBurst` | pool | every other read and write, the catalogues, the owned containers, the room member blob, the gifts. Touches no `Session` and no `Room`, it fills a `LoginBurstFrames`. |
| `dripLoginBurst` | io | sends the lead frames, then hands the chunks to the `SpacedSender` and arms the screen ack. |

`LoginBurstFrames` carries two lists on purpose. `preBurst` holds the shop catalogue reset, the
price table and the room craft frames, which the old code sent one by one before the drip, so
they are still sent one by one and still lead. `chunks` is the sorted burst cut for the spaced
sender. The wire order of `docs/packets/LOGIN_ORDER.md` is unchanged, only the thread changed.

`ShopHandler::sendLoginCatalogs`, `RoomCraftHandler::pushObjectCatalog` and
`pushOwnedInstances` take an optional `std::vector<Packet>* out`. With it they collect instead
of sending, which is what makes them legal on a worker.
`InventoryHandler::buildLoginBurst` takes a character id now, not a session.

### The numbers

Measured read only against the live database, the 18 statements the burst runs for one
character against the single statement the io phase keeps.

| what | statements | time |
|---|---|---|
| the burst reads, warm | 18 | 4.0 to 5.5 ms |
| the burst reads, first run after the container idled | 18 | 289 ms |
| the io phase after the split, the character row | 1 | 0.5 to 0.8 ms |

Warm it is small, cold it is the stall everyone felt, and the cold case is the one that used to
freeze every other client. The server logs both halves on every login so the live split is on
the record:

```
login burst io phase 0.62 ms account 7
login burst pool 4.81 ms io drip 1.90 ms 39940 bytes in 12 chunks for hltest
```

The frame building and the serialisation ride with the pool half. `KNC_DB_THREADS=0` turns the
pool off and then `postDb` refuses and the whole burst runs on the loop exactly as before,
which is how a regression is bisected.

`tests/server/test_login_burst_offload.cpp` covers the three parts: the accept loop keeps taking
clients while a 400 ms burst is being built, the lead frames stay in front of the chunks, and the
burst still runs with no pool at all.

## Still on the loop, in the order they should move

The count is the number of `Database` calls in the file, not the number per request.

| file | calls | what blocks | cost |
|---|---|---|---|
| `InventoryHandler.cpp` | 64 | owned lists, equip, install, remove | mostly small reads, the equip path writes under no lock |
| `SocialHandler.cpp` | 59 | friends, block list, messenger, invites | reads are small, several of them fan out to other sessions and must stay split |
| `ShopHandler.cpp` | 42 | buy, sell, gift, extend | one transaction per buy with a `FOR UPDATE` row lock, this is the second best candidate after the burst because it is a pure request and answer |
| `GarageHandler.cpp` | 29 | the garage lists and the equip verbs | same shape as the inventory |
| `MissionHandler.cpp` | 28 | definitions, progress, rewards | the definition list is sent once per session and is a good pool job |
| `GachaHandler.cpp` | 20 | rolls and the owned pet list | a roll is a transaction |
| `ScenarioHandler.cpp` | 19 | stage progress | |
| `CharCreateHandler.cpp` | 19 | creation, one transaction with many inserts | rare but very long, and it is the one path where the client is already waiting on MSG_WAIT |
| `LicenseHandler.cpp` | 18 | licence progress and rewards | |
| `RaceHandler.cpp` | 12 | results, ghost best, reward rules | runs at the end of a race while everyone is on the podium |

The login burst tail has not moved yet. `SpacedSender::onDone` still runs
`SocialHandler::pushSocialLists`, `CarCraftHandler::sendLoginSnapshot` and
`QuestHandler::onCharacterEnter` on the loop, after the burst is on the wire. Those three are
the next piece of the same job.

Two things never move. The race tick and the motion relay hold `m_raceMutex` and touch no
database. The anti cheat violation insert is a fire and forget write on the kick path, and the
kick has already stopped the session.

## How to move one

```cpp
const uint32_t charId = session->characterId;      // copy what the job needs
auto work = [session, server, charId, req]() {
    ... queries, build the packets ...
    Packet reply = ...;
    auto deliver = [session, reply] { session->send(reply); };
    if (server) server->postIo(deliver); else deliver();
};
if (!server || !server->postDb(session->id(), work)) work();   // pool off, do it inline
```

The inline fallback is not optional. `postDb` returns false when the pool is not running, and
`KNC_DB_THREADS=0` is how a stall is bisected, so every offloaded handler must still work with
no pool at all.

# Distributed Rate Limiter (C++ / Redis / Lua)

A rate limiter whose state lives in Redis instead of process memory, so several service instances enforce **one shared limit**. The check-and-update runs inside a Redis Lua script, which makes it atomic: two instances can never both be admitted by the same last token.

Built with C++17, [redis-plus-plus](https://github.com/sewenew/redis-plus-plus) (on top of hiredis), Lua, Docker Compose and CMake.

## The problem

With in-memory counters, every instance keeps its own count. Run 3 copies of a service with a limit of 100 requests, and up to 300 get through. Moving the counter into a shared store fixes that, but a naive `GET` → compare → `SET` sequence has a race condition: two instances can read the same value at the same moment and both decide to allow the request.

## How it works

```
 app-1 ─┐
 app-2 ─┼──► Redis (one shared bucket per key, updated by a Lua script)
 app-3 ─┘
```

- Each instance loads a Lua script into Redis once (`SCRIPT LOAD`) and then calls it per request by its SHA (`EVALSHA`).
- Redis executes a script as a single uninterrupted unit, so the read, the decision and the write happen atomically.
- Time comes from Redis (`TIME`), not from the application, so instances with different clocks cannot disagree.

### Algorithms

| File | Algorithm | State in Redis |
|---|---|---|
| `token_bucket.lua` | Token bucket: holds up to `capacity` tokens, refills at a steady rate, each request costs tokens. Allows bursts without a hard reset moment. | Hash with `tokens` and `ts` |
| `fixed_window.lua` | Fixed window: counts requests per time window (`INCR` + `EXPIRE`). Simple, but allows a burst across a window boundary. | A counter with a TTL |

`token_bucket.lua` arguments: `KEYS[1]` = bucket key, `ARGV` = capacity, refill rate (tokens/second), tokens per request. The script returns `1` for allowed and `0` for rejected.

## Run it

Requirements: Docker with Docker Compose.

```bash
docker compose down                              # start from an empty Redis
docker compose up --build --scale app=3
```

This starts one Redis container and 3 copies of the app. Each copy loads the script, waits for the same 10-second wall-clock boundary so all three start together, and then sends 20,000 requests for the same key. Each prints how many it was allowed and rejected.

To add up the results after the containers exit (before running `docker compose down`):

```bash
docker compose logs app | grep Allowed
```

`main.cpp` is a test harness, not the limiter itself. The limiter is the Lua script plus the `evalsha` call. To try the fixed window, load `fixed_window.lua` in `main.cpp` and change the arguments of the `evalsha` call to its limit and window.

## Test result

Setup: 3 containers, 20,000 requests each (60,000 total), all on the same key, token bucket with capacity 100 and refill 0.1 token/second.

| Run | app-1 | app-2 | app-3 | Total allowed |
|---|---|---|---|---|
| 1 | 33 | 33 | 34 | **100** |
| 2 | 32 | 36 | 32 | **100** |

Across repeated runs the total was exactly the capacity, and the split between containers changed each time. The varying split shows that the containers were competing, and the exact total shows that nothing got through beyond the limit. The earlier fixed-window version gave the same result.

The test lasts well under a few seconds, so the refill contributes at most a request or two at 0.1 token/second. The upper bound for any run is `capacity + rate × seconds`.

## Limitations and next steps

- **Single Redis node.** It is both a throughput ceiling and a single point of failure. Each script touches one key, so keys could be sharded across Redis Cluster nodes, but that is not built or tested.
- **No failure handling yet.** If Redis restarts, `EVALSHA` fails with `NOSCRIPT` and the program stops instead of reloading the script. There are no timeouts, and no fail-open / fail-closed policy for when Redis is down.
- **Hard-coded parameters.** Key, capacity, refill rate and script name are set in `main.cpp`.
- **Not measured.** There are no throughput or latency numbers yet.
- **Not yet integrated** into my in-memory C++ rate limiter engine as a pluggable storage backend.

Planned: reload on `NOSCRIPT`, socket timeouts and a fail-open / fail-closed policy, configuration through environment variables, a sliding-window script, and a Redis-backed store behind the same interface as the in-memory limiter.
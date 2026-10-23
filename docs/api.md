# HTTP API

Base path: `/api/v1`. Request and response bodies use JSON.
Authorization: `Authorization: Bearer <authToken>`.

| Method | Path | Purpose |
| --- | --- | --- |
| GET | `/maps` | List maps: `id`, `name` |
| GET, HEAD | `/maps/{id}` | Roads, buildings, offices, and item types |
| POST | `/game/join` | Join a game; returns `authToken` and `playerId` |
| GET, HEAD | `/game/players` | Players in the current session |
| GET, HEAD | `/game/state` | Position, velocity, direction, bag, score, and items |
| POST | `/game/player/action` | Change direction |
| POST | `/game/tick` | Advance manual game time |
| GET | `/game/records?start=0&maxItems=100` | Completed player games |

Joining a game:

```json
{"mapId": "map1", "userName": "Courier"}
```

The name must be nonempty and must not contain a null character. An invalid name
returns 400 `invalidArgument` with the message `Invalid name`; no player is created.

Movement: `{"move":"R"}`. Directions: `U`, `D`, `L`, `R`; an empty string
stops the player. Movement continues until a new command or the edge of a road.
Movement commands require a token and `Content-Type: application/json`.

Manual tick: `{"timeDelta":50}`, with time in milliseconds. This endpoint returns
an error in automatic mode. The value must be an integer from 0 to 2147483647.
An overflow of accumulated player time is also rejected: the response is 400,
and the game state remains unchanged. The browser client does not send manual ticks.

Records are ordered by descending score, then ascending play time and name.
The response reports `playTime` in seconds; the database stores it in milliseconds.
Negative `start` or `maxItems` values and requests for more than 100 records
are rejected with status 400. An empty result set (`maxItems=0`) is allowed.

Main error codes: `invalidArgument`, `invalidMethod`, `invalidToken`,
`unknownToken`, `mapNotFound`, and `badRequest`. A malformed token is distinguished
from a correctly formatted but unknown token. Invalid percent-escape sequences
and null bytes in the URL return 400 `badRequest`.

## Request handling rules

- Map and record lists accept GET only. The records endpoint includes `GET, HEAD`
  in its `Allow` header, but HEAD returns a method error.
- An invalid method for the map list returns the text `Invalid method` with a JSON Content-Type.
- An unsupported Content-Type for movement and tick commands returns 401.
- Pagination parsing accepts a numeric prefix: for example, `start=2abc` sets
  an offset of 2. A value that cannot be parsed leaves the parameter at its default.
- On routes that support HEAD, the handler builds the same body as for GET.

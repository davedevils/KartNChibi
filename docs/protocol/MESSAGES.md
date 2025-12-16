# Server Messages Reference

Messages sent in packet payloads (ASCII, null-terminated).

## Authentication

| Message | Description |
|---------|-------------|
| `MSG_SERVER_NOT_READY` | Server starting up |
| `MSG_DB_ACCESS_FAIL` | Database connection error |
| `MSG_DB_CONNECT_FAIL` | Cannot connect to DB |
| `MSG_DB_EXECUTE_FAIL` | DB query failed |
| `MSG_REINPUT_IDPASS` | Wrong ID or password |
| `MSG_INVALID_ID` | User ID not found |
| `MSG_DUPLOGIN` | Duplicate login |
| `MSG_LIMIT_CONN_PER_IP` | Too many connections from IP |
| `MSG_BANNED_IP` | IP is banned |
| `MSG_BANNED_USER` | User is banned |
| `MSG_SESSION_EXPIRED` | Session timeout |

## Game / Room

| Message | Description |
|---------|-------------|
| `MSG_DURABILITY_ZERO` | Item durability = 0 |
| `MSG_DURABILITY_LOW` | Low durability warning |
| `MSG_MAX_ROOM_USER_8` | 8-player room full |
| `MSG_MAX_ROOM_USER_16` | 16-player room full |
| `MSG_TEAM_CHANGE_FAIL` | Cannot change team |
| `MSG_START_FAIL_MINIMUM` | Not enough players |
| `MSG_START_FAIL_NOT_READY` | Players not ready |
| `MSG_KICK_BY_HOST` | Kicked by host |
| `MSG_ROOM_CLOSED` | Room was closed |
| `MSG_RACE_IN_PROGRESS` | Race already started |

## Shop / Inventory

| Message | Description |
|---------|-------------|
| `MSG_NOT_ENOUGH_COIN` | Insufficient coins |
| `MSG_NOT_ENOUGH_CASH` | Insufficient cash |
| `MSG_INVENTORY_FULL` | No inventory space |
| `MSG_ITEM_EXPIRED` | Item expired |
| `MSG_PURCHASE_FAIL` | Purchase failed |
| `MSG_EQUIP_FAIL` | Equip failed |

## General

| Message | Description |
|---------|-------------|
| `MSG_UNKNOWN_ERROR` | Generic error |
| `MSG_TIMEOUT` | Operation timed out |
| `MSG_MAINTENANCE` | Server maintenance |
| `MSG_VERSION_MISMATCH` | Client version too old |
| `MSG_REGION_BLOCKED` | Region not allowed |

## Client Handlers

From `HandleLoginResponse` (0x01):

```c
// Client checks these with strstr():
if (strstr(message, "MSG_SERVER_NOT_READY")) { /* error */ }
if (strstr(message, "MSG_DB_ACCESS_FAIL"))   { /* error */ }
if (strstr(message, "MSG_REINPUT_IDPASS"))   { /* wrong creds */ }
if (strstr(message, "MSG_INVALID_ID"))       { /* user not found */ }
```

## Usage in Packets

```c
// Example: Login failed
struct LoginResponse {
    char message[256];  // "MSG_REINPUT_IDPASS"
    int32 serverCode;   // 1
};
```

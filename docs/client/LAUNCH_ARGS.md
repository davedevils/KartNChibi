# Client Launch Arguments

## Format

```
serviceid=XXX userid=YYY token=ZZZ
```

No dashes. `key=value` pairs separated by spaces.

## From Reverse Engineering

```c
// sub_406740 - WinMain init
sscanf(Buffer, "serviceid=%s userid=%s token=%s", v48, MultiByteStr, v50);
MultiByteToWideChar(0, 0, MultiByteStr, -1, WideCharStr, 512); // userid
MultiByteToWideChar(0, 0, v50, -1, Source, 512);               // token
MultiByteToWideChar(0, 0, v48, -1, v53, 512);                  // serviceid

// stored at:
wcscpy(&word_1ADEA34, WideCharStr); // userid
wcscpy(&word_1ADEC32, Source);      // token
wcscpy(&word_1ADEE30, Source);      // token (duplicated)
```

## Fields

| Field | Desc |
|-------|------|
| serviceid | Service/Platform ID (1 for default) |
| userid | Username/Account name |
| token | Session token from launcher auth |

## Example

```
KnC.exe serviceid=1 userid=test token=session_test
```

## Launcher Code

```cpp
std::string params = "serviceid=1 userid=" + m_username + " token=" + m_sessionToken;
```

## OGPlanet Mode

When args provided:
- Skips username/password input screen
- Uses provided userid/token for auth
- Checks for `TOGPLauncherFrame` window via `FindWindowA`

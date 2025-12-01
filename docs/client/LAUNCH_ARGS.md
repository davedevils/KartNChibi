# KnC Client Launch Arguments

## Format

The client expects command-line arguments in this exact format:

```
serviceid=XXX userid=YYY token=ZZZ
```

**Note:** No dashes, just `key=value` pairs separated by spaces.

## From Reverse Engineering

```c
// sub_406740 - WinMain initialization
sscanf(Buffer, "serviceid=%s userid=%s token=%s", v48, MultiByteStr, v50);
MultiByteToWideChar(0, 0, MultiByteStr, -1, WideCharStr, 512);  // userid
MultiByteToWideChar(0, 0, v50, -1, Source, 512);                 // token
MultiByteToWideChar(0, 0, v48, -1, v53, 512);                    // serviceid

// Stored at:
wcscpy(&word_1ADEA34, WideCharStr);  // userid
wcscpy(&word_1ADEC32, Source);       // token
wcscpy(&word_1ADEE30, Source);       // token (duplicated)
```

## Fields

| Field | Description |
|-------|-------------|
| serviceid | Service/Platform ID (1 for default) |
| userid | Username/Account name |
| token | Session token from launcher auth |

## Example

```
KnC.exe serviceid=1 userid=test token=session_test
```

## Launcher Implementation

```cpp
std::string params = "serviceid=1 userid=" + m_username + " token=" + m_sessionToken;
```

## OGPlanet Mode

When these arguments are provided, the client runs in "OGPlanet mode":
- Skips username/password input screen
- Uses the provided userid/token for authentication
- Checks for `TOGPLauncherFrame` window via `FindWindowA`


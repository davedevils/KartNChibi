# Packet Inspector

**Purpose**: Network packet sniffer and analyzer for KnC protocol debugging

**Type**: DLL Proxy  
**Status**: Production Ready  
**Platform**: Windows (x86)

---

## Overview

The Packet Inspector is a transparent proxy DLL that intercepts Winsock2 API calls to capture, decode, and log all network traffic between the KnC client and server.

**Key Features**:
- Real-time packet capture
- Automatic CMD opcode decoding
- Packet filtering by type
- Export to log files
- Hex dump for analysis
- No client modification required

---

## How It Works

The tool uses **DLL proxying** technique:

```
KnC.exe
  │
  ├─> Loads "WS2_32.dll" (our proxy)
  │   │
  │   ├─> Intercepts send()/recv()
  │   ├─> Logs packet data
  │   └─> Forwards to real WS2_32.dll
  │
  └─> Game continues normally
```

---

## Installation

### 1. Build the Proxy DLL

```bash
cd tools/packet_inspector
cmake -B build -A Win32
cmake --build build --config Release
```

This generates:
- `build/Release/WS2_32.dll` (proxy DLL)

### 2. Deploy to Client

Copy the proxy DLL to your game directory:

```bash
copy build\Release\WS2_32.dll DevClient\
```

### 3. Run Client

Simply launch the game normally:

```bash
cd DevClient
KnC.exe
```

The proxy will automatically intercept all network traffic.

---

## Output

### Log Files

**Location**: `DevClient/logs/`

**Files**:
- `packets.log` - All packets with timestamps
- `packets_send.log` - Outgoing packets only
- `packets_recv.log` - Incoming packets only

### Log Format

```
[2025-12-17 15:30:45] SEND | CMD 0x07 | Size: 128 | Flag: 0x00
00 00 00 80 07 00 00 00  31 37 38 00 04 00 00 00  |........178.....|
75 00 73 00 65 00 72 00  00 00 70 00 61 00 73 00  |u.s.e.r...p.a.s.|
...

[2025-12-17 15:30:46] RECV | CMD 0x01 | Size: 256 | Flag: 0x00
00 00 01 00 01 00 00 00  4D 53 47 5F 4C 4F 47 49  |........MSG_LOGI|
4E 5F 53 55 43 43 45 53  53 00 00 00 00 00 00 00  |N_SUCCESS.......|
...
```

**Fields**:
- **Timestamp**: `[YYYY-MM-DD HH:MM:SS]`
- **Direction**: `SEND` (Client → Server) or `RECV` (Server → Client)
- **CMD**: Packet opcode (e.g. `0x07` = Login Request)
- **Size**: Total packet size in bytes
- **Flag**: Packet flag byte
- **Hex Dump**: Raw packet data (16 bytes per line)

---

## Configuration

**Config File**: `packet_inspector.ini`

```ini
[Logging]
Enabled=1                  # 0=Disable, 1=Enable
LogPath=logs/              # Output directory
LogSend=1                  # Log outgoing packets
LogRecv=1                  # Log incoming packets

[Filtering]
FilterEnabled=0            # 0=Log all, 1=Use filter
FilterCMDs=0x07,0x12,0x31  # Comma-separated CMDs to log

[Output]
Hexdump=1                  # Include hex dump
DecodeStructs=1            # Decode known structures
ShowTimestamp=1            # Include timestamps
```

---

## Common Use Cases

### 1. Capture Login Flow

```bash
# Start fresh capture
del DevClient\logs\*.log

# Launch client and login
cd DevClient
KnC.exe

# Check login packets
type logs\packets.log | findstr "0x07 0x01"
```

### 2. Debug Room Join Issues

Filter for room-related packets (0x12, 0x3E, 0x3F):

```ini
[Filtering]
FilterEnabled=1
FilterCMDs=0x12,0x3E,0x3F
```

### 3. Analyze Race Updates

Filter for position packets (0x31):

```ini
[Filtering]
FilterEnabled=1
FilterCMDs=0x31
```

### 4. Export for Analysis

```bash
# Copy logs for offline analysis
copy DevClient\logs\*.log analysis\capture_2025-12-17\
```

---

## Packet Reference

**See**: [Protocol Documentation](../../docs/protocol/PROTOCOL_OVERVIEW.md)

**Quick Reference**:
- **0x01**: Login Response
- **0x07**: Client Auth / Session Info
- **0x12**: Show Lobby
- **0x31**: Position Update
- **0x3E**: Player Join
- **0xA6**: Heartbeat

Full packet list: [Packet Index](../../docs/packets/README.md)

---

## Troubleshooting

### Proxy not loading

**Problem**: No logs generated  
**Solution**:
1. Verify `WS2_32.dll` is in same directory as `KnC.exe`
2. Check if original `WS2_32.dll` was renamed to `WS2_32_original.dll`
3. Ensure DLL is 32-bit (not 64-bit)

### Game crashes on startup

**Problem**: DLL version mismatch  
**Solution**:
1. Rebuild proxy with correct Windows SDK
2. Verify all exports are forwarded correctly
3. Check `WS2_32.def` export definitions

### No network traffic captured

**Problem**: Proxy loads but no packets logged  
**Solution**:
1. Verify logging is enabled in `packet_inspector.ini`
2. Check write permissions for `logs/` directory
3. Ensure hooks are applied (check debug output)

---

## Advanced Features

### Custom Packet Handlers

Edit `WS2_32.cpp` to add custom handlers:

```cpp
void OnPacketReceived(const uint8_t* data, int size) {
    uint8_t cmd = data[2];  // CMD byte
    
    if (cmd == 0x31) {  // Position update
        // Parse and display position
        float x = *(float*)(data + 8);
        float y = *(float*)(data + 12);
        float z = *(float*)(data + 16);
        
        Log("Position: (%.2f, %.2f, %.2f)", x, y, z);
    }
}
```

### Real-Time Analysis

Use with external tools:

```bash
# Tail logs in real-time
tail -f logs/packets.log

# Filter specific CMD
tail -f logs/packets.log | findstr "0x31"

# Count packets by type
type logs/packets.log | findstr /C:"CMD" | sort | uniq -c
```

---

## Development

### Build from Source

```bash
cd tools/packet_inspector
cmake -B build -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Dependencies

- **Windows SDK** 10.0+
- **MSVC 2019+** (32-bit compiler)
- **CMake 3.15+**

### Project Structure

```
packet_inspector/
├── WS2_32.cpp          # Main proxy implementation
├── WS2_32.def          # Export definitions
├── dinput8.cpp         # DirectInput proxy (optional)
├── version.cpp         # Version proxy (optional)
├── winmm.cpp           # WinMM proxy (optional)
├── app/                # Inspector GUI (optional)
├── widgets/            # UI widgets (optional)
└── CMakeLists.txt      # Build configuration
```

---

## Related Tools

- **[Network Decrypt](../network_decrypt/)** - Decrypt Network2.ini config
- **[Protocol Docs](../../docs/protocol/)** - Complete packet reference
- **[Wireshark](https://www.wireshark.org/)** - Alternative packet capture

---

## License

Same as parent project: CC BY-NC-SA 4.0

---

## Support

- **Issues**: [GitHub Issues](https://github.com/davedevils/KartNChibi/issues)
- **Protocol Questions**: See [Protocol Documentation](../../docs/protocol/)
- **Reverse Engineering**: See [IDA Guide](../../docs/reverse/IDA_GUIDE.md)

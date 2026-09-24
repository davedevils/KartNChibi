# Documentation

Where to look. Root files cover the project as a whole, the folders below go deeper.

| Folder | What is in it |
|---|---|
| `packets/` | The network protocol, reversed from the client. `PACKET_REGISTRY.md` is the opcode table, `PROTOCOL.md` the overview, `WIRE_FORMAT.md` the frame layout, `PROFILE_BLOB.md` the login blob. `systems/` holds one spec per opcode group still open. `*_VERIFIED.md` files are facts checked against the binary |
| `systems/` | What each server system should do and the known gaps, one file per system |
| `server/` | Server layout and how to add content, tracks, items, shop rows |
| `engine/` | The engine, one file per subsystem, `INDEX.md` first |
| `client/` | The stock client, its states, launch arguments and UI states |
| `tools/` | The tools and the UI file format |
| `formats/` | Game file formats, NIF, KF, DDS, PAK |
| `reverse/` | How to work in IDA and Ghidra on this client. `CLIENT_PHYSICS_MAP.md` is the ground for the physics port, `physics/` holds one file per layer, body, world collision, helpers, remote car, constants. `CLIENT_SHADING.md` is the light rig and the fixed function rules the renderer must match. `GOA/` is the other client this protocol was compared with |
| `conventions/` | Coding standards, dependency rules, testing rules |

Rules that apply to every document

- Plain short English. Say the fact, skip the story
- No emoji, no badges, no marketing tone
- A claim about the client carries the address it was read at, `sub 47F800` style
- When the code moves, the doc moves in the same commit

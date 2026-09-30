# PROTOCOL — client ↔ game server (protocol 31)

## Real-client evidence (supplied by the project owner from an earlier independent implementation; treated as authoritative but NOT reproduced by this repository)
* Client reached protocol 31, descriptor sync, SET_GLOBALS and emitted ID_DATA.
* Descriptor tables: 332 classes, 968 properties, 320 events, 182 types.
* Legacy Huffman string variant: a leading zero bit, then the Huffman bit length as a network-order 32-bit value, then the Huffman bits.
* SET_GLOBALS has a 121-bit Workspace preamble before the container count. Correct decode: 22 top-level containers, first class id 231 = ReplicatedFirst. The client then immediately emits several ID_DATA.
* **Open hard problem:** authoritative server→client ID_DATA.

## Derived by BLOXEN (static, verifiable)
* Class id = 0-based index in alphabetical order of the 332-class dump → ReplicatedFirst = 231 (matches the evidence above, which corroborates the ordering).
* Legacy string = bit 0 + u32 big-endian bit length + RakNet Huffman bits, bits MSB-first, integers big-endian. Implemented in `src/gameserver/bitstream.js` / `huffman.js`. Huffman table = `preservation/reference/raknet-english-freq.json` (facebookarchive/RakNet BSD); **identity with the client's 3.x fork is UNVERIFIED**.

## BLOXEN simulator profile (`bloxen-profile-v0`) — INFERRED
Ids and layouts below are BLOXEN's own, used so server, simulator and tests agree. They are **not** claimed to match the real client.
`PROTOCOL_SYNC 0x80`, `SET_GLOBALS 0x81`, `TEACH_DESCRIPTOR_DICTIONARIES 0x82`, `ID_DATA 0x83`, `PROTOCOL_MISMATCH 0x85`, `JOIN_REFUSED 0x86`; ID_DATA ops NEW 1, PROP 2, DEL 3, CLIENT_READY 0x10, MOVE 0x11, RESET 0x12, LOCAL_PLAYER 0x21.
Transport `raknet-framing-v0`: reliability/ordering layer written from general RakNet knowledge; wire compatibility with 3.x is UNVERIFIED.

## Hardening (UNIT-TESTED)
Join token single-use (consumed by the game server, not by Join.ashx peek), flood limit 400 pkt/s, 5 malformed packets → kick, max 12 players, non-loopback bind refused by default, server scripts never replicated/executed.

## Test status labels
| Layer | Label |
|---|---|
| Bitstream, Huffman, framing, descriptor/dictionary coding, world replication | UNIT-TESTED |
| Simulator client ↔ server join, spawn, move, reject replay | SIMULATOR-TESTED |
| Anything involving RobloxPlayerBeta.exe | **NOT REAL-CLIENT-TESTED** |

## Next protocol work (needs a real client capture on Windows, see validation doc)
1. Capture the client's first ID_DATA batch and property-id assignment to replace INFERRED ids.
2. Determine authoritative server→client ID_DATA (instance/property encoding).
3. Check the Huffman table against the client's.


## Frozen gate
**AUTHORITATIVE SERVER→CLIENT ID_DATA VALIDATION = BLOCKED: REAL CLIENT CAPTURE REQUIRED.** BLOXEN does not guess the server→client ID_DATA encoding. `bloxen-profile-v0` (ids above) is INFERRED, round-trips only against BLOXEN's own simulator (`src/sim/client.js`), and is labelled so in every report. Packet logging (game-server logger, compat log) and the simulator are retained so a real capture can be compared against them as soon as the Windows gate has been run.

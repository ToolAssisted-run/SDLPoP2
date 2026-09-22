# Level data (PoP2)

Levels live in PRINCE.DAT as untyped resources: ids 2000..2013 (14 levels) and 2020..2033 (14 more,
purpose to be confirmed), each 12025 bytes (+1 checksum byte). The game reads a level with one DOS read
straight into DGROUP at DS:2BB8 (verified byte-identical against the running game), so the file bytes
ARE the in-memory level struct:

| DS offset | struct offset | content (from code access patterns) |
|---|---|---|
| 2B9A | -0x1E | tiles[room 0] (dummy room, zeros, not in the file) |
| 2BB8 | 0x0000 | tiles[room][30]: one byte per tile, 10 cols x 3 rows, rooms 1..; PoP1 tile ids (0 empty, 1 floor, 20 wall) |
| 2F00 | 0x0348 | tile_attrs[room][30]: one dword per tile (bits 0xC000 tested by 0CD6:027A; low bits = modifier) |
| 43AB | 0x17F3 | room records, 0x74 bytes each, indexed by room: +0 = number of characters starting in the room |
| 43FF | 0x1847 | level number byte (compared against 5, 6 in play_seq specials) |
| 441E | 0x1866 | ? (42 references) |
| 5AB2 | 0x2EFA | ? (last dword, 69 references) |
| 5AB6 | end | Char record follows immediately |

tools/leveldump.py (to write) will decode a level file into rooms and print tile maps.

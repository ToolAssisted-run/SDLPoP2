# Level data (PoP2)

Levels live in PRINCE.DAT as untyped resources: ids 2000..2013 (14 levels) and 2020..2033 (14 more,
purpose to be confirmed), each 12025 bytes (+1 checksum byte). The game reads a level with one DOS read
straight into DGROUP at DS:2BB8 (verified byte-identical against the running game), so the file bytes
ARE the in-memory level struct:

| DS offset | struct offset | content (from code access patterns) |
|---|---|---|
| 2B9A | -0x1E | tiles[room 0] (dummy room, zeros, not in the file) |
| 2BB8 | 0x0000 | tiles[room][30]: one byte per tile, 10 cols x 3 rows, rooms 1..; PoP1 tile ids (0 empty, 1 floor, 20 wall) |
| 2F00 | 0x0348 | tile_attrs[room][30]: one dword per tile, room 0 included (room r at 0x348 + r*0x78); bits 0xC000 tested by 0CD6:027A |
| 3C98 | 0x10E0 | 1811 bytes, all zero in level 1 (door links?) |
| 43AB | 0x17F3 | level header (0x74 bytes, occupies the "room 0" slot): +4D room count (19 in level 1), +54 level number, +6D start room, +6E start tile (col + 10*row), +6F start direction (-1 = the prince faces right after ~), +73 level type/flag |
| 441F | 0x1867 | room records for rooms 1..: 0x74 = 1 + 5*23 bytes: +0 number of characters, then five 23-byte character init records (read by the room-entry routine OVL01::02D444 through OVL01::02DC8C(i, room)): +0 start tile (col + 10*row), +1 x (word), +3 direction, +5 word (100 = use +11 as y for charid 2; 0x0C sets fall 2/18 for charid 6), +7 word (0 = ?), +0xD, +0xE bytes -> DS:5AEE/5AEF, +0xF type (charid = DS:0096[type]; overridden by the level type byte 441E unless 1/3/9/10), +0x11 word (y for charid 2), +0x15 word (y for charid 6, if nonzero) |
| 50CF | 0x2517 | 2530 bytes, zero in level 1 |
| 5AB2 | 0x2EFA | last dword: a resource handle set at runtime (FUN_194c_19dc), not level data |
| 5AB6 | end | Char record follows immediately |

tools/leveldump.py decodes a level file (PRINCE.DAT untyped resource body) and prints tile maps and
the character init records. Tile ids are PoP1's in the low 5 bits (room 4 of level 1 shows floors, walls
and a 0x27 = doortop-with-floor + flag 0x20). Levels 2020..2033 are byte-identical copies of 2000..2013
except for two bytes (their purpose is still open).

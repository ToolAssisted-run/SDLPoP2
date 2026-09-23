/* read_input (0823:10A0 / 1178) against captured key tables and the controls the game derived from them. The BIOS
 * shift flags are rebuilt from the table (left shift 0x37, right shift 0x43, ctrl 0x2A, alt 0x45).
 * control() sees control_x negated when the prince faces right (0AFF:1336). usage: inputtest pairs.bin (from tests/inputtest.py) */
#include <stdio.h>
#include <string.h>
#include "../src/types.h"
#include "../src/globals.h"
int main(int argc, char **argv)
{
	FILE *f = argc > 1 ? fopen(argv[1], "rb") : NULL; if (!f) return 2;
	uint8_t rec[0x74]; int n = 0, bad = 0;
	while (fread(rec, 1, sizeof rec, f) == sizeof rec) {
		n++; memcpy(key_table, rec, 0x70);
		bios_shift_flags = (key_table[0x37] ? 2 : 0) | (key_table[0x43] ? 1 : 0) | (key_table[0x2A] ? 4 : 0) | (key_table[0x45] ? 8 : 0);
		read_input();
		if (rec[0x73] == 0) control_x = -control_x;   /* 0AFF:12CA: control() sees x relative to a right-facing prince */
		if ((uint8_t)control_x != rec[0x70] || (uint8_t)control_y != rec[0x71] || (uint8_t)control_shift != rec[0x72]) {
			bad++; printf("record %d: got %d %d %d, game %d %d %d\n", n, control_x, control_y, control_shift, (int8_t)rec[0x70], (int8_t)rec[0x71], (int8_t)rec[0x72]);
		}
	}
	printf("input: %d records, %d mismatching\n", n, bad); return bad != 0;
}

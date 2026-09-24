#!/usr/bin/env python3
"""Pseudo-code of an OVL00 routine from the listing: calls with their pushed arguments (Pascal order: first pushed =
first parameter), jumps and labels.  nispseudo.py LISTING START END   (e.g. $POP2_WORKSPACE/work/ovl00_2D3E.asm 2D7D:2E27 2D7D:3E00)"""
import sys, re
NAMES = {
 '0x26bc:0x876': 'draw_shape', '0x2583:0x6': 'draw_img', '0x25a1:0xfa': 'shape_get', '0x194c:0x6632': 'fill_rect',
 '0x194c:0x4cb0': 'copy_bits', '0x194c:0x3890': 'port_new', '0x194c:0x79a3': 'setpal', '0x2631:0x37e': 'fade_to',
 '0x2631:0x64a': 'fade_out_clear', '0x2797:0x158': 'timer_wait', '0x2797:0x104': 'wait_countdown', '0x2797:0x176': 'wait_sound',
 '0x194c:0x840e': 'sound_play', '0x194c:0x805c': 'sound_load', '0x194c:0x83d2': 'sound_stop', '0x194c:0x8426': 'sound_playing',
 '0x194c:0x6f4c': 'get_resource', '0x194c:0x16e6': 'free_handle', '0x194c:0x15b2': 'lock', '0x2a31:0xcf9': 'play_anim',
 '0x2a31:0xce5': 'dissolve', '0x194c:0x3320': 'sound_stop_all', '0x194c:0x4e36': 'port_dispose', '0x194c:0x647e': 'union_rect',
 '0x194c:0x50ec': 'offset_rect', '0x194c:0x5266': 'sect_rect', '0x2631:0x3e6': 'fade_new', '0x2631:0x4e0': 'fade_step',
 '0x2631:0x296': 'setpal_banks', '0x2797:0x9c': 'key_pressed', '0x194c:0x5194': 'save_rect', '0x2699:0xfa': 'port_free2',
 '0x2699:0xe': 'port_new2', '0x194c:0x4f8c': 'font_load', '0x194c:0x4fac': 'set_font', '0x194c:0x5334': 'text_box',
}
NEAR = {'0xe': 'shpl_load', '0x74': 'strl_count', '0x9c': 'play_or_time', '0xca': 'say', '0xe4': 'wait_or_time', '0x135': 'wait_cue',
 '0x16b': 'load_25000_fonts', '0x193': 'free_25000_fonts', '0x1af': 'show_text', '0x3ebf': 'pic_fill', '0x3eea': 'pic_draw',
 '0x3f1c': 'pic_img', '0x3f52': 'pic_flash', '0x3f9e': 'pic_show', '0x3fbd': 'pic_fade_in', '0x4011': 'pic_dissolve',
 '0x4119': 'say_text', '0x4147': 'shape_rect', '0x4a6a': 'key_handler_on'}
def main():
    lst, a, b = sys.argv[1], sys.argv[2], sys.argv[3]
    lines = [l.rstrip('\n') for l in open(lst) if a <= l[:9] < b]
    targets = set()
    for l in lines:
        m = re.search(r'\bj\w+ (?:short )?0x([0-9a-f]+)$', l)
        if m: targets.add(int(m.group(1), 16))
    regs = {}; pushes = []; out = []
    for l in lines:
        addr, ins = l[:9], l[11:]
        off = int(addr[5:], 16)
        if off in targets: out.append('L_%04X:' % off)
        m = re.match(r'mov (\w\w),(.+)$', ins)
        if m and m.group(1) in ('ax','bx','cx','dx','si','di'): regs[m.group(1)] = m.group(2); continue
        m = re.match(r'sub (\w\w),\1$', ins)
        if m: regs[m.group(1)] = '0'; continue
        m = re.match(r'lea (\w\w),\[(.+)\]$', ins)
        if m: regs[m.group(1)] = '&' + m.group(2); continue
        m = re.match(r'mov al,(.+)$', ins)
        if m: regs['ax'] = m.group(1); continue
        m = re.match(r'push (.+)$', ins)
        if m:
            v = m.group(1)
            if v == 'cs': continue
            if v in ('ds', 'ss', 'es'): pushes.append('SEG:' + v); continue
            pushes.append(regs.get(v, v) if v in regs else v); continue
        m = re.match(r'call (0x[0-9a-f]+:0x[0-9a-f]+)$', ins)
        m2 = re.match(r'call (0x[0-9a-f]+)$', ins)
        if m or m2:
            name = NAMES.get(m.group(1), 'far_' + m.group(1)) if m else NEAR.get(m2.group(1), 'near_' + m2.group(1))
            args = [p for p in pushes if not p.startswith('SEG:')]
            out.append('    %s  %s(%s)' % (addr, name, ', '.join(args)))
            pushes = []; regs = {}
            continue
        m = re.match(r'mov (?:word |byte )?(\[.+\]),(.+)$', ins)
        if m: out.append('    %s  %s = %s' % (addr, m.group(1), regs.get(m.group(2), m.group(2)))); continue
        if ins.startswith('j') or ins.startswith('cmp') or ins.startswith('or ') or ins.startswith('ret') or ins.startswith('test') or ins.startswith('inc') or ins.startswith('dec') or ins.startswith('add') or ins.startswith('shl') or ins.startswith('neg') or ins.startswith('sub'):
            out.append('    %s    %s' % (addr, ins))
    print('\n'.join(out))
main()

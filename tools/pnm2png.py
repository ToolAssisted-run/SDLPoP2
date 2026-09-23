#!/usr/bin/env python3
"""pnm2png.py IN.pgm|IN.ppm OUT.png: binary PGM (P5) or PPM (P6) to PNG, standard library only."""
import struct, zlib, sys
b=open(sys.argv[1],'rb').read(); parts=b.split(maxsplit=4); typ=parts[0]; w,h=int(parts[1]),int(parts[2]); px=parts[4]
n=1 if typ==b'P5' else 3
raw=b''.join(b'\0'+px[y*w*n:(y+1)*w*n] for y in range(h))
def chunk(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
open(sys.argv[2],'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0 if n==1 else 2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

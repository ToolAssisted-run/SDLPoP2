#!/usr/bin/env python3
"""Dump a PoP2 level resource (12024-byte body from PRINCE.DAT untyped ids 2000..2033, or a RAM
image slice starting at DS:2BB8). usage: leveldump.py level.bin [room ...]"""
import struct, sys
TILES=0x0000; ATTRS=0x0348; LINKS=0x17BC; HDR=0x17F3; ROOMS=0x1867; NROOMS_MAX=28
NAMES={0:".",1:"_",2:"^",3:"|",4:"#",5:"b",6:"B",7:"T",8:"P",9:"p",10:"o",11:"L",12:"t",13:"M",14:"d",15:"O",16:"[",17:"]",18:"C",19:"i",20:"W",21:"s",22:"S",23:"<",24:">",25:"%",26:"v",27:"~",28:"(",29:")",30:"D"}
def load(path):
    b=open(path,"rb").read()
    hdr=b[HDR:HDR+0x74]
    lvl=dict(nrooms=hdr[0x4D],number=hdr[0x54],start_room=hdr[0x6D],start_tile=hdr[0x6E],start_dir=struct.unpack("b",hdr[0x6F:0x70])[0],hdr=hdr)
    rooms={}
    for r in range(1,lvl["nrooms"]+1):
        t=b[TILES+(r-1)*30:TILES+r*30]; a=[struct.unpack_from("<I",b,ATTRS+r*120+i*4)[0] for i in range(30)]
        rec=b[ROOMS+(r-1)*0x74:ROOMS+r*0x74]
        chars=[rec[1+i*23:1+(i+1)*23] for i in range(5)]
        rooms[r]=dict(tiles=t,attrs=a,nchars=rec[0],chars=chars,links=tuple(b[LINKS+r*4:LINKS+r*4+4]))
    return lvl,rooms
if __name__=="__main__":
    lvl,rooms=load(sys.argv[1]); want=[int(a) for a in sys.argv[2:]] or sorted(rooms)
    print("level %d: %d rooms, start room %d tile %d dir %d"%(lvl["number"],lvl["nrooms"],lvl["start_room"],lvl["start_tile"],lvl["start_dir"]))
    for r in want:
        R=rooms[r]; print("room %d (%d chars) links L%d R%d U%d D%d"%((r,R["nchars"])+R["links"]))
        for row in range(3): print("   "+" ".join("%2d"%x for x in R["tiles"][row*10:(row+1)*10])+"   "+"".join(NAMES.get(x&0x1F,"?") for x in R["tiles"][row*10:(row+1)*10]))
        for i in range(R["nchars"]): print("   char %d: %s"%(i,R["chars"][i].hex()))

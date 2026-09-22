#!/usr/bin/env python3
"""Decode PoP2's SEQUENCE.DAT (DAT container, one "SQES" resource per sequence, 16-bit LE items with
PoP1-style negative opcodes). usage: seqdump.py SEQUENCE.DAT [id...]"""
import struct, sys
OPS={0xFFFF:"JMP",0xFFFE:"FLIP",0xFFFD:"UP",0xFFFC:"DOWN",0xFFFB:"DX",0xFFFA:"DY",0xFFF9:"ACT",0xFFF8:"SET_FALL",
     0xFFF7:"ADD_FALL",0xFFF6:"JMP_IF_FEATHER",0xFFF5:"SET_85F8",0xFFF4:"KNOCK_DOWN",0xFFF3:"KNOCK_UP",0xFFF2:"SEQ_CTL",0xFFF1:"SND",
     0xFFF0:"LVL6_COUNTER",0xFFEF:"CLEAR_CHAR",0xFFEE:"CLR_CHAR19",0xFFED:"Y_TO_FLOOR",0xFFEC:"OP_EC",0xFFEB:"SET_CHAR24",0xFFEA:"JMP_IF",0xFFE9:"HOLD",0xFFE8:"FLASH"}
NARGS={"JMP":1,"DX":1,"DY":1,"ACT":1,"SET_FALL":2,"ADD_FALL":2,"JMP_IF_FEATHER":1,"SET_85F8":1,"SEQ_CTL":1,"SND":1,"SET_CHAR24":1,"JMP_IF":3,"FLASH":-1}
def load(path):
    d=open(path,"rb").read(); to,ts=struct.unpack_from("<IH",d,0)
    cnt=struct.unpack_from("<H",d,to+8)[0]
    seqs={}
    for i in range(cnt):
        rid,off,size,fl,x=struct.unpack_from("<HIHHB",d,to+10+i*11)
        body=d[off+1:off+size]                       # leading byte = DAT checksum
        words=[struct.unpack_from("<H",body,k)[0] for k in range(0,len(body)-1,2)]
        if len(body)%2: words.append(body[-1])       # trailing byte = target sequence id of the final JMP
        seqs[rid]=words
    return seqs
def decode(words):
    out=[]; i=0
    while i<len(words):
        w=words[i]; i+=1
        if w in OPS:
            op=OPS[w]; n=NARGS.get(op,0)
            if n<0:                                   # FLASH: word, and a second word when the first is nonzero
                n=1 if i<len(words) and words[i]==0 else 2
            args=words[i:i+n]; i+=n
            out.append(op+("(%s)"%",".join(str(a if a<0x8000 else a-0x10000) for a in args) if n else ""))
        else: out.append("f%d"%w)
    return out
if __name__=="__main__":
    seqs=load(sys.argv[1]); ids=[int(a) for a in sys.argv[2:]] or sorted(seqs)
    for rid in ids: print("seq %3d (%3d words): %s"%(rid,len(seqs[rid])," ".join(decode(seqs[rid]))))

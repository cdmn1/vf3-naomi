#!/usr/bin/env python3
"""Stream-decode the v77 VF3 pose trace without loading the 514 MB stream into RAM."""
import csv, gzip, mmap, re, struct, sys\nfrom bisect import bisect_right
from collections import Counter
from pathlib import Path

TRACE = Path(sys.argv[1])
WATCH = Path(sys.argv[2])
OUT = Path(sys.argv[3])
OUT.mkdir(parents=True, exist_ok=True)
PC_OPEN, PC_CLOSE, PCX_OPEN, PCX_CLOSE, MEMORY, DMA, OPAQUE = 0xFA10,0xFA11,0xFA12,0xFA13,0xFA20,0xFA21,0xFA22

pcs=set()
ranges=[]
for line in WATCH.read_text().splitlines():
    line=line.split("#",1)[0].strip()
    m=re.fullmatch(r"pc\s+(0x[0-9a-fA-F]+)",line)
    if m: pcs.add(int(m.group(1),16)&0xffffffff)
    m=re.fullmatch(r"mem\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)",line)
    if m:
        a,n=(int(x,16) for x in m.groups())
        ranges.append((a,a+n))
ranges.sort()
range_starts=[lo for lo,hi in ranges]
def watched(a):
    pos=bisect_right(range_starts,a)-1
    return pos>=0 and a<ranges[pos][1]

pc_counts=Counter(); mem_counts=Counter(); widths=Counter(); unknown=Counter()
pc_path=OUT/"v77-pc-snapshots.csv.gz"
mem_path=OUT/"v77-watched-memory.csv.gz"
pc_fields=["event_index","word_index","pc","pr"]+[f"r{i}" for i in range(16)]
mem_fields=["event_index","word_index","pc","access","width","address","value"]
nwords=TRACE.stat().st_size//8
if TRACE.stat().st_size%8: raise SystemExit("Trace size is not divisible by 8")
events=0; pc_total=mem_total=dma_total=opaque_total=0; malformed=0
with TRACE.open("rb") as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as mm, \
     gzip.open(pc_path,"wt",newline="",encoding="utf-8") as pz, \
     gzip.open(mem_path,"wt",newline="",encoding="utf-8") as mz:
    pw=csv.DictWriter(pz,fieldnames=pc_fields); pw.writeheader()
    mw=csv.DictWriter(mz,fieldnames=mem_fields); mw.writeheader()
    i=0
    while i<nwords:
        w=struct.unpack_from("<Q",mm,i*8)[0]
        marker=w&0xffff
        pc=(w>>16)&0xffffffff
        if marker==PC_OPEN:
            if i+7>=nwords: malformed+=1; break
            close=struct.unpack_from("<Q",mm,(i+7)*8)[0]
            if (close&0xffff)!=PC_CLOSE or ((close>>16)&0xffffffff)!=pc:
                malformed+=1; i+=1; continue
            if pc in pcs or (pc | 0x80000000) in pcs:
                regs=[struct.unpack_from("<Q",mm,(i+j)*8)[0]&0xffffffff for j in range(1,7)]
                row={"event_index":events,"word_index":i,"pc":f"0x{pc:08X}","pr":""}
                for k,v in zip(("r13","r14","r4","r5","r6","r7"),regs): row[k]=f"0x{v:08X}"
                pw.writerow(row); pc_counts[pc]+=1
            pc_total+=1; events+=1; i+=8; continue
        if marker==PCX_OPEN:
            if i+18>=nwords: malformed+=1; break
            close=struct.unpack_from("<Q",mm,(i+18)*8)[0]
            if (close&0xffff)!=PCX_CLOSE or ((close>>16)&0xffffffff)!=pc:
                malformed+=1; i+=1; continue
            if pc in pcs:
                pr=struct.unpack_from("<Q",mm,(i+1)*8)[0]&0xffffffff
                regs=[struct.unpack_from("<Q",mm,(i+j)*8)[0]&0xffffffff for j in range(2,18)]
                row={"event_index":events,"word_index":i,"pc":f"0x{pc:08X}","pr":f"0x{pr:08X}"}
                row.update({f"r{k}":f"0x{v:08X}" for k,v in enumerate(regs)})
                pw.writerow(row); pc_counts[pc]+=1
            pc_total+=1; events+=1; i+=19; continue
        if marker==MEMORY:
            if i+3>=nwords: malformed+=1; break
            meta,addr,val=struct.unpack_from("<3Q",mm,(i+1)*8)
            width=(meta>>48)&0xff; write=(meta>>47)&1
            if watched(addr):
                mw.writerow({"event_index":events,"word_index":i,"pc":f"0x{pc:08X}",
                    "access":"W" if write else "R","width":width,
                    "address":f"0x{addr:08X}","value":f"0x{val:016X}" if width==8 else f"0x{val&0xffffffff:08X}"})
                mem_counts[(addr,"W" if write else "R")]+=1
            widths[width]+=1; mem_total+=1; events+=1; i+=4; continue
        if marker==DMA:
            dma_total+=1; events+=1; i+=6; continue
        if marker==OPAQUE:
            opaque_total+=1; events+=1; i+=9; continue
        unknown[marker]+=1
        i+=1
report=OUT/"REPORT.txt"
with report.open("w") as f:
    f.write("VF3 v77 POSE TRACE STREAM ANALYSIS\n")
    f.write(f"trace={TRACE.name}\nbytes={TRACE.stat().st_size}\nu64_words={nwords}\n")
    f.write(f"decoded_pc_groups={pc_total}\ndecoded_memory_records={mem_total}\ndecoded_dma_records={dma_total}\ndecoded_opaque_fa22_records={opaque_total}\nmalformed_markers={malformed}\n")
    f.write(f"retained_pc_snapshots={sum(pc_counts.values())}\nretained_watched_memory_records={sum(mem_counts.values())}\n")
    f.write("PC_HITS\n")
    for pc,count in sorted(pc_counts.items()): f.write(f"0x{pc:08X} {count}\n")
    f.write("MEMORY_WIDTHS\n")
    for width,count in sorted(widths.items()): f.write(f"{width} {count}\n")
    f.write("TOP_WATCHED_MEMORY\n")
    for (addr,access),count in mem_counts.most_common(100): f.write(f"0x{addr:08X} {access} {count}\n")
    f.write("UNRECOGNIZED_LOW16_MARKERS_TOP20\n")
    for marker,count in unknown.most_common(20): f"0x{marker:04X} {count}\n"; f.write(f"0x{marker:04X} {count}\n")
print(report.read_text())

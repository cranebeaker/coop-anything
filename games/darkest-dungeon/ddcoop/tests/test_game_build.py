"""Read-only static audit for the supported native build; never loads or runs the executable.

Requires developer-only capstone5 (not shipped to players). --decoder-dir may point at an isolated
pip --target directory. Run after editing addresses, copied hook lengths or model layouts.
"""
from pathlib import Path
import argparse, hashlib, re, struct, sys

args=argparse.ArgumentParser()
args.add_argument('exe',type=Path)
args.add_argument('--decoder-dir',type=Path)
opt=args.parse_args()
if opt.decoder_dir:sys.path.insert(0,str(opt.decoder_dir))
from capstone import Cs,CS_ARCH_X86,CS_MODE_64,CS_GRP_CALL,CS_GRP_JUMP
from capstone.x86_const import X86_OP_IMM,X86_OP_MEM,X86_REG_RIP

ROOT=Path(__file__).resolve().parents[1]
data=opt.exe.read_bytes()
BUILD='f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
assert hashlib.sha256(data).hexdigest()==BUILD,'Wrong executable: rerun the port audit, never whitelist blindly'
pins=(ROOT/'launcher/src/lr_hashes.h').read_text()
assert re.search(r'#define LR_HASH_GAME "'+BUILD+'"',pins)
assert BUILD in (ROOT/'runtime/plugin.c').read_text()
pe=struct.unpack_from('<I',data,60)[0]
first=pe+24+struct.unpack_from('<H',data,pe+20)[0]
sections=[struct.unpack_from('<8sIIII',data,first+40*i) for i in range(struct.unpack_from('<H',data,pe+6)[0])]
def read(rva,n):
    for _,vs,va,rs,raw in sections:
        if va<=rva and rva+n<=va+rs:
            return data[raw+rva-va:raw+rva-va+n]
    raise AssertionError(f'Unbacked RVA {rva:x} / {n}')
def u32(rva):return struct.unpack('<I',read(rva,4))[0]
def u64(rva):return struct.unpack('<Q',read(rva,8))[0]
text=next(s for s in sections if s[0].startswith(b'.text'))
code=data[text[4]:text[4]+text[3]]
addresses={}
for name,va,pattern in re.findall(r'ADDR\((\w+),\s*0x([0-9a-f]+)ULL,\s*\{([^}]+)\}',(ROOT/'src/addresses.inc').read_text()):
    rva=int(va,16)-0x140000000
    needle=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]+)',pattern))
    assert len(needle)==48 and read(rva,48)==needle and code.count(needle)==1,name
    addresses[name]=rva
assert len(addresses)==50

source=(ROOT/'runtime/dd_runtime.c').read_text()
names=['activity_assign_hero','activity_slot_widget_ctor','buy_upgrade','wagon_buy','hero_recruit',
       'recruit_panel_show','recruit_panel_hide','roster_ui_drop','wagon_sell']
lengths=[int(n) for n in re.search(r'size_t sizes\[\] = \{([^}]+)',source)[1].split(',')]
hooks=list(zip(names,lengths))
hooks += [(name,int(n)) for name,n in re.findall(r'\{"([a-z_]+)",\s*\(uintptr_t\)\w+,\s*&[^,]+,\s*(\d+)\}',
                                              (ROOT/'runtime/dd_hamlet_bridge.inc').read_text())]
assert len(hooks)==28
cs=Cs(CS_ARCH_X86,CS_MODE_64);cs.detail=True
for name,n in hooks:
    ins=list(cs.disasm(read(addresses[name],n),addresses[name]))
    assert sum(i.size for i in ins)==n,(name,'split instruction')
    assert not any(i.group(CS_GRP_CALL) or i.group(CS_GRP_JUMP) or any(
        o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP for o in i.operands) for i in ins),(name,'PC-relative trampoline copy')
# The former14-byte combat hook now splits an instruction. This is an actual update regression.
assert sum(i.size for i in cs.disasm(read(addresses['hero_toggle_combat'],14),addresses['hero_toggle_combat'])) != 14

vt_checks={0xe9c338:'NomadWagon@Building',0xe9c2e0:'TrinketSupply@Store@Building',
           0xe9d088:'StageCoach@Building',0xe9cfd0:'HeroRecruit@Store@Building',
           0xec0cb0:'HeroRecruitDisplay@Panel@TownUI',0xebcc00:'HeroSlot@Panel@TownUI',
           0xee0990:'RecruitHeroSlotInterface',0xec00a0:'RosterList@Panel@TownUI',
           0xeba728:'EmbarkParty@Panel@TownUI',0xee2ae8:'CharacterDisplay@Panel@UI',
           0xec37a8:'TownDisplay@Screen@TownUI',0xee0838:'QuestSelectHeroSlotInterface',
           0xebda18:'PreChildrenShowInternal@TrinketSupplyDisplay'}
for vt,expected in vt_checks.items():
    col=u64(vt-8)-0x140000000
    assert u32(col)==1 and u32(col+20)==col
    typename=read(u32(col+12)+16,350).split(b'\0')[0].decode()
    assert expected in typename,(hex(vt),typename)
    assert text[2]<=u64(vt)-0x140000000<text[2]+text[1]

# These markers were traced through the ordinary native Hero/slot/equipment paths, not fake objects.
# Cross-check key reader fields against real instructions so a coordinated fake-layout error is caught.
markers={'hero_toggle_camping':(0x1398,0x13a0),
         'hero_toggle_combat':(0x1320,0x13c8,0x13d0),
         'character_refresh_skills':(0x1320,0x137c,0x1398,0x13a0,0x13c8,0x13d0),
         'actor_refresh_trinkets':(0x1280,0x1288,0x1290),
         'roster_change_location':(0x15b0,),
         'roster_add_entry':(0x1638,),
         'wagon_buy':(0x40,0x84,0x9c)}
for name,values in markers.items():
    # Include chained unwind fragments; the bool combat function has several .pdata ranges.
    ins=list(cs.disasm(read(addresses[name],3000 if name=='actor_refresh_trinkets' else 720),addresses[name]))
    operands={o.mem.disp if o.type==X86_OP_MEM else o.imm for i in ins for o in i.operands
              if o.type in (X86_OP_MEM,X86_OP_IMM)}
    assert all(value in operands for value in values),(name,values)
for caller,target in ((0x8045ec,'hero_toggle_combat'),(0x8040c3,'hero_toggle_camping')):
    i=next(cs.disasm(read(caller,5),caller))
    assert i.mnemonic=='call' and i.operands[0].imm==addresses[target]
bridge=(ROOT/'runtime/dd_hamlet_bridge.inc').read_text()
assert '0x8045f1' in bridge and '0x8040c8' in bridge

def rip_targets(start,n):
    return {i.address+i.size+o.mem.disp for i in cs.disasm(read(start,n),start)
            for o in i.operands if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP}
assert 0x118e138 in rip_targets(addresses['campaign_current'],113)
assert 0x2a6d4bb in rip_targets(addresses['roster_recruit_count'],232)
assert 0x1185248 in rip_targets(0x2330a0,2500) # Normal storage-manager references.
assert {0x118e0f8,0x2a7dca4} <= rip_targets(0x6a7260,180) # Normal sale quote, definition cost +a4.
assert {0x2a6f868} <= rip_targets(0x133670,0x29) # Static initializer of hash("trinket").
i=next(cs.disasm(read(0x133672,7),0x133672))
key=i.address+i.size+i.operands[1].mem.disp
assert read(key,8)==b'trinket\0'
for value in (0x488,0x2d8,0x2e0):
    ins=list(cs.disasm(read(0x7ed2d0,1100),0x7ed2d0))
    assert any((o.mem.disp if o.type==X86_OP_MEM else o.imm)==value for i in ins
               for o in i.operands if o.type in (X86_OP_MEM,X86_OP_IMM)),hex(value)

print('Build28385: pinned EXE,50 unique addresses,28 safe hook boundaries,13 RTTI identities,Hero/item fields,global references and skill callers: OK')

import sys, pefile
pe = pefile.PE(sys.argv[1], fast_load=True)
for s in pe.sections: print(s.Name.rstrip(b"\0"), hex(s.VirtualAddress), hex(s.Misc_VirtualSize), hex(s.SizeOfRawData), hex(s.Characteristics))

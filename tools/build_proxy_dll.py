"""Build standalone 32-bit dinput8.dll export-forwarder proxy hook for Resident Evil Revelations.

Pure Python, no external compiler or dependencies needed.
Exports DirectInput8Create with forwarder to C:\\Windows\\SysWOW64\\dinput8.DirectInput8Create,
and executes hook initialization on DLL_PROCESS_ATTACH.

Usage:
    python tools/build_proxy_dll.py <output_dll_path>
"""

import os
import struct
import sys

def build_proxy_dll(out_path):
    image_base = 0x10000000
    section_align = 0x1000
    file_align = 0x200
    
    rva_base = 0x2000
    exp_dir_size = 40
    exp_funcs_off = 0x28
    exp_names_off = 0x2C
    exp_ords_off = 0x30
    
    imp_desc_off = 0x40
    ilt_off = 0x68
    iat_off = 0x78
    str_area_off = 0x88
    
    dll_exp_name = b"dinput8.dll\x00"
    exp_func_name = b"DirectInput8Create\x00"
    fwd_target = b"C:\\Windows\\SysWOW64\\dinput8.DirectInput8Create\x00"
    kernel32_name = b"KERNEL32.dll\x00"
    
    hint_CreateFileA = struct.pack('<H', 0) + b"CreateFileA\x00"
    if len(hint_CreateFileA) % 2 != 0: hint_CreateFileA += b'\x00'
    
    hint_WriteFile = struct.pack('<H', 0) + b"WriteFile\x00"
    if len(hint_WriteFile) % 2 != 0: hint_WriteFile += b'\x00'
    
    hint_CloseHandle = struct.pack('<H', 0) + b"CloseHandle\x00"
    if len(hint_CloseHandle) % 2 != 0: hint_CloseHandle += b'\x00'
    
    log_filename = b"rerev_hook.log\x00"
    log_message = b"[ReRevCoop] DirectInput8 proxy hook initialized in Resident Evil Revelations!\r\n[ReRevCoop] Isolated test environment ready for memory exploration.\r\n"
    
    cur_off = str_area_off
    dll_exp_name_rva = rva_base + cur_off; cur_off += len(dll_exp_name)
    exp_func_name_rva = rva_base + cur_off; cur_off += len(exp_func_name)
    fwd_target_rva = rva_base + cur_off; cur_off += len(fwd_target)
    kernel32_name_rva = rva_base + cur_off; cur_off += len(kernel32_name)
    
    h_CreateFileA_rva = rva_base + cur_off; cur_off += len(hint_CreateFileA)
    h_WriteFile_rva = rva_base + cur_off; cur_off += len(hint_WriteFile)
    h_CloseHandle_rva = rva_base + cur_off; cur_off += len(hint_CloseHandle)
    
    log_filename_rva = rva_base + cur_off; cur_off += len(log_filename)
    log_message_rva = rva_base + cur_off; cur_off += len(log_message)
    rdata_total_size = cur_off
    
    export_dir = struct.pack('<IIHHIIIIIII',
        0, 0, 0, 0,
        dll_exp_name_rva,
        1, 1, 1,
        rva_base + exp_funcs_off,
        rva_base + exp_names_off,
        rva_base + exp_ords_off
    )
    exp_funcs = struct.pack('<I', fwd_target_rva)
    exp_names = struct.pack('<I', exp_func_name_rva)
    exp_ords = struct.pack('<H', 0)
    
    imp_desc = struct.pack('<IIIII',
        rva_base + ilt_off,
        0, 0,
        kernel32_name_rva,
        rva_base + iat_off
    )
    imp_desc_null = bytes(20)
    
    thunks = struct.pack('<IIII',
        h_CreateFileA_rva,
        h_WriteFile_rva,
        h_CloseHandle_rva,
        0
    )
    
    iat_CreateFileA_addr = image_base + rva_base + iat_off + 0
    iat_WriteFile_addr   = image_base + rva_base + iat_off + 4
    iat_CloseHandle_addr = image_base + rva_base + iat_off + 8
    
    code = bytearray()
    code += bytes([0x55])                          # push ebp
    code += bytes([0x89, 0xE5])                    # mov ebp, esp
    code += bytes([0x56])                          # push esi
    code += bytes([0x8B, 0x45, 0x0C])              # mov eax, [ebp + 12] (reason)
    code += bytes([0x83, 0xF8, 0x01])              # cmp eax, 1 (DLL_PROCESS_ATTACH)
    code += bytes([0x75, 0x36])                    # jne done
    
    # CreateFileA(log_filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
    code += bytes([0x6A, 0x00])                    # push 0
    code += bytes([0x68, 0x80, 0x00, 0x00, 0x00])        # push 0x80
    code += bytes([0x6A, 0x02])                    # push 2
    code += bytes([0x6A, 0x00])                    # push 0
    code += bytes([0x6A, 0x01])                    # push 1
    code += bytes([0x68, 0x00, 0x00, 0x00, 0x40])        # push 0x40000000
    code += bytes([0x68]) + struct.pack('<I', image_base + log_filename_rva)
    code += bytes([0xFF, 0x15]) + struct.pack('<I', iat_CreateFileA_addr)
    
    code += bytes([0x83, 0xF8, 0xFF])              # cmp eax, -1
    code += bytes([0x74, 0x1B])                    # je done
    code += bytes([0x89, 0xC6])                    # mov esi, eax
    
    # WriteFile(esi, log_message, len, &written, NULL)
    code += bytes([0x6A, 0x00])                    # push 0
    code += bytes([0x8D, 0x55, 0x0C])              # lea edx, [ebp + 12]
    code += bytes([0x52])                          # push edx
    code += bytes([0x68]) + struct.pack('<I', len(log_message))
    code += bytes([0x68]) + struct.pack('<I', image_base + log_message_rva)
    code += bytes([0x56])                          # push esi
    code += bytes([0xFF, 0x15]) + struct.pack('<I', iat_WriteFile_addr)
    
    # CloseHandle(esi)
    code += bytes([0x56])                          # push esi
    code += bytes([0xFF, 0x15]) + struct.pack('<I', iat_CloseHandle_addr)
    
    # done:
    code += bytes([0x5E])                          # pop esi
    code += bytes([0xB8, 0x01, 0x00, 0x00, 0x00])        # mov eax, 1
    code += bytes([0x5D])                          # pop ebp
    code += bytes([0xC2, 0x0C, 0x00])              # ret 12
    
    text_raw = bytes(code).ljust(file_align, b'\x00')
    
    rdata_buf = bytearray(max(file_align, (rdata_total_size + file_align - 1) & ~(file_align - 1)))
    rdata_buf[0:40] = export_dir
    rdata_buf[exp_funcs_off:exp_funcs_off + 4] = exp_funcs
    rdata_buf[exp_names_off:exp_names_off + 4] = exp_names
    rdata_buf[exp_ords_off:exp_ords_off + 2] = exp_ords
    
    rdata_buf[imp_desc_off:imp_desc_off + 20] = imp_desc
    rdata_buf[imp_desc_off + 20:imp_desc_off + 40] = imp_desc_null
    
    rdata_buf[ilt_off:ilt_off + 16] = thunks
    rdata_buf[iat_off:iat_off + 16] = thunks
    
    def put_str(off, s):
        rdata_buf[off:off + len(s)] = s
    put_str(dll_exp_name_rva - rva_base, dll_exp_name)
    put_str(exp_func_name_rva - rva_base, exp_func_name)
    put_str(fwd_target_rva - rva_base, fwd_target)
    put_str(kernel32_name_rva - rva_base, kernel32_name)
    put_str(h_CreateFileA_rva - rva_base, hint_CreateFileA)
    put_str(h_WriteFile_rva - rva_base, hint_WriteFile)
    put_str(h_CloseHandle_rva - rva_base, hint_CloseHandle)
    put_str(log_filename_rva - rva_base, log_filename)
    put_str(log_message_rva - rva_base, log_message)
    
    dos_header = bytearray(0x80)
    dos_header[0:2] = b'MZ'
    dos_header[0x3C:0x40] = struct.pack('<I', 0x80)
    
    pe_sig = b'PE\x00\x00'
    file_header = struct.pack('<HHIIIHH',
        0x014C, # Machine: i386
        2,      # 2 sections: .text, .rdata
        0, 0, 0,
        0x00E0,
        0x2102  # DLL | 32BIT | EXECUTABLE
    )
    
    opt_header = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII',
        0x010B, # PE32
        14, 0,
        len(text_raw),
        len(rdata_buf),
        0,
        0x1000,
        0x1000,
        0x2000,
        image_base,
        section_align,
        file_align,
        6, 0, 0, 0, 6, 0, 0,
        0x3000,
        file_align,
        0,
        2,
        0x0000, # NO ASLR
        0x100000, 0x1000,
        0x100000, 0x1000,
        0, 16
    )
    
    data_dirs = bytearray(16 * 8)
    # The export directory range must contain the forwarder string, otherwise the
    # Windows loader treats the function RVA as code (crash on DirectInput8Create).
    exp_dir_size = (fwd_target_rva + len(fwd_target)) - rva_base
    data_dirs[0:8] = struct.pack('<II', rva_base, exp_dir_size)
    data_dirs[8:16] = struct.pack('<II', rva_base + imp_desc_off, 40)
    data_dirs[12 * 8:12 * 8 + 8] = struct.pack('<II', rva_base + iat_off, 16)
    
    sec1 = struct.pack('<8sIIIIIIHHI',
        b'.text\x00\x00\x00',
        len(code),
        0x1000,
        len(text_raw),
        file_align,
        0, 0, 0, 0,
        0x60000020
    )
    
    sec2 = struct.pack('<8sIIIIIIHHI',
        b'.rdata\x00\x00',
        rdata_total_size,
        0x2000,
        len(rdata_buf),
        file_align * 2,
        0, 0, 0, 0,
        0xC0000040 # READ | WRITE | INITIALIZED_DATA
    )
    
    headers = (dos_header + pe_sig + file_header + opt_header + data_dirs + sec1 + sec2).ljust(file_align, b'\x00')
    pe_file = headers + text_raw + bytes(rdata_buf)
    
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(pe_file)
    print(f'Proxy DLL built: {out_path} ({len(pe_file)} bytes)')

if __name__ == '__main__':
    target = sys.argv[1] if len(sys.argv) > 1 else 'dinput8.dll'
    build_proxy_dll(target)

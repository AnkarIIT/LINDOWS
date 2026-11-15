#!/usr/bin/env python3
"""
LINDOWS mkimg.py — create a 16 MiB raw disk image with MBR + FAT32 partition
suitable for Limine BIOS boot (v12.10.0).
"""

import struct
import os
import sys

# --- Configuration ---
IMG_SIZE_SECTORS = 32768
BYTES_PER_SECTOR = 512
IMG_SIZE_BYTES = IMG_SIZE_SECTORS * BYTES_PER_SECTOR

PART_LBA_START = 2048
PART_SECTORS = 20480
TOTAL_IMG_SECTORS = IMG_SIZE_SECTORS

SECTORS_PER_CLUSTER = 8
RESERVED_SECTORS = 32
NUM_FATS = 2
SECTORS_PER_FAT = 20
FS_INFO_SECTOR_REL = 1
FS_INFO_BACKUP_REL = 7
DATA_START_REL = 72
ROOT_CLUSTER = 2

FNAME_KERNEL = b"KERNX64.BIN"
FNAME_BIOS = b"LIMINE~1.SYS"
FNAME_CONF = b"LIMINE.CON"

KERNEL_SIZE = 0
BIOS_SIZE = 0
CONF_SIZE = 0


def create_image(output_path):
    img = bytearray(IMG_SIZE_BYTES)
    
    # Step 1: Write MBR
    mbr = bytearray(512)
    mbr[446] = 0x80
    mbr[446 + 2] = 0x0B
    struct.pack_into('<I', mbr, 446 + 8, PART_LBA_START)
    struct.pack_into('<I', mbr, 446 + 12, PART_SECTORS)
    struct.pack_into('<I', mbr, 440, 0x20210509)
    img[0:512] = mbr
    
    # Step 2: Write BPB
    bpb_offset = PART_LBA_START * BYTES_PER_SECTOR
    bpb = bytearray(512)
    struct.pack_into('<H', bpb, 0, BYTES_PER_SECTOR)
    bpb[2] = SECTORS_PER_CLUSTER
    struct.pack_into('<H', bpb, 3, RESERVED_SECTORS)
    bpb[5] = NUM_FATS
    struct.pack_into('<H', bpb, 6, 0)
    struct.pack_into('<H', bpb, 8, 0)
    bpb[10] = 0xF8
    struct.pack_into('<H', bpb, 11, 0)
    bpb[13] = 0; bpb[14] = 0; bpb[15] = 0
    struct.pack_into('<I', bpb, 16, 0)
    struct.pack_into('<I', bpb, 20, PART_SECTORS)
    struct.pack_into('<I', bpb, 36, SECTORS_PER_FAT)
    bpb[40] = 0
    struct.pack_into('<I', bpb, 44, ROOT_CLUSTER)
    struct.pack_into('<H', bpb, 48, FS_INFO_SECTOR_REL)
    struct.pack_into('<H', bpb, 50, FS_INFO_BACKUP_REL)
    bpb[510] = 0x55; bpb[511] = 0xAA
    img[bpb_offset:bpb_offset+512] = bpb
    
    # Step 3: Write FAT1 at LBA 2080
    fat1_offset = (PART_LBA_START + 32) * BYTES_PER_SECTOR
    fat_size = SECTORS_PER_FAT * BYTES_PER_SECTOR
    fat1 = bytearray(fat_size)
    struct.pack_into('<I', fat1, 1 * 4, 0xFFFFFFFF)
    struct.pack_into('<I', fat1, 2 * 4, 0xFFFFFFFF)
    struct.pack_into('<I', fat1, 3 * 4, 0xFFFFFFFF)
    struct.pack_into('<I', fat1, 4 * 4, 0xFFFFFFFF)
    struct.pack_into('<I', fat1, 5 * 4, 0xFFFFFFFF)
    img[fat1_offset:fat1_offset+fat_size] = fat1
    
    # Step 4: Write FAT2 at LBA 2100
    fat2_offset = (PART_LBA_START + 52) * BYTES_PER_SECTOR
    img[fat2_offset:fat2_offset+fat_size] = fat1
    
    # Step 5: Write FSInfo sectors
    fsinfo_offset1 = (PART_LBA_START + FS_INFO_SECTOR_REL) * BYTES_PER_SECTOR
    fsinfo_offset2 = (PART_LBA_START + FS_INFO_BACKUP_REL) * BYTES_PER_SECTOR
    fsinfo1 = bytearray(512)
    fsinfo2 = bytearray(512)
    struct.pack_into('<I', fsinfo1, 0, 0x41615254)
    struct.pack_into('<I', fsinfo2, 0, 0x41615254)
    struct.pack_into('<I', fsinfo1, 508, 0xAA534255)
    struct.pack_into('<I', fsinfo2, 508, 0xAA534255)
    struct.pack_into('<I', fsinfo1, 8, 4083)
    struct.pack_into('<I', fsinfo2, 8, 4083)
    struct.pack_into('<I', fsinfo1, 12, 6)
    struct.pack_into('<I', fsinfo2, 12, 6)
    img[fsinfo_offset1:fsinfo_offset1+512] = fsinfo1
    img[fsinfo_offset2:fsinfo_offset2+512] = fsinfo2
    
    # Step 6: Write root directory entries (at absolute LBA 2120)
    dir_offset = (PART_LBA_START + 72) * BYTES_PER_SECTOR
    dir_sector = bytearray(512)
    
    # Entry 1: /KERNX64.BIN (kernel) - first cluster = 3
    dir_sector[0:11] = bytearray([0x4B, 0x45, 0x52, 0x4E, 0x58, 0x36, 0x34, 0x20, 0x45, 0x58, 0x45])
    dir_sector[11] = 0x00
    dir_sector[12] = 0
    dir_sector[13] = 0
    dir_sector[14:16] = b'\x00\x00'
    dir_sector[16:18] = b'\x00\x00'
    dir_sector[18:20] = b'\x00\x00'
    dir_sector[20:22] = b'\x03\x00'
    dir_sector[22:24] = struct.pack('<I', KERNEL_SIZE)
    
    # Entry 2: /LIMINE~1.SYS (bios) - first cluster = 4
    dir_sector[32:43] = bytearray([0x4C, 0x49, 0x4D, 0x49, 0x4E, 0x45, 0x7E, 0x31, 0x53, 0x59, 0x53])
    dir_sector[43] = 0x00
    dir_sector[44] = 0
    dir_sector[45] = 0
    dir_sector[46:48] = b'\x00\x00'
    dir_sector[48:50] = b'\x00\x00'
    dir_sector[50:52] = b'\x00\x00'
    dir_sector[52:54] = b'\x04\x00'
    dir_sector[54:56] = struct.pack('<I', BIOS_SIZE)
    
    # Entry 3: /LIMINE.CON (config) - first cluster = 5
    dir_sector[64:75] = bytearray([0x4C, 0x49, 0x4D, 0x49, 0x4E, 0x45, 0x20, 0x20, 0x43, 0x4F, 0x4E])
    dir_sector[75] = 0x00
    dir_sector[76] = 0
    dir_sector[77] = 0
    dir_sector[78:80] = b'\x00\x00'
    dir_sector[80:82] = b'\x00\x00'
    dir_sector[82:84] = b'\x00\x00'
    dir_sector[84:86] = b'\x05\x00'
    dir_sector[86:88] = struct.pack('<I', CONF_SIZE)
    
    # Entry 4: terminator (all zeros) - already zero
    
    img[dir_offset:dir_offset+512] = dir_sector
    
    # Step 7: Write file data clusters
    # Kernel at absolute LBA 2128 (cluster 3)
    kernel_offset = 2128 * BYTES_PER_SECTOR
    kernel_path = os.path.join(os.getcwd(), 'kernel', 'linspace64.bin')
    if os.path.exists(kernel_path):
        with open(kernel_path, 'rb') as f:
            kdata = f.read()[:BYTES_PER_SECTOR * 8]
        kdata = kdata + bytearray(BYTES_PER_SECTOR * 8 - len(kdata))
        img[kernel_offset:kernel_offset+BYTES_PER_SECTOR*8] = kdata
    
    # Bios at absolute LBA 2136 (cluster 4)
    bios_offset = 2136 * BYTES_PER_SECTOR
    bios_path = os.path.join(os.getcwd(), 'tools', 'limine', 'bin', 'limine-binary', 'limine-bios.sys')
    if os.path.exists(bios_path):
        with open(bios_path, 'rb') as f:
            bdata = f.read()[:BYTES_PER_SECTOR * 8]
        bdata = bdata + bytearray(BYTES_PER_SECTOR * 8 - len(bdata))
        img[bios_offset:bios_offset+BYTES_PER_SECTOR*8] = bdata
    
    # Config at absolute LBA 2144 (cluster 5)
    conf_offset = 2144 * BYTES_PER_SECTOR
    conf_text = "timeout: 0\nserial: yes\nprotocol: limine\npath: boot():/KERNX64.BIN\n"
    cdata = conf_text.encode('utf-8')[:BYTES_PER_SECTOR * 8]
    cdata = cdata + bytearray(BYTES_PER_SECTOR * 8 - len(cdata))
    img[conf_offset:conf_offset+BYTES_PER_SECTOR*8] = cdata
    
    # Step 8: Write limine.conf
    conf_path = os.path.join(os.getcwd(), 'boot', 'limine.conf')
    with open(conf_path, 'w') as f:
        f.write("timeout: 0\n")
        f.write("serial: yes\n")
        f.write("protocol: limine\n")
        f.write("path: boot():/KERNX64.BIN\n")
    
    # Step 9: Write the image
    with open(output_path, 'wb') as f:
        f.write(img)
    
    print(f"  Disk image created: {output_path} ({IMG_SIZE_SECTORS} sectors)")
    print(f"  Partition: starts LBA {PART_LBA_START}, {PART_SECTORS} sectors")
    print(f"  Kernel: {FNAME_KERNEL.decode()} at cluster 3 (LBA 2128)")
    print(f"  BIOS: {FNAME_BIOS.decode()} at cluster 4 (LBA 2136)")
    print(f"  Conf: {FNAME_CONF.decode()} at cluster 5 (LBA 2144)")
    print("Image ready for QEMU boot:")
    print(f"  qemu-system-i386 -drive file={output_path} -serial file:ser64.log -display none -cpu max")


def main():
    output_path = os.path.join(os.getcwd(), 'boot', 'lindows.img')
    create_image(output_path)
    
    # Run Limine BIOS install with --force
    lime_tool = os.path.join(os.getcwd(), 'tools', 'limine', 'bin', 'limine-binary', 'limine-tool-windows-x86', 'limine.exe')
    if os.path.exists(lime_tool):
        import subprocess
        win_img = output_path.replace('/', '\\')
        result = subprocess.run([lime_tool, 'bios-install', '--force', win_img], capture_output=True, text=True)
        print(f"Limine bios-install: {result.stdout}")
        if result.stderr: print(f"Limine bios-install stderr: {result.stderr}")
    else:
        print("Limine tool not found at expected path. Manual limine-bios-install required.")
    
    print(f"\nBoot command: qemu-system-i386 -drive file={output_path} -serial file:ser64.log -display none -cpu max")


if __name__ == '__main__':
    main()
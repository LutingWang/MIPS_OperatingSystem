/*
 * operations on IDE disk.
 */

#include "fs.h"
#include "lib.h"
#include <mmu.h>

// Overview:
//      read data from IDE disk. First issue a read request through
//      disk register and then copy data from disk buffer
//      (512 bytes, a sector) to destination array.
//
// Parameters:
//      diskno: disk number.
//      secno: start sector number.
//      dst: destination for data read from IDE disk.
//      nsecs: the number of sectors to read.
//
// Post-Condition:
//      If error occurred during read the IDE disk, panic.
//
// Hint: use syscalls to access device registers and buffers
void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs) {
        // 0x200: the size of a sector: 512 bytes.
        int offset_begin = secno * 0x200;
        int offset_end = offset_begin + nsecs * 0x200;
        int offset = 0;
        int temp;

        while (offset_begin + offset < offset_end) {
                syscall_write_dev((u_int) &diskno, 0x13000010, 4);
                temp = offset_begin + offset;
                syscall_write_dev((u_int) &temp, 0x13000000, 4);
                temp = 0;
                syscall_write_dev((u_int) &temp, 0x13000020, 1);
                syscall_read_dev((u_int) &temp, 0x13000030, 4);
                if (!temp) {
                        user_panic("ide_read@ide.c: error occurred during reading the ide disk\n");
                }
                if (syscall_read_dev((u_int) dst+offset, 0x13004000, 0x200)) {
                        user_panic("ide_read@ide.c: error occurred during reading disk buffer\n");
                }
                offset += 0x200;
        }
}

// Overview:
//      write data to IDE disk.
//
// Parameters:
//      diskno: disk number.
//      secno: start sector number.
//      src: the source data to write into IDE disk.
//      nsecs: the number of sectors to write.
//
// Post-Condition:
//      If error occurred during read the IDE disk, panic.
//
// Hint: use syscalls to access device registers and buffers
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs) {
        // 0x200: the size of a sector: 512 bytes.
        int offset_begin = secno * 0x200;
        int offset_end = offset_begin + nsecs * 0x200;
        int offset = 0;
        writef("diskno: %d\n", diskno);
        int temp;

        while (offset_begin + offset < offset_end) {
                if (syscall_write_dev((u_int) src+offset, 0x13004000, 0x200)) {
                        user_panic("ide_write@ide.c: error occurred during writing disk buffer\n");
                }
                syscall_write_dev((u_int) &diskno, 0x13000010, 4);
                temp = offset_begin + offset;
                syscall_write_dev((u_int) &temp, 0x13000000, 4);
                temp = 1 << 24; // little end store
                syscall_write_dev((u_int) &temp, 0x13000020, 1);
                syscall_read_dev((u_int) &temp, 0x13000030, 4);
                if (!temp) {
                        user_panic("ide_write@ide.c: error occurred during writing the ide disk\n");
                }
                offset += 0x200;
        }
}



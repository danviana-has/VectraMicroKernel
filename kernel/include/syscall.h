#ifndef VECTRA_SYSCALL_H
#define VECTRA_SYSCALL_H

#include "types.h"
#include "idt.h"

#define SYS_EXIT       0
#define SYS_PRINT      1
#define SYS_GETCHAR    2
#define SYS_EXEC       3
#define SYS_IPC_SEND   4
#define SYS_IPC_RECV   5
#define SYS_LIST_APPS  6
#define SYS_SPEAKER       7
#define SYS_NVRAM         8
#define SYS_YIELD         9
#define SYS_VFS_MKDIR     10
#define SYS_VFS_READ_FILE 11
#define SYS_VFS_WRITE_FILE 12
#define SYS_VFS_LIST_DIR  13
#define SYS_VFS_STAT      14
#define SYS_NET_DOWNLOAD  15
#define SYS_NET_INFO      16
#define SYS_GET_CMDLINE   17
#define SYS_CONSOLE_CLEAR      18
#define SYS_CONSOLE_PUTC       19
#define SYS_CONSOLE_SET_CURSOR 20
#define SYS_CONSOLE_DRAW_ROW   21

void syscall_init(void);

#endif /* VECTRA_SYSCALL_H */

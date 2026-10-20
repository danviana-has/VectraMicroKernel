#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#ifndef NULL
#define NULL ((void*)0)
#endif

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

#define KEY_UP        0x101
#define KEY_DOWN      0x102
#define KEY_LEFT      0x103
#define KEY_RIGHT     0x104
#define KEY_HOME      0x105
#define KEY_END       0x106
#define KEY_NPAGE     0x107
#define KEY_PPAGE     0x108
#define KEY_IC        0x109
#define KEY_DC        0x10A

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall1(int num, int arg1) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1) : "memory");
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2) : "memory");
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

static inline void sys_exit(int code) {
    syscall1(SYS_EXIT, code);
}

static inline void sys_print(const char* str) {
    syscall1(SYS_PRINT, (int)str);
}

static inline int sys_getchar(int blocking) {
    return syscall1(SYS_GETCHAR, blocking);
}

static inline int sys_exec(const char* app, int blocking) {
    return syscall2(SYS_EXEC, (int)app, blocking);
}

static inline int sys_list_apps(void) {
    return syscall0(SYS_LIST_APPS);
}

static inline void sys_speaker(unsigned short freq, unsigned int duration) {
    syscall2(SYS_SPEAKER, (int)freq, (int)duration);
}

static inline unsigned char sys_nvram_read(unsigned char addr) {
    return (unsigned char)syscall2(SYS_NVRAM, 0, (int)addr);
}

static inline void sys_nvram_write(unsigned char val, unsigned char addr) {
    syscall3(SYS_NVRAM, 1, (int)addr, (int)val);
}

static inline void sys_yield(void) {
    syscall0(SYS_YIELD);
}

static inline int sys_mkdir(const char* path) {
    return syscall1(SYS_VFS_MKDIR, (int)path);
}

static inline int sys_read_file(const char* path, void* buffer, unsigned int max_size) {
    return syscall3(SYS_VFS_READ_FILE, (int)path, (int)buffer, (int)max_size);
}

static inline int sys_write_file(const char* path, const void* data, unsigned int size) {
    return syscall3(SYS_VFS_WRITE_FILE, (int)path, (int)data, (int)size);
}

static inline int sys_list_dir(const char* path) {
    return syscall1(SYS_VFS_LIST_DIR, (int)path);
}

static inline int sys_stat(const char* path) {
    return syscall1(SYS_VFS_STAT, (int)path);
}

static inline int sys_download_file(const char* url, const char* dest_path) {
    return syscall2(SYS_NET_DOWNLOAD, (int)url, (int)dest_path);
}

static inline int sys_net_info(char* out_ip_str) {
    return syscall1(SYS_NET_INFO, (int)out_ip_str);
}

static inline int sys_get_cmdline(char* out_buf, int max_len) {
    return syscall2(SYS_GET_CMDLINE, (int)out_buf, max_len);
}

static inline void sys_clear(void) {
    syscall0(SYS_CONSOLE_CLEAR);
}

static inline void sys_console_putc(unsigned char row, unsigned char col, unsigned int ch) {
    syscall3(SYS_CONSOLE_PUTC, (int)row, (int)col, (int)ch);
}

static inline void sys_console_set_cursor(unsigned int x, unsigned int y) {
    syscall2(SYS_CONSOLE_SET_CURSOR, (int)x, (int)y);
}

static inline void sys_console_draw_row(unsigned char row, const char* str, unsigned char attr) {
    syscall3(SYS_CONSOLE_DRAW_ROW, (int)row, (int)str, (int)attr);
}

#endif /* USER_SYSCALL_H */

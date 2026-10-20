#include "syscall.h"
#include "task.h"
#include "ipc.h"
#include "loader.h"
#include "vfs.h"
#include "drivers.h"
#include "http_client.h"
#include "netif_e1000.h"
#include <string.h>

void syscall_handler(registers_t* regs) {
    uint32_t syscall_num = regs->eax;

    switch (syscall_num) {
        case SYS_EXIT: {
            task_exit();
            break;
        }

        case SYS_PRINT: {
            const char* str = (const char*)regs->ebx;
            if (str) {
                kprint(str);
            }
            regs->eax = 0;
            break;
        }

        case SYS_GETCHAR: {
            bool blocking = (bool)regs->ebx;
            if (blocking) {
                while (1) {
                    if (keyboard_havechar()) {
                        int c = keyboard_getchar();
                        if (c > 0) {
                            regs->eax = (uint32_t)c;
                            break;
                        }
                    } else if (serial_havechar()) {
                        int c = serial_getchar();
                        if (c > 0) {
                            regs->eax = (uint32_t)c;
                            break;
                        }
                    } else {
                        task_yield();
                    }
                }
            } else {
                if (keyboard_havechar()) {
                    int c = keyboard_getchar();
                    regs->eax = (c > 0) ? (uint32_t)c : (uint32_t)-1;
                } else if (serial_havechar()) {
                    int c = serial_getchar();
                    regs->eax = (c > 0) ? (uint32_t)c : (uint32_t)-1;
                } else {
                    regs->eax = (uint32_t)-1;
                }
            }
            break;
        }

        case SYS_EXEC: {
            const char* app_name = (const char*)regs->ebx;
            bool blocking = (bool)regs->ecx;
            regs->eax = (uint32_t)bin_load_and_run(app_name, blocking);
            break;
        }

        case SYS_IPC_SEND: {
            uint32_t dest = regs->ebx;
            message_t* msg = (message_t*)regs->ecx;
            regs->eax = (uint32_t)ipc_send(dest, msg);
            break;
        }

        case SYS_IPC_RECV: {
            uint32_t src = regs->ebx;
            message_t* msg = (message_t*)regs->ecx;
            regs->eax = (uint32_t)ipc_receive(src, msg);
            break;
        }

        case SYS_LIST_APPS: {
            vfs_list_files();
            regs->eax = vfs_get_count();
            break;
        }

        case SYS_SPEAKER: {
            u16 freq = (u16)regs->ebx;
            unsigned int duration = (unsigned int)regs->ecx;
            speaker_tone(freq, duration);
            regs->eax = 0;
            break;
        }

        case SYS_NVRAM: {
            uint32_t op = regs->ebx; /* 0 = read, 1 = write */
            uint8_t addr = (uint8_t)regs->ecx;
            if (op == 0) {
                regs->eax = (uint32_t)nvram_read(addr);
            } else {
                uint8_t val = (uint8_t)regs->edx;
                nvram_write(val, addr);
                regs->eax = 0;
            }
            break;
        }

        case SYS_YIELD: {
            task_yield();
            regs->eax = 0;
            break;
        }

        case SYS_VFS_MKDIR: {
            const char* path = (const char*)regs->ebx;
            vfs_node_t* node = vfs_mkdir(path);
            regs->eax = node ? 0 : (uint32_t)-1;
            break;
        }

        case SYS_VFS_READ_FILE: {
            const char* path = (const char*)regs->ebx;
            void* buffer = (void*)regs->ecx;
            uint32_t max_size = regs->edx;
            regs->eax = (uint32_t)vfs_read_file(path, buffer, max_size);
            break;
        }

        case SYS_VFS_WRITE_FILE: {
            const char* path = (const char*)regs->ebx;
            const void* data = (const void*)regs->ecx;
            uint32_t size = regs->edx;
            regs->eax = (uint32_t)vfs_write_file(path, data, size);
            break;
        }

        case SYS_VFS_LIST_DIR: {
            const char* path = (const char*)regs->ebx;
            vfs_list_dir(path);
            regs->eax = 0;
            break;
        }

        case SYS_VFS_STAT: {
            const char* path = (const char*)regs->ebx;
            regs->eax = (uint32_t)vfs_stat(path);
            break;
        }

        case SYS_NET_DOWNLOAD: {
            const char* url = (const char*)regs->ebx;
            const char* dest_path = (const char*)regs->ecx;
            regs->eax = (uint32_t)http_download(url, dest_path);
            break;
        }

        case SYS_NET_INFO: {
            char* out_buf = (char*)regs->ebx;
            if (out_buf) {
                const char* ip = net_get_ip_str();
                strcpy(out_buf, ip);
            }
            regs->eax = net_has_ip() ? net_get_ip() : 0;
            break;
        }

        case SYS_GET_CMDLINE: {
            char* out_buf = (char*)regs->ebx;
            uint32_t max_len = regs->ecx;
            if (out_buf && max_len > 0) {
                const char* cmd = loader_get_last_cmdline();
                uint32_t i = 0;
                while (cmd[i] && i < max_len - 1) {
                    out_buf[i] = cmd[i];
                    i++;
                }
                out_buf[i] = '\0';
                regs->eax = i;
            } else {
                regs->eax = 0;
            }
            break;
        }

        case SYS_CONSOLE_CLEAR: {
            video_console_clear();
            serial_clear();
            regs->eax = 0;
            break;
        }

        case SYS_CONSOLE_PUTC: {
            u8 row = (u8)regs->ebx;
            u8 col = (u8)regs->ecx;
            unsigned int ch = (unsigned int)regs->edx;
            video_console_putc(row, col, ch);
            regs->eax = 0;
            break;
        }

        case SYS_CONSOLE_SET_CURSOR: {
            unsigned int x = (unsigned int)regs->ebx;
            unsigned int y = (unsigned int)regs->ecx;
            video_console_set_cursor(x, y);
            regs->eax = 0;
            break;
        }

        case SYS_CONSOLE_DRAW_ROW: {
            u8 row = (u8)regs->ebx;
            const char* str = (const char*)regs->ecx;
            uint8_t attr = (uint8_t)regs->edx;
            for (u8 col = 0; col < 80; col++) {
                char c = (str && *str) ? *str++ : ' ';
                video_console_putc(row, col, ((unsigned int)attr << 8) | (unsigned char)c);
            }
            regs->eax = 0;
            break;
        }

        default: {
            kprint("[SYSCALL] Chamada de sistema desconhecida: ");
            kprint_dec(syscall_num);
            kprint("\n");
            regs->eax = (uint32_t)-1;
            break;
        }
    }
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
}

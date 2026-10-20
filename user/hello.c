#include "include/syscall.h"

int main(void) {
    sys_print("\n>>> =============================================== <<<\n");
    sys_print(">>> Ola do aplicativo externo HELLO.BIN (Ring 3)!  <<<\n");
    sys_print(">>> Este binario foi lido do disco VFS Multiboot!  <<<\n");
    sys_print(">>> Privilegio de execucao: Usuario DPL=3          <<<\n");
    sys_print(">>> =============================================== <<<\n\n");

    /* Emite um som rapido de confirmacao */
    sys_speaker(1000, 100);

    return 0;
}

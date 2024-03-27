#include <stdio.h>

int main(int argc, char *argv[]) {
    // si no hay 3 argumentos (nombre del programa, directorio origen y directorio destino)
    // se imprime un mensaje de uso correcto y se termina el programa
    if (argc != 3) {
        printf("Uso: %s <directorio origen> <directorio destino>\n", argv[0]);
        return 1;
    }
}
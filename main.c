#include <stdio.h>      // Para printf, fgets.
#include <string.h>     // Para funciones de cadenas como strcmp, strncmp, strcspn
#include <stdlib.h>     // Para funciones generales como malloc, free
#include <dirent.h>     // Para trabajar con directorios (opendir, readdir, closedir)
#include <time.h>       // Para medir tiempos de ejecución con clock()
#include <sys/stat.h>   // Para obtener información de archivos (stat)
#include "filesystem.h"

// -------------------------------------------------------
//  - fs: puntero al sistema de archivos en memoria
//  - folder: ruta del directorio de donde cargar los archivos
// -------------------------------------------------------
void load_all_files(FileSystem* fs, const char* folder) {
    DIR* dir;
    struct dirent* entry;
    clock_t start, end;
    int count = 0;

    // Abrir el directorio
    dir = opendir(folder);
    if (!dir) {
        printf("Error abrir dir %s\n", folder);
        return;
    }

    printf("[INFO] Loading from %s\n", folder);
    start = clock(); // Inicia el conteo de tiempo

    // Recorrer todos los archivos dentro del directorio
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar "." y ".." (entradas especiales)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name,"..") == 0) continue;

        // Construir la ruta completa al archivo
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", folder, entry->d_name);

        // Crear el archivo dentro del sistema de archivos virtual
        if (fs_create(fs, filepath)) count++;
    }

    end = clock();
    closedir(dir); // Cerrar directorio

    // Calcular el tiempo total de carga
    double total = (double)(end - start) / CLOCKS_PER_SEC;

    // Mostrar tamaño actual del archivo de almacenamiento
    struct stat st;
    if (stat(fs->storage_file, &st) == 0)
        printf("[INFO] storage size %lld bytes\n", (long long)st.st_size);

    // Mostrar resumen
    printf("[INFO] Loaded %d files in %.3f s\n", count, total);
}

int main() {
    FileSystem fs;          // Estructura del sistema de archivos
    char command[512];      // Buffer para leer comandos del usuario

    // Inicializa el sistema de archivos usando "storage.bin" como almacenamiento
    fs_init(&fs, "storage.bin");

    // Bucle principal de comandos
    while (1) {
        printf("> "); // Prompt de comando

        // Leer línea de entrada
        if (!fgets(command, sizeof(command), stdin)) break;

        // Eliminar salto de línea al final
        command[strcspn(command, "\n")] = 0;

        // ----------------- INTERPRETACIÓN DE COMANDOS -----------------

        if (strncmp(command, "init", 4) == 0) {
            fs_init(&fs, "storage.bin"); // Reinicia el sistema de archivos
        }
        else if (strncmp(command, "create ", 7) == 0) {
            fs_create(&fs, command + 7); // Crea archivo con la ruta indicada
        }
        else if (strncmp(command, "read ", 5) == 0) {
            fs_read(&fs, command + 5);   // Lee archivo desde el sistema de archivos
        }
        else if (strncmp(command, "delete ", 7) == 0) {
            fs_delete(&fs, command + 7); // Elimina archivo
        }
        else if (strncmp(command, "list", 4) == 0) {
            fs_list(&fs);                // Lista archivos almacenados
        }
        else if (strncmp(command, "save ", 5) == 0) {
            fs_save(&fs, command + 5);   // Guarda archivo del sistema virtual al sistema real
        }
        else if (strncmp(command, "load ", 5) == 0) {
            fs_load(&fs, command + 5);   // Carga un archivo del sistema real al virtual
        }
        else if (strncmp(command, "loadall ", 8) == 0) {
            load_all_files(&fs, command + 8); // Carga todos los archivos de una carpeta
        }
        else if (strcmp(command, "exit") == 0) {
            break; // Salir del programa
        }
        else {
            printf("Comando no reconocido.\n");
        }
    }

    return 0;
}


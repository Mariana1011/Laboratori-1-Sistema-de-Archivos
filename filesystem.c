#include "filesystem.h"
#include "compression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Estructura de metadatos para cada archivo
typedef struct {
    char name[256];      // Nombre del archivo
    long position;       // Posición dentro de storage.bin
    size_t comp_size;    // Tamaño del archivo comprimido
} MetaEntry;

// --------------------------------------------------------
// Inicializa el sistema de archivos
// --------------------------------------------------------
void fs_init(FileSystem* fs, const char* storage_name) {
    fs->index = btree_create(); // Crea un B-tree vacío
    strncpy(fs->storage_file, storage_name, sizeof(fs->storage_file)-1);
    fs->storage_file[sizeof(fs->storage_file)-1] = '\0';

    // Asegura que el archivo de almacenamiento exista
    FILE *f = fopen(fs->storage_file, "ab");
    if (f) fclose(f);

    printf("Inicializado: %s\n", fs->storage_file);
}

// --------------------------------------------------------
// Crea (guarda) un archivo en el sistema de archivos virtual
// --------------------------------------------------------
bool fs_create(FileSystem* fs, const char* filename) {
    FILE *src = fopen(filename, "rb");
    if (!src) {
        printf("Error: no se pudo abrir %s\n", filename);
        return false;
    }

    // Leer archivo original
    fseek(src, 0, SEEK_END);
    size_t orig_size = ftell(src);
    fseek(src, 0, SEEK_SET);
    uint8_t *buf = malloc(orig_size);
    if (!buf) { fclose(src); return false; }
    fread(buf, 1, orig_size, src);
    fclose(src);

    // Comprimir con LZW
    size_t comp_size = 0;
    uint8_t *comp = lzw_compress(buf, orig_size, &comp_size);
    free(buf);
    if (!comp) {
        printf("Error: compresión fallida\n");
        return false;
    }

    // Guardar en storage.bin
    FILE *storage = fopen(fs->storage_file, "ab");
    if (!storage) {
        free(comp);
        printf("Error: no se pudo abrir almacenamiento\n");
        return false;
    }
    long pos = ftell(storage); // Posición inicial
    fwrite(&comp_size, sizeof(size_t), 1, storage); // Tamaño comprimido
    fwrite(comp, 1, comp_size, storage);            // Datos comprimidos
    fclose(storage);
    free(comp);

    // Insertar en el índice (B-tree)
    btree_insert(&fs->index, filename, pos);

    printf("Guardado '%s' (orig %zu bytes -> comp %zu bytes)\n",
           filename, orig_size, comp_size);
    return true;
}

// --------------------------------------------------------
// Lee (muestra) un archivo del sistema virtual
// --------------------------------------------------------
bool fs_read(FileSystem* fs, const char* filename) {
    // Buscar posición en el índice
    long pos = btree_search(fs->index, filename);
    if (pos == -1) {
        printf("Error: '%s' no encontrado\n", filename);
        return false;
    }

    FILE *storage = fopen(fs->storage_file, "rb");
    if (!storage) {
        printf("Error: no se pudo abrir almacenamiento\n");
        return false;
    }
    fseek(storage, pos, SEEK_SET);

    // Lee tamaño comprimido y datos
    size_t comp_size;
    fread(&comp_size, sizeof(size_t), 1, storage);
    uint8_t *comp = malloc(comp_size);
    fread(comp, 1, comp_size, storage);
    fclose(storage);

    // Descomprimir con LZW
    size_t orig_size;
    uint8_t *orig = lzw_decompress(comp, comp_size, &orig_size);
    free(comp);
    if (!orig) {
        printf("Error: descompresion fallida\n");
        return false;
    }

    // Mostrar contenido
    printf("Contenido de '%s' (%zu bytes):\n", filename, orig_size);
    fwrite(orig, 1, orig_size, stdout);
    printf("\n---\n");
    free(orig);
    return true;
}

// --------------------------------------------------------
// Elimina un archivo del índice (pero no del storage.bin)
// --------------------------------------------------------
bool fs_delete(FileSystem* fs, const char* filename) {
    btree_delete(&fs->index, filename);
    printf("Eliminado '%s' del indice \n", filename);
    return true;
}

// --------------------------------------------------------
// Lista todos los archivos almacenados (según el índice)
// --------------------------------------------------------
void fs_list(FileSystem* fs) {
    printf("Archivos en el sistema:\n");
    btree_list(fs->index);
}

// --------------------------------------------------------
// Función auxiliar para guardar metadatos del B-tree
// --------------------------------------------------------
static void save_btree_recursive(BTreeNode* node, FILE* meta, FILE* storage) {
    if (!node) return;
    int i;
    for (i = 0; i < node->n; i++) {
        if (!node->leaf) save_btree_recursive(node->C[i], meta, storage);

        // Lee tamaño comprimido desde el storage
        size_t comp_size = 0;
        fseek(storage, node->positions[i], SEEK_SET);
        fread(&comp_size, sizeof(size_t), 1, storage);

        // Crea entrada de metadatos
        MetaEntry entry;
        strncpy(entry.name, node->keys[i], sizeof(entry.name)-1);
        entry.name[sizeof(entry.name)-1] = '\0';
        entry.position = node->positions[i];
        entry.comp_size = comp_size;

        fwrite(&entry, sizeof(MetaEntry), 1, meta);
    }
    if (!node->leaf) save_btree_recursive(node->C[i], meta, storage);
}

// --------------------------------------------------------
// Guarda el índice en un archivo .meta
// --------------------------------------------------------
bool fs_save(FileSystem* fs, const char* save_name) {
    char meta_file[512];
    snprintf(meta_file, sizeof(meta_file), "%s.meta", save_name);

    FILE *meta = fopen(meta_file, "wb");
    if (!meta) {
        printf("Error: no se pudo crear %s\n", meta_file);
        return false;
    }

    // Escribimos un contador temporal (0) que luego corregiremos
    uint32_t count = 0;
    fwrite(&count, sizeof(uint32_t), 1, meta);

    FILE *storage = fopen(fs->storage_file, "rb");
    if (!storage) {
        fclose(meta);
        printf("Error: no se pudo abrir almacenamiento\n");
        return false;
    }

    // Guardar las entradas recorriendo el B-tree
    long start_pos = ftell(meta);
    save_btree_recursive(fs->index, meta, storage);

    // Calcular número real de archivos guardados
    long end_pos = ftell(meta);
    count = (end_pos - start_pos) / sizeof(MetaEntry);

    // Reescribir contador al inicio
    fseek(meta, 0, SEEK_SET);
    fwrite(&count, sizeof(uint32_t), 1, meta);

    fclose(meta);
    fclose(storage);

    printf("Guardado en %s (%u archivos)\n", meta_file, count);
    return true;
}

// --------------------------------------------------------
// Carga el índice desde un archivo .meta
// --------------------------------------------------------
bool fs_load(FileSystem* fs, const char* load_name) {
    char meta_file[512];
    snprintf(meta_file, sizeof(meta_file), "%s.meta", load_name);

    FILE *meta = fopen(meta_file, "rb");
    if (!meta) {
        printf("Error: no se pudo abrir %s\n", meta_file);
        return false;
    }

    uint32_t count = 0;
    fread(&count, sizeof(uint32_t), 1, meta);

    fs->index = btree_create();

    // Insertar cada entrada en el B-tree
    for (uint32_t i = 0; i < count; i++) {
        MetaEntry entry;
        fread(&entry, sizeof(MetaEntry), 1, meta);
        btree_insert(&fs->index, entry.name, entry.position);
    }

    fclose(meta);

    printf("Cargado metadata desde %s (%u archivos)\n", meta_file, count);
    return true;
}

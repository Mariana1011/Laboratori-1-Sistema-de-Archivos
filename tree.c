#include "tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Función auxiliar: número máximo de llaves que puede tener un nodo
static int max_keys() { return 2 * BTREE_T - 1; }

// Función auxiliar: número máximo de hijos que puede tener un nodo
static int max_children() { return 2 * BTREE_T; }

// Asigna memoria para un nuevo nodo BTree
static BTreeNode* allocate_node(bool leaf) {
    BTreeNode* node = malloc(sizeof(BTreeNode));
    node->n = 0;               // Inicialmente sin llaves
    node->leaf = leaf;         // Si es hoja o no
    node->keys = malloc(sizeof(*node->keys) * max_keys());       // Arreglo de llaves
    node->positions = malloc(sizeof(long) * max_keys());         // Posiciones asociadas a las llaves
    node->C = malloc(sizeof(BTreeNode*) * max_children());       // Punteros a hijos
    for (int i = 0; i < max_children(); ++i) node->C[i] = NULL;  // Inicializa hijos como NULL
    return node;
}

// Crear un nuevo árbol B vacío (raíz hoja)
BTreeNode* btree_create() {
    return allocate_node(true);
}

// Buscar una clave en el árbol, devuelve su posición o -1 si no existe
long btree_search(BTreeNode* x, const char* key) {
    if (!x) return -1;
    int i = 0;
    // Avanzar hasta encontrar clave >= key
    while (i < x->n && strcmp(key, x->keys[i]) > 0) i++;

    // Si la encontramos, devolver su posición
    if (i < x->n && strcmp(key, x->keys[i]) == 0)
        return x->positions[i];

    // Si es hoja y no está, no existe
    if (x->leaf)
        return -1;

    // Buscar recursivamente en el hijo correspondiente
    return btree_search(x->C[i], key);
}

// Divide un hijo lleno en dos nodos y sube la clave del medio al padre
void btree_split_child(BTreeNode* x, int i, BTreeNode* y) {
    BTreeNode* z = allocate_node(y->leaf); // Nuevo nodo que recibirá la mitad de las llaves
    int t = BTREE_T;

    // Copiar las llaves superiores de y a z
    z->n = t - 1;
    for (int j = 0; j < t - 1; ++j) {
        strcpy(z->keys[j], y->keys[j + t]);
        z->positions[j] = y->positions[j + t];
    }

    // Copiar hijos si no es hoja
    if (!y->leaf) {
        for (int j = 0; j < t; ++j) {
            z->C[j] = y->C[j + t];
            y->C[j + t] = NULL;
        }
    }

    // Reducir número de llaves
    y->n = t - 1;

    // Mover punteros de hijos en x para insertar z
    for (int j = x->n; j >= i + 1; --j)
        x->C[j + 1] = x->C[j];
    x->C[i + 1] = z;

    // Mover llaves en x para insertar la clave del medio
    for (int j = x->n - 1; j >= i; --j) {
        strcpy(x->keys[j + 1], x->keys[j]);
        x->positions[j + 1] = x->positions[j];
    }

    // Insertar clave del medio de y en x
    strcpy(x->keys[i], y->keys[t - 1]);
    x->positions[i] = y->positions[t - 1];
    x->n += 1;
}

// Inserta una clave en un nodo no lleno
void btree_insert_nonfull(BTreeNode* x, const char* k, long pos) {
    int i = x->n - 1;

    if (x->leaf) {
        // Mover llaves hacia la derecha hasta encontrar lugar
        while (i >= 0 && strcmp(k, x->keys[i]) < 0) {
            strcpy(x->keys[i + 1], x->keys[i]);
            x->positions[i + 1] = x->positions[i];
            i--;
        }
        // Insertar nueva clave
        strcpy(x->keys[i + 1], k);
        x->positions[i + 1] = pos;
        x->n += 1;
    } else {
        // Encontrar hijo donde insertar
        while (i >= 0 && strcmp(k, x->keys[i]) < 0) i--;
        i++;

        // Si el hijo está lleno, dividirlo
        if (x->C[i]->n == max_keys()) {
            btree_split_child(x, i, x->C[i]);
            if (strcmp(k, x->keys[i]) > 0) i++;
        }
        // Insertar en el hijo correspondiente
        btree_insert_nonfull(x->C[i], k, pos);
    }
}

// Inserta una clave en el árbol (con manejo de raíz llena y actualización de valores)
void btree_insert(BTreeNode** root_ref, const char* key, long position) {
    BTreeNode* r = *root_ref;

    // Si la clave ya existe, solo actualiza su posición
    if (btree_search(r, key) != -1) {
        BTreeNode* node = r;
        int i = 0;
        while (1) {
            while (i < node->n && strcmp(key, node->keys[i]) > 0) i++;
            if (i < node->n && strcmp(key, node->keys[i]) == 0) {
                node->positions[i] = position;
                return;
            }
            if (node->leaf) break;
            node = node->C[i];
            i = 0;
        }
        return;
    }

    // Si la raíz está llena, dividir y crear nueva raíz
    if (r->n == max_keys()) {
        BTreeNode* s = allocate_node(false);
        *root_ref = s;
        s->C[0] = r;
        btree_split_child(s, 0, r);
        btree_insert_nonfull(s, key, position);
    } else {
        btree_insert_nonfull(r, key, position);
    }
}

// Elimina una clave del árbol (versión simple, solo en hojas)
void btree_delete(BTreeNode** root_ref, const char* key) {
    BTreeNode* node = *root_ref;
    while (node) {
        int i = 0;
        while (i < node->n && strcmp(key, node->keys[i]) > 0) i++;

        // Si la encontramos, eliminarla desplazando las demás
        if (i < node->n && strcmp(key, node->keys[i]) == 0) {
            for (int j = i; j < node->n - 1; ++j) {
                strcpy(node->keys[j], node->keys[j + 1]);
                node->positions[j] = node->positions[j + 1];
            }
            node->n -= 1;
            return;
        }

        // Si es hoja y no está, no se elimina
        if (node->leaf) break;

        // Continuar en el hijo correspondiente
        node = node->C[i];
    }
}

// Lista todas las claves del árbol en orden ascendente
void btree_list(BTreeNode* x) {
    if (!x) return;
    int i;
    for (i = 0; i < x->n; ++i) {
        if (!x->leaf) btree_list(x->C[i]);
        printf("- %s\n", x->keys[i]);
    }
    if (!x->leaf) btree_list(x->C[i]);
}

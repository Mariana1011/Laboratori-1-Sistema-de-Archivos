#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "tree.h"
#include <stdbool.h>
typedef struct { BTreeNode* index; char storage_file[512]; } FileSystem;
void fs_init(FileSystem* fs, const char* storage_name);
bool fs_create(FileSystem* fs, const char* filename);
bool fs_read(FileSystem* fs, const char* filename);
bool fs_delete(FileSystem* fs, const char* filename);
void fs_list(FileSystem* fs);
bool fs_save(FileSystem* fs, const char* save_name);
bool fs_load(FileSystem* fs, const char* load_name);
#endif

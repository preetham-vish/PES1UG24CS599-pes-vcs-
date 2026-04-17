// tree.c — Tree object serialization and construction
//
// PROVIDED functions: get_file_mode, tree_parse, tree_serialize
// TODO functions:     tree_from_index
//
// Binary tree format (per entry, concatenated with no separators):
//   "<mode-as-ascii-octal> <name>\0<32-byte-binary-hash>"
//
// Example single entry (conceptual):
//   "100644 hello.txt\0" followed by 32 raw bytes of SHA-256

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "index.h"

// ─── Mode Constants ─────────────────────────────────────────────────────────

#define MODE_FILE      0100644
#define MODE_EXEC      0100755
#define MODE_DIR       0040000

// ─── PROVIDED ───────────────────────────────────────────────────────────────

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1; 

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;
        if (mode_len >= sizeof(mode_str)) return -1;
        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1; 

        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1; 

        size_t name_len = null_byte - ptr;
        if (name_len >= sizeof(entry->name)) return -1;
        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0'; 

        ptr = null_byte + 1; 

        if (ptr + HASH_SIZE > end) return -1; 
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }
    return 0;
}

static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name, ((const TreeEntry *)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 296; 
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted_tree = *tree;
    qsort(sorted_tree.entries, sorted_tree.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;
    for (int i = 0; i < sorted_tree.count; i++) {
        const TreeEntry *entry = &sorted_tree.entries[i];
        
        int written = sprintf((char *)buffer + offset, "%o %s", entry->mode, entry->name);
        offset += written + 1; 
        
        memcpy(buffer + offset, entry->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;
    return 0;
}

// ─── IMPLEMENTATIONS ──────────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

__attribute__((weak)) int index_load(Index *index) {
    if (index) index->count = 0;
    return 0;
}

static int write_tree_level(IndexEntry *entries, int count, int depth, ObjectID *id_out) {
    Tree tree;
    tree.count = 0;

    int i = 0;
    while (i < count) {
        char *slash = strchr(entries[i].path, '/');

        if (!slash) {
            TreeEntry *e = &tree.entries[tree.count++];
            e->mode = entries[i].mode;
            // Explicitly limit to 255 chars to silence the truncation warning
            snprintf(e->name, sizeof(e->name), "%.255s", entries[i].path);
            memcpy(e->hash.hash, entries[i].hash.hash, HASH_SIZE);
            i++;
        } else {
            size_t dir_len = slash - entries[i].path;
            char dirname[256] = {0};
            snprintf(dirname, sizeof(dirname), "%.*s", (int)dir_len, entries[i].path);

            int j = i;
            while (j < count) {
                if (strncmp(entries[j].path, dirname, dir_len) == 0 && entries[j].path[dir_len] == '/') {
                    j++;
                } else break;
            }

            IndexEntry *sub = malloc((j - i) * sizeof(IndexEntry));
            if (!sub) return -1;
            
            for (int k = i; k < j; k++) {
                sub[k-i] = entries[k];
                memmove(sub[k-i].path, entries[k].path + dir_len + 1, strlen(entries[k].path) - dir_len);
            }

            ObjectID sub_id;
            if (write_tree_level(sub, j - i, depth + 1, &sub_id) != 0) {
                free(sub);
                return -1;
            }
            free(sub);

            TreeEntry *e = &tree.entries[tree.count++];
            e->mode = 0040000;
            snprintf(e->name, sizeof(e->name), "%.255s", dirname);
            e->hash = sub_id;

            i = j;
        }
    }

    void *tree_data;
    size_t tree_len;
    if (tree_serialize(&tree, &tree_data, &tree_len) != 0) return -1;
    int rc = object_write(OBJ_TREE, tree_data, tree_len, id_out);
    free(tree_data);
    return rc;
}

int tree_from_index(ObjectID *id_out) {
    Index index;
    index.count = 0;
    if (index_load(&index) != 0) return -1;
    if (index.count == 0) return -1;  

    return write_tree_level(index.entries, index.count, 0, id_out);
}

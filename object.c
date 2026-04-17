#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>

#ifndef OBJECTS_DIR
#define OBJECTS_DIR ".pes/objects"
#endif

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    SHA256((const unsigned char *)data, len, id_out->hash);
}

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + (i * 2), "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int val;
        if (sscanf(hex + (i * 2), "%2x", &val) != 1) return -1;
        id_out->hash[i] = (unsigned char)val;
    }
    return 0;
}

void object_path(const ObjectID *id, char *path_out, size_t size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    const char *type_str = "";
    if (type == OBJ_BLOB) type_str = "blob";
    else if (type == OBJ_TREE) type_str = "tree";
    else if (type == OBJ_COMMIT) type_str = "commit";
    else return -1;

    char header[128];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, len) + 1;

    size_t total_len = header_len + len;
    uint8_t *full_data = malloc(total_len);
    if (!full_data) return -1;

    memcpy(full_data, header, header_len);
    memcpy(full_data + header_len, data, len);

    compute_hash(full_data, total_len, id_out);

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id_out, hex);

    char shard_dir[256];
    snprintf(shard_dir, sizeof(shard_dir), "%s/%.2s", OBJECTS_DIR, hex);
    mkdir(shard_dir, 0755);

    char final_path[512];
    object_path(id_out, final_path, sizeof(final_path));

    if (access(final_path, F_OK) == 0) {
        free(full_data);
        return 0; 
    }

    char tmp_path[600];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", final_path);

    // CHANGED HERE: 0644 instead of 0444 so the test script can corrupt it
    int fd = open(tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(full_data);
        return -1;
    }

    if (write(fd, full_data, total_len) != (ssize_t)total_len) {
        close(fd);
        free(full_data);
        return -1;
    }

    fsync(fd);
    close(fd);

    if (rename(tmp_path, final_path) != 0) {
        free(full_data);
        return -1;
    }

    free(full_data);
    return 0;
}

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    char path[512];
    object_path(id, path, sizeof(path));

    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -1;
    }

    size_t file_size = st.st_size;
    uint8_t *buf = malloc(file_size);
    if (!buf) {
        close(fd);
        return -1;
    }

    if (read(fd, buf, file_size) != (ssize_t)file_size) {
        free(buf);
        close(fd);
        return -1;
    }
    close(fd);

    ObjectID computed_id;
    compute_hash(buf, file_size, &computed_id);
    if (memcmp(id->hash, computed_id.hash, HASH_SIZE) != 0) {
        free(buf);
        return -1;
    }

    uint8_t *null_pos = memchr(buf, '\0', file_size);
    if (!null_pos) {
        free(buf);
        return -1;
    }

    if (strncmp((char *)buf, "blob ", 5) == 0) *type_out = OBJ_BLOB;
    else if (strncmp((char *)buf, "tree ", 5) == 0) *type_out = OBJ_TREE;
    else if (strncmp((char *)buf, "commit ", 7) == 0) *type_out = OBJ_COMMIT;
    else {
        free(buf);
        return -1;
    }

    size_t header_len = (null_pos - buf) + 1;
    *len_out = file_size - header_len;

    *data_out = malloc(*len_out + 1);
    if (!*data_out) {
        free(buf);
        return -1;
    }

    memcpy(*data_out, buf + header_len, *len_out);
    ((char *)*data_out)[*len_out] = '\0'; 

    free(buf);
    return 0;
}

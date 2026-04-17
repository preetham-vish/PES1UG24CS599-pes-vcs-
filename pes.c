#include "pes.h"
#include "index.h"
#include "commit.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int cmd_init(int argc, char **argv) {
    mkdir(".pes", 0755);
    mkdir(".pes/objects", 0755);
    mkdir(".pes/refs", 0755);
    mkdir(".pes/refs/heads", 0755);
    FILE *f = fopen(".pes/HEAD", "w");
    if (f) { fprintf(f, "ref: refs/heads/main\n"); fclose(f); }
    printf("Initialized empty PES repository\n");
    return 0;
}

int cmd_add(int argc, char **argv) {
    Index idx;
    index_load(&idx);
    for (int i = 1; i < argc; i++) index_add(&idx, argv[i]);
    return 0;
}

int cmd_commit(int argc, char **argv) {
    const char *msg = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) msg = argv[i+1];
    }
    if (!msg) return -1;
    ObjectID id;
    if (commit_create(msg, &id) == 0) {
        char hex[HASH_HEX_SIZE+1];
        hash_to_hex(&id, hex);
        printf("Committed: %.7s\n", hex);
        return 0;
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    if (strcmp(argv[1], "init") == 0) return cmd_init(argc-1, argv+1);
    if (strcmp(argv[1], "add") == 0) return cmd_add(argc-1, argv+1);
    if (strcmp(argv[1], "commit") == 0) return cmd_commit(argc-1, argv+1);
    return 0;
}

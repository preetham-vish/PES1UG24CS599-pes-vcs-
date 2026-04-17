#include "pes.h"
#include "index.h"
#include "commit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h> // FIX: Added to resolve implicit declaration of ctime

// ─── CALLBACK ───────────────────────────────────────────────────────────────

// FIX: Changed return type to void to match commit_walk_fn in commit.h
void log_print_callback(const ObjectID *id, const Commit *c, void *ctx) {
    (void)ctx;
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);

    printf("\033[33mcommit %s\033[0m\n", hex); 
    printf("Author: %s\n", c->author);
    
    time_t t = (time_t)c->timestamp;
    printf("Date:   %s", ctime(&t)); 
    
    printf("\n    %s\n\n", c->message);
}

// ─── COMMAND HANDLERS ───────────────────────────────────────────────────────

int cmd_init(int argc, char **argv) {
    (void)argc; (void)argv;
    if (mkdir(".pes", 0755) != 0) {}
    mkdir(".pes/objects", 0755);
    mkdir(".pes/refs", 0755);
    mkdir(".pes/refs/heads", 0755);

    FILE *f = fopen(".pes/HEAD", "w");
    if (f) {
        fprintf(f, "ref: refs/heads/main\n");
        fclose(f);
    }
    printf("Initialized empty PES repository in .pes/\n");
    return 0;
}

int cmd_add(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pes add <file>...\n");
        return -1;
    }
    Index index;
    if (index_load(&index) != 0) return -1;
    for (int i = 1; i < argc; i++) {
        if (index_add(&index, argv[i]) != 0) {
            fprintf(stderr, "error: failed to add '%s'\n", argv[i]);
        }
    }
    return 0;
}

int cmd_status(int argc, char **argv) {
    (void)argc; (void)argv;
    Index index;
    if (index_load(&index) != 0) return -1;
    return index_status(&index);
}

int cmd_commit(int argc, char **argv) {
    const char *message = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && (i + 1) < argc) {
            message = argv[i + 1];
            break;
        }
    }
    if (!message) {
        fprintf(stderr, "error: commit requires a message (-m \"message\")\n");
        return -1;
    }
    ObjectID commit_id;
    if (commit_create(message, &commit_id) != 0) return -1;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_id, hex);
    printf("Committed: %.12s... %s\n", hex, message);
    return 0;
}

int cmd_log(int argc, char **argv) {
    (void)argc; (void)argv;
    return commit_walk(log_print_callback, NULL);
}

// ─── MAIN ───────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pes <command> [<args>]\n");
        return 1;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "init") == 0) return cmd_init(argc - 1, argv + 1);
    if (strcmp(cmd, "add") == 0) return cmd_add(argc - 1, argv + 1);
    if (strcmp(cmd, "status") == 0) return cmd_status(argc - 1, argv + 1);
    if (strcmp(cmd, "commit") == 0) return cmd_commit(argc - 1, argv + 1);
    if (strcmp(cmd, "log") == 0) return cmd_log(argc - 1, argv + 1);
    fprintf(stderr, "unknown command: %s\n", cmd);
    return 1;
}

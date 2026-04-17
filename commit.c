#include "commit.h"
#include "tree.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int commit_create(const char *message, ObjectID *commit_id_out) {
    ObjectID tree_id;
    if (tree_from_index(&tree_id) != 0) return -1;

    Commit c;
    memset(&c, 0, sizeof(Commit));
    c.tree = tree_id;
    c.timestamp = (uint64_t)time(NULL);
    strncpy(c.author, pes_author(), sizeof(c.author)-1);
    strncpy(c.message, message, sizeof(c.message)-1);

    if (head_read(&c.parent) == 0) {
        c.has_parent = 1;
    }

    void *data; size_t len;
    if (commit_serialize(&c, &data, &len) != 0) return -1;

    ObjectID cid;
    if (object_write(OBJ_COMMIT, data, len, &cid) != 0) {
        free(data);
        return -1;
    }
    free(data);

    if (head_update(&cid) != 0) return -1;
    if (commit_id_out) *commit_id_out = cid;
    return 0;
}

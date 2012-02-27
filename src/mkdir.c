#include <droplet.h>
#include <glib.h>
#include <errno.h>

#include "mkdir.h"
#include "log.h"
#include "timeout.h"
#include "hash.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_mkdir(const char *path,
          mode_t mode)
{
        dpl_status_t rc;
        int ret;
        tpath_entry *pe = NULL;
        char *key = NULL;

        LOG(LOG_DEBUG, "path=%s, mode=0x%x", path, (int)mode);

        rc = dfs_mkdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_mkdir_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                if (-1 == populate_hash(hash, path, FILE_DIR, &pe)) {
                        LOG(LOG_ERR, "populate with path %s failed", path);
                        ret = -1;
                        goto err;
                }
                LOG(LOG_DEBUG, "added a new dir entry in hashtable: %s", path);
        }

        pe->filetype = FILE_DIR;

        ret = 0;
 err:
        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));
        return ret;
}

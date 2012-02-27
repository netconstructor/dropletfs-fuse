#include <glib.h>
#include <droplet.h>
#include <errno.h>

#include "log.h"
#include "rmdir.h"
#include "timeout.h"
#include "hash.h"
#include "tmpstr.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_rmdir(const char *path)
{
        dpl_status_t rc = DPL_FAILURE;
        int ret;
        tpath_entry *pe = NULL;
        char *local = NULL;

        LOG(LOG_DEBUG, "path=%s", path);

        rc = dfs_rmdir_timeout(ctx, path);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_rmdir_timeout: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        local = tmpstr_printf("%s/%s", conf->cache_dir, path);
        if (-1 == rmdir(local)) {
                LOG(LOG_INFO, "rmdir cache directory (%s): %s", local,
                    strerror(errno));
                /* fallback: continue */
        }

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                /* the fs should not get an error, but we have to log this
                 * incoherence! */
                LOG(LOG_ERR, "%s: entry not found in the hashtable", path);
                ret = 0;
                goto err;
        }

        (void)pentry_remove_dirent(pe, path);

        ret = 0;
 err:
        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));
        return ret;
}

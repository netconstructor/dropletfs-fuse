#include <glib.h>
#include <droplet.h>
#include <assert.h>
#include <errno.h>

#include "chmod.h"
#include "log.h"
#include "metadata.h"
#include "timeout.h"
#include "hash.h"

extern GHashTable *hash;
extern dpl_ctx_t *ctx;

int
dfs_chmod(const char *path,
          mode_t mode)
{
        dpl_status_t rc;
        int ret;
        tpath_entry *pe = NULL;
        time_t now;

        LOG(LOG_DEBUG, "%s", path);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_ERR, "path=%s no entry for in hashtable", path);
                ret = -1;
                goto err;
        }

        assert(NULL != pe->usermd);

        now = time(NULL);
        assign_meta_to_dict(pe->usermd, "mode", (unsigned long)mode);
        assign_meta_to_dict(pe->usermd, "mtime", (unsigned long)now);
        assign_meta_to_dict(pe->usermd, "ctime", (unsigned long)now);

        if (-1 != pe->fd && FILE_LOCAL == pe->ondisk) {
                /* change the cache file info */
                if (-1 == fchmod(pe->fd, mode)) {
                        if (EPERM != errno) {
                                LOG(LOG_ERR, "fchmod(fd=%d, mode=%d): %s (%d)",
                                    pe->fd, (int) mode, strerror(errno), errno);
                                ret = -1;
                                goto err;
                        }
                }
        }

        /* update metadata on the cloud */
        rc = dfs_setattr_timeout(ctx, path, pe->usermd);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dpl_setattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));
        return ret;
}

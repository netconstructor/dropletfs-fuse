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
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        int ret;
        pentry_t *pe = NULL;
        time_t now;
        int fd = -1;

        LOG(LOG_DEBUG, "%s", path);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_ERR, "path=%s no entry for in hashtable", path);
                ret = -1;
                goto err;
        }

        metadata = pentry_get_metadata(pe);

        assert(NULL != metadata);

        now = time(NULL);
        assign_meta_to_dict(metadata, "mode", (unsigned long)mode);
        assign_meta_to_dict(metadata, "mtime", (unsigned long)now);
        assign_meta_to_dict(metadata, "ctime", (unsigned long)now);

        fd = pentry_get_fd(pe);
        if (-1 != fd && FILE_LOCAL == pentry_get_placeholder(pe)) {
                /* change the cache file info */
                if (-1 == fchmod(fd, mode)) {
                        LOG(LOG_ERR, "fchmod(fd=%d): %s", fd, strerror(errno));
                        ret = -1;
                        goto err;
                }
        }

        /* update metadata on the cloud */
        rc = dfs_setattr_timeout(ctx, path, metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_setattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));
        return ret;
}

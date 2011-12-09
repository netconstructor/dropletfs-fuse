#include <glib.h>
#include <droplet.h>
#include <assert.h>
#include <errno.h>

#include "chown.h"
#include "log.h"
#include "metadata.h"
#include "timeout.h"
#include "tmpstr.h"
#include "hash.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_chown(const char *path,
          uid_t uid,
          gid_t gid)
{
        dpl_status_t rc;
        int ret;
        tpath_entry *pe = NULL;
        time_t now;

        LOG(LOG_DEBUG, "%s, uid=%lu, gid=%lu",
            path, (unsigned long)uid, (unsigned long)gid);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_ERR, "path=%s no entry for in hashtable", path);
                ret = -1;
                goto err;
        }

        assert(NULL != pe->usermd);

        now = time(NULL);

        pentry_md_lock(pe);
        assign_meta_to_dict(pe->usermd, "mtime", (unsigned long) now);
        assign_meta_to_dict(pe->usermd, "ctime", (unsigned long) now);
        assign_meta_to_dict(pe->usermd, "uid", (unsigned long) uid);
        assign_meta_to_dict(pe->usermd, "gid", (unsigned long) gid);
        pentry_md_unlock(pe);

        if (-1 != pe->fd && FILE_LOCAL == pe->ondisk) {
                /* change the cache file info */
                if (-1 == fchown(pe->fd, uid, gid)) {
                        if (EPERM != errno) {
                                LOG(LOG_ERR,
                                    "fchown(fd=%d, uid=%d, gid=%d): %s (%d)",
                                    pe->fd, (int) uid, (int) gid,
                                    strerror(errno), errno);
                                ret = -1;
                                goto err;
                        }
                }
        }

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

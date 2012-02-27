#include <glib.h>
#include <droplet.h>
#include <errno.h>

#include "opendir.h"
#include "log.h"
#include "timeout.h"
#include "hash.h"

extern dpl_ctx_t *ctx;
extern GHashTable *hash;

int
dfs_opendir(const char *path,
            struct fuse_file_info *info)
{
        dpl_status_t rc = DPL_FAILURE;
        tpath_entry *pe = NULL;
        int ret;
        char *key = NULL;

        LOG(LOG_DEBUG, "path=%s, info=%p", path, (void *)info);

        rc = dfs_opendir_timeout(ctx, path, (void *[]){NULL});
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                ret = rc;
                goto err;
        }

        ret = 0;
  err:

        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));
        return ret;
}

#include <droplet.h>

#include "rename.h"
#include "unlink.h"
#include "file.h"
#include "log.h"

extern dpl_ctx_t *ctx;

int
dfs_rename(const char *oldpath,
           const char *newpath)
{
        dpl_status_t rc;
        char *p = NULL;
        int ret = 0;

        LOG(LOG_DEBUG, "%s -> %s", oldpath, newpath);

        if (0 == strcmp(newpath, ".")) {
                p = strrchr(oldpath, '/');
                newpath = p ? p + 1 : oldpath;
        }

        rc = dpl_fcopy(ctx, (char *)oldpath, (char *)newpath);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_fcopy: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        ret = dfs_unlink(oldpath);
  err:
        return ret;
}

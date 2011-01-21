#include <droplet.h>

#include "opendir.h"
#include "log.h"

extern dpl_ctx_t *ctx;

int
dfs_opendir(const char *path,
            struct fuse_file_info *info)
{
        dpl_status_t rc = DPL_FAILURE;

        LOG(LOG_DEBUG, "path=%s, info=%p", path, (void *)info);

        rc = dpl_opendir(ctx, (char *)path, (void *[]){NULL});

        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_opendir: %s", dpl_status_str(rc));
                return rc;
        }

        return 0;
}

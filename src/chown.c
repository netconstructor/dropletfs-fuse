#include <glib.h>
#include <droplet.h>

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
        return 0;
        dpl_dict_t *metadata = NULL;
        dpl_status_t rc;
        int ret;
        pentry_t *pe = NULL;

        LOG(LOG_DEBUG, "%s, uid=%lu, gid=%lu",
            path, (unsigned long)uid, (unsigned long)gid);

        rc = dfs_getattr_timeout(ctx, path, &metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_getattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        if (! metadata) {
                metadata = dpl_dict_new(13);
                if (! metadata) {
                        LOG(LOG_ERR, "allocation failure");
                        ret = -1;
                        goto err;
                }
        }

        assign_meta_to_dict(metadata, "uid", (unsigned long)uid);
        assign_meta_to_dict(metadata, "gid", (unsigned long)gid);

        rc = dfs_setattr_timeout(ctx, path, metadata);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dpl_setattr: %s", dpl_status_str(rc));
                ret = -1;
                goto err;
        }

        pe = g_hash_table_lookup(hash, path);
        if (pe) {
                pentry_set_metadata(pe, metadata);
                pentry_set_atime(pe, time(NULL));
        }

        ret = 0;
  err:
        if (metadata)
                dpl_dict_free(metadata);

        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));

        return ret;
}

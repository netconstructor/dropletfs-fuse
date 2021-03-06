#include <unistd.h>
#include <errno.h>
#include <glib.h>

#include "fsync.h"
#include "hash.h"
#include "log.h"

extern GHashTable *hash;

int
dfs_fsync(const char *path,
          int issync,
          struct fuse_file_info *info)
{
        tpath_entry *pe = NULL;
        int ret;

        LOG(LOG_DEBUG, "%s", path);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_INFO, "unable to find a path entry (%s)", path);
                ret = -1;
                goto end;
        }

        if (-1 == pe->fd) {
                LOG(LOG_ERR, "unusable file descriptor: %d", pe->fd);
                ret = -1;
                goto end;
        }

        if (issync)
                goto end;

        if (-1 == fsync(pe->fd)) {
                LOG(LOG_ERR, "fsync(fd=%d): %s", pe->fd, strerror(errno));
                ret = -errno;
                goto end;
        }

        ret = 0;

  end:
        LOG(LOG_DEBUG, "path=%s ret=%s", path, dpl_status_str(ret));
        return ret;
}

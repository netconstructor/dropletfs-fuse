#include <errno.h>

#include "log.h"
#include "write.h"
#include "hash.h"

ssize_t pwrite(int, const void *, size_t, off_t);

int
dfs_write(const char *path,
          const char *buf,
          size_t size,
          off_t offset,
          struct fuse_file_info *info)
{
        tpath_entry *pe = NULL;
        int ret = 0;

        LOG(LOG_DEBUG, "path=%s, buf=%p, size=%zu, offset=%lld, info=%p",
            path, (void *)buf, size, (long long)offset, (void *)info);

        pe = (tpath_entry *)info->fh;

        if (pe->fd < 0) {
                LOG(LOG_ERR, "unusable file descriptor fd=%d", pe->fd);
                ret = EBADF;
                goto err;
        }

        ret = pwrite(pe->fd, buf, size, offset);
        if (-1 == ret) {
                LOG(LOG_ERR, "pwrite: %s", strerror(errno));
                ret = -errno;
                goto err;
        }

  err:
        LOG(LOG_DEBUG, "return value = %d", ret);
        return ret;
}

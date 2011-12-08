#include <errno.h>
#include <glib.h>
#include <libgen.h>

#include "open.h"
#include "misc.h"
#include "log.h"
#include "glob.h"
#include "file.h"
#include "tmpstr.h"

extern GHashTable *hash;
extern struct conf *conf;

enum state_mode {
        MODE_RDONLY,
        MODE_WRONLY,
        MODE_RDWR,
        MODE_CREAT,
        MODE_DEFAULT = MODE_RDWR,
};

static int open_flags[] = {
        [MODE_RDONLY] = O_RDONLY,
        [MODE_WRONLY] = O_RDWR,
        [MODE_RDWR]   = O_RDWR,
        [MODE_CREAT]  = O_RDWR|O_CREAT|O_TRUNC,
};

static int
populate_hash(GHashTable *h,
              const char * const path,
              tpath_entry **pep)
{
        int ret;
        char *key = NULL;
        tpath_entry *pe = NULL;

        if (! pep) {
                ret = -1;
                goto err;
        }

        pe = pentry_new();
        if (! pe) {
                ret = -1;
                goto err;
        }

        pe->fd = -1;
        pentry_set_path(pe, path);
        key = strdup(path);
        if (! key) {
                LOG(LOG_CRIT, "strdup(%s): %s", path, strerror(errno));
                pentry_free(pe);
                ret = -1;
                goto err;
        }

        *pep = pe;
        g_hash_table_insert(h, key, pe);

        ret = 0;
  err:
        return ret;
}

static char *
build_cache_tree(const char *path)
{
        LOG(LOG_DEBUG, "building cache dir for '%s'", path);

        char *local = NULL;
        char *tmp_local = NULL;
        char *dir = NULL;
        struct stat st;

        /* ignore the leading spaces */
        while (path && '/' == *path)
                path++;

        local = tmpstr_printf("%s/%s", conf->cache_dir, path);

        tmp_local = strdup(local);

        if (! tmp_local) {
                LOG(LOG_CRIT, "strdup(%s): %s", tmp_local, strerror(errno));
                return NULL;
        }

        dir = tmpstr_printf("%s", dirname(tmp_local));
        if (-1 == stat(dir, &st)) {
                if (ENOENT == errno)
                        mkdir_tree(dir);
                else
                        LOG(LOG_ERR, "stat(%s): %s", dir, strerror(errno));
        }

        free(tmp_local);

        return local;
}

static enum state_mode
get_mode_from_flags(int flags)
{
        if (flags & O_APPEND) return MODE_RDWR;
        if (flags & O_CREAT) return MODE_CREAT;
        if (O_RDONLY == (flags & O_ACCMODE)) return MODE_RDONLY;
        if (O_WRONLY == (flags & O_ACCMODE)) return MODE_CREAT;
        if (O_RDWR == (flags & O_ACCMODE)) return MODE_CREAT;

        return MODE_DEFAULT;
}

static int
open_creat(const char * const path,
           tpath_entry *pe,
           int flags)
{
        int ret;
        int fd;
        char *local = NULL;

        local = build_cache_tree(path);

        if (! local) {
                LOG(LOG_ERR, "can't create a cache local path (%s)", path);
                ret = -1;
                goto err;
        }

        LOG(LOG_INFO, "opening cache file '%s'", local);

        fd = open(local, flags, 0644);
        if (-1 == fd) {
                LOG(LOG_ERR, "%s: %s", local, strerror(errno));
                ret = -1;
                goto err;
        }
        pe->fd = fd;
        pe->flag = FLAG_DIRTY;

        ret = 0;
  err:
        return ret;
}

static int
open_existing(const char * const path,
              tpath_entry *pe,
              int flags)
{
        int ret;

        /* negative fd? then we don't have any cache file, get it! */
        if (pe->fd < 0) {
                (void) build_cache_tree(path);
                pe->fd = dfs_get_local_copy(pe, path, flags);
                if (-1 == pe->fd) {
                        ret = -1;
                        goto err;
                }
        }

        ret = 0;
  err:
        return ret;
}

static int
open_rdonly(const char * const path,
            tpath_entry *pe,
            int flags)
{
        return open_existing(path, pe, flags);
}

static int
open_wronly(const char * const path,
            tpath_entry *pe,
            int flags)
{
        int ret;

        ret = open_existing(path, pe, flags);
        if (0 == ret)
                pe->flag = FLAG_DIRTY;

        return ret;
}

static int
open_rdwr(const char * const path,
          tpath_entry *pe,
          int flags)
{
        return open_wronly(path, pe, flags);
}

int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        tpath_entry *pe = NULL;
        int ret = -1;
        enum state_mode smode;
        dpl_dict_t *usermd;

        info->fh = 0;
        LOG(LOG_DEBUG, "path=%s %s 0%o",
            path, flags_to_str(info->flags), info->flags);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG(LOG_INFO, "'%s': entry not found in hashtable", path);
                if (-1 == populate_hash(hash, path, &pe)) {
                        ret = -1;
                        goto err;
                }
                LOG(LOG_INFO, "adding file '%s' to the hashtable", path);
        } else {
                if (FILE_LOCAL == pe->ondisk)
                        LOG(LOG_INFO, "%s: found in the hashtable, and the "
                            "file is on disk (fd=%d)", path, pe->fd);
                else
                        LOG(LOG_INFO, "%s: found in hashtable, but the file "
                            "isn't downloaded (fd=%d)", path, pe->fd);
        }

        info->fh = (uint64_t)pe;
        pentry_inc_refcount(pe);

        smode = get_mode_from_flags(info->flags);

        info->fh = (uint64_t)pe;
        LOG(LOG_DEBUG, "path=%s, MODE=%d", path, smode);

        int (*fn[])(const char *, tpath_entry *, int) = {
                [MODE_RDONLY] = open_rdonly,
                [MODE_WRONLY] = open_wronly,
                [MODE_RDWR]   = open_rdwr,
                [MODE_CREAT]  = open_creat,
        };

        if (-1 == fn[smode](path, pe, open_flags[smode])) {
                ret = -1;
                goto err;
        }

        pe->ondisk = FILE_LOCAL;
        ret = 0;
  err:
        LOG(LOG_DEBUG, "@pentry=%p, fd=%d, flags=0x%X, ret=%d",
            pe, pe->fd, info->flags, ret);
        return ret;
}

#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <glib.h>

#include "file.h"
#include "log.h"
#include "hash.h"
#include "metadata.h"
#include "tmpstr.h"
#include "list.h"
#include "utils.h"

extern GHashTable *hash;
extern struct conf *conf;

tpath_entry *
pentry_new(void)
{
        tpath_entry *pe = NULL;
        pthread_mutexattr_t attr;
        pthread_mutexattr_t md_attr;
        int rc;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG(LOG_CRIT, "out of memory");
                return NULL;
        }

        rc = sem_init(&pe->refcount, 0, 0);
        if (-1 == rc) {
                LOG(LOG_INFO, "sem_init sem@%p: %s",
                    (void *)&pe->refcount, strerror(errno));
                goto release;
        }

        rc = pthread_mutexattr_init(&attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutexattr_init mutex@%p: %s",
                    (void *)&pe->mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutex_init(&pe->mutex, &attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutex_init mutex@%p %s",
                    (void *)&pe->mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutexattr_init(&md_attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutexattr_init mutex@%p: %s",
                    (void *)&pe->md_mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutex_init(&pe->md_mutex, &attr);
        if (rc) {
                LOG(LOG_INFO, "pthread_mutex_init mutex@%p %s",
                    (void *)&pe->md_mutex, strerror(rc));
                goto release;
        }

        pe->usermd = NULL;
        pe->ondisk = FILE_UNSET;
        pe->fd = -1;
        pe->dirent = NULL;
        pe->path = NULL;
        pe->exclude = 0;
        pe->flag = FLAG_DIRTY;

        return pe;

  release:
        pentry_free(pe);
        return NULL;
}

void
pentry_free(tpath_entry *pe)
{
        if (-1 != pe->fd)
                safe_close(pe->fd);

        if (pe->usermd)
                dpl_dict_free(pe->usermd);

        if (pe->path)
                free(pe->path);

        (void)pthread_mutex_destroy(&pe->mutex);
        (void)sem_destroy(&pe->refcount);

        list_free(pe->dirent);

        free(pe);
}

char *
pentry_placeholder_to_str(int flag)
{
        switch (flag) {
        case FILE_LOCAL: return "local";
        case FILE_REMOTE: return "remote";
        case FILE_UNSET: return "unset";
        }

        assert(! "impossible case");
        return "invalid";
}

static int
cb_compare_path(void *p1,
                void *p2)
{
        char *path1 = p1;
        char *path2 = p2;

        assert(p1);
        assert(p2);

        return ! strcmp(path1, path2);
}

int
pentry_remove_dirent(tpath_entry *pe,
                     const char *path)
{
        int ret;

        assert(pe);

        if (! path) {
                LOG(LOG_ERR, "NULL path");
                ret = -1;
                goto err;
        }

        if (FILE_DIR != pe->filetype) {
                LOG(LOG_ERR, "%s: is not a directory", path);
                ret = -1;
                goto err;
        }

        pe->dirent = list_remove(pe->dirent, (char *)path, cb_compare_path);

        ret = 0;
  err:
        return ret;
}

void
pentry_add_dirent(tpath_entry *pe,
                  const char *path)
{
        char *key = NULL;

        assert(pe);

        LOG(LOG_DEBUG, "path='%s', new entry: '%s'", pe->path, path);

        key = strdup(path);
        if (! key)
                LOG(LOG_ERR, "%s: strdup: %s", path, strerror(errno));
        else
                pe->dirent = list_add(pe->dirent, key);
}

void
pentry_unlink_cache_file(tpath_entry *pe)
{
        char *local = NULL;

        assert(pe);

        if (! pe->path)
                return;

        local = tmpstr_printf("%s/%s", conf->cache_dir, pe->path);

        if (-1 == unlink(local))
                LOG(LOG_INFO, "unlink(%s): %s", local, strerror(errno));
}

void
pentry_inc_refcount(tpath_entry *pe)
{
        assert(pe);


        if (-1 == sem_post(&pe->refcount))
                LOG(LOG_INFO, "path=%s, sem_post@%p: %s",
                    pe->path, (void *)&pe->refcount, strerror(errno));
}

void
pentry_dec_refcount(tpath_entry *pe)
{
        assert(pe);


        if (-1 == sem_wait(&pe->refcount))
                LOG(LOG_INFO, "path=%s, sem_wait@%p: %s",
                    pe->path, (void *)&pe->refcount, strerror(errno));
}

int
pentry_get_refcount(tpath_entry *pe)
{
        assert(pe);

        int ret;
        (void)sem_getvalue(&pe->refcount, &ret);

        return ret;
}

tpath_entry *
pentry_get_parent(tpath_entry *pe)
{
        tpath_entry *parent = NULL;
        char *path = NULL;
        char *p = NULL;
        char *dirname = NULL;

        /* sanity check */
        if (! pe) {
                LOG(LOG_ERR, "NULL entry, no parent");
                goto end;
        }

        LOG(LOG_DEBUG, "path=%s", pe->path);

        path = tmpstr_printf("%s", pe->path);

        p = strrchr(path, '/');
        if (! p) {
                LOG(LOG_ERR, "malformed path: %s", pe->path);
                goto end;
        }

        if (p == path)
                dirname = "/";
        else
                *p = 0;

        parent = g_hash_table_lookup(hash, dirname);

  end:
        LOG(LOG_DEBUG, "parent path=%s", parent ? parent->path : "null");

        return parent;
}

void
pentry_set_path(tpath_entry *pe,
                const char *path)
{
        assert(pe);

        if (! path) {
                LOG(LOG_ERR, "empty path");
                return;
        }

        if (pe->path)
                free(pe->path);

        pe->path = strdup(path);
        if (! pe->path)
                LOG(LOG_CRIT, "strdup(%s): %s", path, strerror(errno));
}

static int
pentry_gen_trylock(tpath_entry *pe,
                   pthread_mutex_t *lock)
{
        int ret;

        assert(pe);

        ret = pthread_mutex_trylock(lock);

        return ret;
}

static void
pentry_gen_lock(tpath_entry *pe,
                pthread_mutex_t *lock)
{
        int ret;

        assert(pe);

        ret = pthread_mutex_lock(lock);
        assert(0 == ret);
}

static void
pentry_gen_unlock(tpath_entry *pe,
                  pthread_mutex_t *lock)
{
        int ret;

        assert(pe);

        ret = pthread_mutex_unlock(lock);
        assert(0 == ret);
}

int
pentry_trylock(tpath_entry *pe)
{
        assert(pe);

        return pentry_gen_trylock(pe, &pe->mutex);
}

void
pentry_lock(tpath_entry *pe)
{
        assert(pe);

        pentry_gen_lock(pe, &pe->mutex);
}

void
pentry_unlock(tpath_entry *pe)
{
        assert(pe);

        pentry_gen_unlock(pe, &pe->mutex);
}

int
pentry_md_trylock(tpath_entry *pe)
{
        assert(pe);

        return pentry_gen_trylock(pe, &pe->md_mutex);
}

void
pentry_md_lock(tpath_entry *pe)
{
        assert(pe);

        pentry_gen_lock(pe, &pe->md_mutex);
}

void
pentry_md_unlock(tpath_entry *pe)
{
        assert(pe);

        pentry_gen_unlock(pe, &pe->md_mutex);
}

int
pentry_set_usermd(tpath_entry *pe,
                  dpl_dict_t *dict)
{
        int ret;

        assert(pe);

        if (pe->usermd)
                dpl_dict_free(pe->usermd);

        pe->usermd = dpl_dict_new(13);
        if (! pe->usermd) {
                LOG(LOG_ERR, "path=%s: dpl_dict_new: can't allocate memory",
                    pe->path);
                ret = -1;
                goto err;
        }

        if (DPL_FAILURE == dpl_dict_copy(pe->usermd, dict)) {
                LOG(LOG_ERR, "path=%s: dpl_dict_copy: failed", pe->path);
                ret = -1;
                goto err;
        }

        ret = 0;
  err:
        return ret;
}

int
pentry_set_digest(tpath_entry *pe,
                  const char *digest)
{
        assert(pe);

        if (! digest)
                return -1;

        memcpy(pe->digest, digest, sizeof pe->digest);
        return 0;
}

char *
pentry_type_to_str(filetype_t type)
{
        switch (type) {
        case FILE_REG: return "regular file";
        case FILE_DIR: return "directory";
        case FILE_SYMLINK: return "symlink";
        default: return "unknown";
        }
}

static void
print(void *key, void *data, void *user_data)
{
        char *path = key;
        tpath_entry *pe = data;
        LOG(LOG_DEBUG, "key=%s, path=%s, fd=%d, type=%s, digest=%.*s",
            path, pe->path, pe->fd, pentry_type_to_str(pe->filetype),
            MD5_DIGEST_LENGTH, pe->digest);
}

void
hash_print_all(void)
{
        g_hash_table_foreach(hash, print, NULL);
}

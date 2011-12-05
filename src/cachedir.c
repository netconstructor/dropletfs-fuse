#include <time.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "cachedir.h"
#include "tmpstr.h"
#include "log.h"
#include "hash.h"
#include "getattr.h"
#include "timeout.h"
#include "file.h"
#include "list.h"

#define MAX_CHILDREN 30

extern struct conf *conf;
extern dpl_ctx_t *ctx;
extern GHashTable *hash;

GThreadPool *pool;

static void
cb_map_dirents(void *elem, void *cb_arg)
{
        char *path = NULL;
        dpl_dict_t *usermd = NULL;
        dpl_status_t rc;
        dpl_ftype_t type;
        dpl_ino_t ino, parent_ino, obj_ino;
        tpath_entry *pe_dirent = NULL;
        tpath_entry *pe = NULL;

        path = elem;
        pe = cb_arg;

        LOG(LOG_DEBUG, "path='%s', dirent='%s'", path, pe->path);

        pe_dirent = g_hash_table_lookup(hash, path);
        if (! pe_dirent) {
                LOG(LOG_ERR, "'%s' is not an entry anymore in '%s'",
                    path, pe->path);
                goto end;
        }

        rc = dfs_namei_timeout(ctx, path, ctx->cur_bucket,
                               ino, &parent_ino, &obj_ino, &type);

        LOG(LOG_DEBUG, "path=%s, dpl_namei: %s, type=%s, parent_ino=%s, obj_ino=%s",
            path, dpl_status_str(rc), ftype_to_str(type),
            parent_ino.key, obj_ino.key);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_NOTICE, "dfs_namei_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        rc = dfs_getattr_timeout(ctx, path, &usermd);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        if (pentry_md_trylock(pe_dirent))
                goto end;

        if (usermd)
                pentry_set_usermd(pe_dirent, usermd);

        pe_dirent->atime = time(NULL);

        pentry_md_unlock(pe_dirent);
  end:
        if (usermd)
                dpl_dict_free(usermd);

}

static void
update_md(gpointer data,
          gpointer user_data)
{
        tpath_entry *pe = NULL;
        dpl_ftype_t type;
        dpl_ino_t ino;
        dpl_status_t rc;
        dpl_dict_t *usermd = NULL;
        struct list *dirent = NULL;

        (void)user_data;
        pe = data;

        LOG(LOG_DEBUG, "path=%s", pe->path);

        ino = dpl_cwd(ctx, ctx->cur_bucket);

        rc = dfs_namei_timeout(ctx, pe->path, ctx->cur_bucket,
                               ino, NULL, NULL, &type);

        if (DPL_SUCCESS != rc) {
                LOG(LOG_NOTICE, "dfs_namei_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        rc = dfs_getattr_timeout(ctx, pe->path, &usermd);
        if (DPL_SUCCESS != rc && DPL_EISDIR != rc) {
                LOG(LOG_ERR, "dfs_getattr_timeout: %s", dpl_status_str(rc));
                goto end;
        }

        /* If this is a directory, update its entries' metadata */
        if (DPL_FTYPE_DIR == type) {
                if (pe->dirent)
                        list_map(pe->dirent, cb_map_dirents, pe);
        }

        if (pentry_md_trylock(pe))
                goto end;

        if (usermd)
                pentry_set_usermd(pe, usermd);

        pe->atime = time(NULL);

        pentry_md_unlock(pe);
  end:
        if (usermd)
                dpl_dict_free(usermd);
}

static void
cachedir_callback(gpointer key,
                  gpointer value,
                  gpointer user_data)
{
        GHashTable *hash = NULL;
        char *path = NULL;
        tpath_entry *pe = NULL;
        time_t age;
        int total_size;

        path = key;
        pe = value;
        hash = user_data;

        age = time(NULL) - pe->atime;

        LOG(LOG_DEBUG, "%s, age=%d sec", path, (int) age);

        /* 64 is a bold estimation of a path length */
        total_size = g_hash_table_size(hash) * (sizeof *pe + 64);
        if (total_size > conf->cache_max_size)
                return;

        if (age < conf->sc_age_threshold)
                return;

        g_thread_pool_push(pool, pe, NULL);

        return;
}

static void
cb_get_md(gpointer data,
          gpointer user_data)
{
        const char *path = data;
        struct stat stbuf;

        LOG(LOG_DEBUG, "starting a new thread for rootdir md update");

        memset(&stbuf, 0, sizeof stbuf);
        (void)dfs_getattr(path, &stbuf);
}

static void
root_dir_preload(GThreadPool *pool,
                 GHashTable *hash)
{
        char *root_dir = "/";
        void *dir_hdl = NULL;
        dpl_dirent_t dirent;
        dpl_status_t rc = DPL_FAILURE;
        tpath_entry *pe = NULL;
        char *direntname = NULL;
        char *key = NULL;

        pe = g_hash_table_lookup(hash, root_dir);
        if (! pe) {
                pe = pentry_new();
                if (! pe) {
                        LOG(LOG_ERR, "%s: can't add a new cell", root_dir);
                        goto err;
                }
                pentry_set_path(pe, root_dir);
                key = strdup(root_dir);
                if (! key) {
                        LOG(LOG_ERR, "%s: strdup: %s",
                            root_dir, strerror(errno));
                        pentry_free(pe);
                        goto err;
                }
                g_hash_table_insert(hash, key, pe);
        }

        rc = dfs_chdir_timeout(ctx, root_dir);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_chdir_timeout: %s", dpl_status_str(rc));
                goto err;
        }

        rc = dfs_opendir_timeout(ctx, ".", &dir_hdl);
        if (DPL_SUCCESS != rc) {
                LOG(LOG_ERR, "dfs_opendir_timeout: %s", dpl_status_str(rc));
                goto err;
        }

        while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent)) {
                direntname = tmpstr_printf("%s%s", root_dir, dirent.name);
                g_thread_pool_push(pool, direntname, NULL);
        }

        pe->atime = time(NULL);
  err:
        if (dir_hdl)
                dpl_closedir(dir_hdl);
}

void *
thread_cachedir(void *cb_arg)
{
        GHashTable *hash = cb_arg;
        int n_children;
        GError *err;

        LOG(LOG_DEBUG, "entering thread");

        if (! conf->sc_loop_delay || ! conf->sc_age_threshold)
                return NULL;

        pool = g_thread_pool_new(cb_get_md, NULL, 50, FALSE, &err);
        if (err) {
                LOG(LOG_ERR, "thread pool creation: %s", err->message);
                goto end;
        } else {
                root_dir_preload(pool, hash);
        }

        while (pool && ! g_thread_pool_get_num_threads(pool))
                usleep(5 * 1000); /* 5ms */

        while (1) {
                LOG(LOG_DEBUG, "updating cache directories");
                sleep(conf->sc_loop_delay);
                g_hash_table_foreach(hash, cachedir_callback, hash);
        }

  end:
        if (pool)
                g_thread_pool_free(pool, TRUE, TRUE);

        return NULL;
}

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/statvfs.h>
#include <libgen.h>

#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "glob.h"


dpl_ctx_t *ctx = NULL;
FILE *fp = NULL;
mode_t root_mode = 0;
int debug = 0;
GHashTable *hash = NULL;

static void
atexit_callback(void)
{
        g_hash_table_remove_all(hash);

        if (ctx)
                dpl_ctx_free(ctx);

	dpl_free();

        if (fp)
                fclose(fp);
}



static void
display_attribute(dpl_var_t *var,
                  void *arg)
{
        LOG("attribute for object %s: %s=%s",
            (char *)arg, var->key, var->value);
}





/* Not implemented yet */

static int
dfs_getxattr(const char *path,
             const char *name,
             char *value,
             size_t size)
{
        LOG("path=%s, name=%s, value=%s, size=%zu",
            path, name, value, size);
        return 0;
}

static int
dfs_listxattr(const char *path,
              char *list,
              size_t size)
{
        LOG("path=%s, list=%s, size=%zu", path, list, size);
        return 0;
}

static int
dfs_removexattr(const char *path,
                const char *name)
{
        LOG("path=%s, name=%s", path, name);
        return 0;
}

static int
dfs_flush(const char *path,
          struct fuse_file_info *info)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_mknod(const char *path,
          mode_t mode)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_readlink(const char *path,
             char *buf,
             size_t bufsiz)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_symlink(const char *oldpath,
            const char *newpath)
{
        LOG("%s -> %s", oldpath, newpath);
        return 0;
}

static int
dfs_truncate(const char *path,
             off_t offset)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_utime(const char *path,
          struct utimbuf *times)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_releasedir(const char *path,
               struct fuse_file_info *info)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_fsyncdir(const char *path,
             int datasync,
             struct fuse_file_info *info)
{
        LOG("%s", path);
        return 0;
}

static void *
dfs_init(struct fuse_conn_info *conn)
{
        LOG("Entering function");
        return NULL;
}

static void
dfs_destroy(void *arg)
{
        LOG("%p", arg);
}

static int
dfs_access(const char *path, int perm)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_ftruncate(const char *path,
              off_t offset,
              struct fuse_file_info *info)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_fgetattr(const char *path,
             struct stat *buf,
             struct fuse_file_info *info)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_lock(const char *path,
         struct fuse_file_info *info,
         int cmd,
         struct flock *flock)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_utimens(const char *path,
            const struct timespec tv[2])
{
        LOG("%s", path);
        return 0;
}

static int
dfs_bmap(const char *path,
         size_t blocksize,
         uint64_t *idx)
{
        LOG("%s", path);
        return 0;
}

#if 0
static int
dfs_ioctl(const char *path,
          int cmd,
          void *arg,
          struct fuse_file_info *info,
          unsigned int flags,
          void *data)
{
        LOG("%s", path);
        return 0;
}

static int
dfs_poll(const char *path,
         struct fuse_file_info *info,
         struct fuse_pollhandle *ph,
         unsigned *reventsp)
{
        LOG("%s", path);
        return 0;
}
#endif

struct fuse_operations dfs_ops = {
        /* implemented */
        .getattr    = dfs_getattr,
        .mkdir      = dfs_mkdir,
        .write      = dfs_write,
        .readdir    = dfs_readdir,
        .opendir    = dfs_opendir,
        .unlink     = dfs_unlink,
        .rmdir      = dfs_rmdir,
        .statfs     = dfs_statfs,
        .read       = dfs_read,
        .release    = dfs_release,
        .open       = dfs_open,
        .fsync      = dfs_fsync,
        .setxattr   = dfs_setxattr,
        .create     = dfs_create,
        .chmod      = dfs_chmod,
        .chown      = dfs_chown,

        /* not implemented yet */
        .getxattr   = dfs_getxattr,
        .listxattr  = dfs_listxattr,
        .removexattr= dfs_removexattr,
        .readlink   = dfs_readlink,
        .symlink    = dfs_symlink,
        .rename     = dfs_rename,
        .truncate   = dfs_truncate,
        .utime      = dfs_utime,
        .flush      = dfs_flush,
        .setxattr   = dfs_setxattr,
        .fsyncdir   = dfs_fsyncdir,
        .init       = dfs_init,
        .destroy    = dfs_destroy,
        .access     = dfs_access,
        .ftruncate  = dfs_ftruncate,
        .fgetattr   = dfs_fgetattr,
        .lock       = dfs_lock,
        .utimens    = dfs_utimens,
        .bmap       = dfs_bmap,
#if 0
        .iotcl      = dfs_ioctl,
        .poll       = dfs_poll,
#endif
        .getdir     = NULL, /* deprecated */
        .link       = NULL, /* no support needed */
};


static int
dfs_fuse_main(struct fuse_args *args)
{
        hash = g_hash_table_new(g_str_hash, g_str_equal);
        return fuse_main(args->argc, args->argv, &dfs_ops, NULL);
}


static void
droplet_pp(dpl_ctx_t *ctx)
{
#define PP(field, fmt) fprintf(stdout, #field": "fmt"\n", ctx->field)
        PP(n_conn_buckets, "%d");
        PP(n_conn_max, "%d");
        PP(n_conn_max_hits, "%d");
        PP(conn_idle_time , "%d");
        PP(conn_timeout, "%d");
        PP(read_timeout, "%d");
        PP(write_timeout, "%d");
        PP(use_https, "%d");
        PP(host, "%s");
        PP(port, "%d");

        PP(access_key, "%s");
        PP(secret_key, "%s");
        PP(ssl_cert_file, "%s");
        PP(ssl_key_file, "%s");
        PP(ssl_password, "%s");
        PP(ssl_ca_list, "%s");

        PP(trace_level, "%d");

        PP(pricing, "%s");
        PP(read_buf_size, "%u");
        PP(encrypt_key, "%s");
        PP(delim, "%s");

        PP(n_conn_fds, "%d");
        PP(cur_bucket, "%s");
        PP(droplet_dir, "%s");
        PP(profile_name, "%s");
#undef PP
}

static void
usage(const char * const prog)
{
        printf("Usage: %s <bucket> [-d] <mount point>\n", prog);
}


int
main(int argc, char **argv)
{
        root_mode = 0;
        debug = 0;
        hash = NULL;

        atexit(atexit_callback);

        int rc = EXIT_FAILURE;
        char *bucket = NULL;
        openlog("dplfs", LOG_CONS | LOG_NOWAIT | LOG_PID, LOG_USER);
        fp = fopen("/tmp/fuse.log", "a");
        if (! fp) {
                goto err0;
        }

        if (argc < 3) {
                usage(argv[0]);
                goto err1;
        }

        bucket = argv[1];
        argc -= 1;
        argv += 1;

        if (0 == strncmp(argv[1], "-d", 2)) {
                debug = 1;
                argc -= 1;
                argv += 1;
        }

        struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	dpl_status_t st = dpl_init();
	if (DPL_SUCCESS != st) {
		fprintf(fp, "dpl_init: %s\n", dpl_status_str(st));
                goto err1;
	}

	ctx = dpl_ctx_new(NULL, NULL);
	if (! ctx)
                goto err2;

        ctx->trace_level = ~0;
        ctx->cur_bucket = strdup(bucket);
        droplet_pp(ctx);

        char *cache_dir = tmpstr_printf("/tmp/%s", ctx->cur_bucket);
        if (-1 == mkdir(cache_dir, 0777) && EEXIST != errno) {
                LOG("mkdir(%s) = %s", cache_dir, strerror(errno));
                goto err3;
        }

        rc = dfs_fuse_main(&args);

  err3:
        dpl_ctx_free(ctx);
        g_hash_table_remove_all(hash);
  err2:
	dpl_free();
  err1:
        fclose(fp);
  err0:
	return rc;
}


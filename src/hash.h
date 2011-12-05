#ifndef DROPLET_HASH_H
#define DROPLET_HASH_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

#include <droplet.h>

enum {
        FLAG_CLEAN=0,
        FLAG_DIRTY,
};

typedef enum {
        FILE_REG=0,
        FILE_DIR,
        FILE_SYMLINK,
} filetype_t;

enum {
        FILE_LOCAL=0,
        FILE_REMOTE,
        FILE_UNSET,
};

/* path entry on remote storage file system */
typedef struct {
        int fd;
        char *path;
        struct stat st;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *usermd;
        pthread_mutex_t md_mutex;
        pthread_mutex_t mutex;
        sem_t refcount;
        int flag;
        int exclude;
        filetype_t filetype;
        struct list *dirent;
        int ondisk;
        time_t atime, mtime, ctime;
} tpath_entry;

void hash_print_all(void);

tpath_entry *pentry_new(void);
void pentry_free(tpath_entry *);

char *pentry_placeholder_to_str(int);

int pentry_remove_dirent(tpath_entry *, const char *);
void pentry_add_dirent(tpath_entry *, const char *);
struct list;
struct list *pentry_get_dirents(tpath_entry *);

void pentry_unlink_cache_file(tpath_entry *);

int tpath_entryrylock(tpath_entry *);
void pentry_lock(tpath_entry *);
void pentry_unlock(tpath_entry *);
int pentry_md_trylock(tpath_entry *);
void pentry_md_lock(tpath_entry *);
void pentry_md_unlock(tpath_entry *);

void pentry_inc_refcount(tpath_entry *);
void pentry_dec_refcount(tpath_entry *);
int pentry_get_refcount(tpath_entry *);

void pentry_set_path(tpath_entry *, const char *);

char *tpath_entryype_to_str(filetype_t);

/* return 0 on success, -1 on failure */
int pentry_set_usermd(tpath_entry *, dpl_dict_t *);

/* return 0 on success, -1 on failure */
int pentry_set_digest(tpath_entry *, const char *);


#endif

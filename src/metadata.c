#include <assert.h>

#include "log.h"
#include "metadata.h"
#include "tmpstr.h"


void
assign_meta_to_dict(dpl_dict_t *dict,
                    char *meta,
                    unsigned long val)
{
        char *buf = NULL;

        buf = tmpstr_printf("%lu", val);
        LOG(LOG_DEBUG, "meta='%s', value='%s'", meta, buf);

        if (DPL_SUCCESS != dpl_dict_update_value(dict, meta, buf))
                LOG(LOG_ERR, "can't update value '%s' for '%s'", buf, meta);
}

void
fill_metadata_from_stat(dpl_dict_t *dict,
                        struct stat *st)
{
        assign_meta_to_dict(dict, "mode", (unsigned long)st->st_mode);
        assign_meta_to_dict(dict, "size", (unsigned long)st->st_size);
        assign_meta_to_dict(dict, "uid", (unsigned long)st->st_uid);
        assign_meta_to_dict(dict, "gid", (unsigned long)st->st_gid);
        assign_meta_to_dict(dict, "atime", (unsigned long)st->st_atime);
        assign_meta_to_dict(dict, "mtime", (unsigned long)st->st_mtime);
        assign_meta_to_dict(dict, "ctime", (unsigned long)st->st_ctime);
}

static long long
metadatatoll(dpl_dict_t *dict,
             const char *const name)
{
        char *value = NULL;
        long long v = 0;

        value = dpl_dict_get_value(dict, (char *)name);

        if (! value) {
                return -1;
        }

        v = strtoull(value, NULL, 10);
        if (0 == strcmp(name, "mode"))
                LOG(LOG_DEBUG, "meta=%s, value=O%x", name, (unsigned)v);
        else
                LOG(LOG_DEBUG, "meta=%s, value=%s", name, value);

        return v;
}

#define STORE_META(st, dict, name, type) do {                           \
                long long v = metadatatoll(dict, #name);                \
                if (-1 != v)                                            \
                        st->st_##name = (type)v;                        \
        } while (0 /*CONSTCOND*/)

void
fill_stat_from_metadata(struct stat *st,
                        dpl_dict_t *dict)
{
        time_t now = time(NULL);
        STORE_META(st, dict, size, size_t);
        STORE_META(st, dict, mode, mode_t);
        STORE_META(st, dict, uid, uid_t);
        STORE_META(st, dict, gid, gid_t);
        STORE_META(st, dict, atime, time_t);
        STORE_META(st, dict, ctime, time_t);
        STORE_META(st, dict, mtime, time_t);

        if (dpl_dict_get(dict, "symlink"))
                st->st_mode |= S_IFLNK;
}

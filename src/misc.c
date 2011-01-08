#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "misc.h"
#include "tmpstr.h"

#define MKDIR(path, mode) do {                                          \
                if (-1 == mkdir(path, mode) && EEXIST != errno)         \
                        perror("mkdir");                                \
        } while (0)

void
mkdir_tree(const char *dir) {
/*         char *tmp = NULL; */
        char *p = NULL;
        size_t len;

/*         tmp = tmpstr_printf(tmp, sizeof tmp, "%s", dir); */
        char tmp[2048];
        snprintf(tmp, sizeof tmp, "%s", dir);
        len = strlen(tmp);

        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;

        for(p = tmp + 1; *p; p++) {
                if(*p == '/') {
                        *p = 0;
                        MKDIR(tmp, 0755);
                        *p = '/';
                }
        }

        MKDIR(tmp, 0755);
}

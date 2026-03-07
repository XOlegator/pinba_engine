/* C-compatible plugin types for _mysql_plugin_declarations_ definition. */
#ifndef MYSQL8_PLUGIN_C_H
#define MYSQL8_PLUGIN_C_H

#include <stddef.h>

struct st_plugin_var;
struct st_mysql_storage_engine;

struct st_mysql_plugin {
    int type;
    void *info;
    const char *name;
    const char *author;
    const char *descr;
    int license;
    int (*init)(void *);
    int (*check_uninstall)(void *);
    int (*deinit)(void *);
    unsigned int version;
    struct st_plugin_var *status_vars;
    struct st_plugin_var *system_vars;
    void *reserved;
    unsigned long flags;
};

#endif

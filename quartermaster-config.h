#ifndef QUARTERMASTER_CONFIG_H
#define QUARTERMASTER_CONFIG_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define CONF_DIR "/etc/quartermaster.d"
#define MAX_ARGS 3

struct config_target {
    char exec[256];
    char *match[MAX_ARGS];
    char match_buf[512];
    int stop_when_none;
};
int valid_target_name(const char *namestr);
int load_config_target(const char *name, struct config_target *t);
int tokenize_args(char *argstr, char *out[], int max);

#endif

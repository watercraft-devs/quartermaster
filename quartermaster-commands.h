#ifndef QUARTERMASTER_COMMANDS_H
#define QUARTERMASTER_COMMANDS_H


#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include "quartermaster-config.h"


#define RULES_SRC "/usr/local/share/quartermaster/rules"
#define RULES_DST "/etc/udev/rules.d"

int copy_file(const char *from, const char *to, mode_t rwx);

int cmd_enable(const char *target_name);

int cmd_disable(const char *target_name);

int cmd_list(void);


#endif

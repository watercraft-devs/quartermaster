#include "quartermaster-config.h"

int tokenize_args(char *argstr, char *out[], int max) { // destroys argstr on call
    char *checkpoint;
    int argcount = 0;
    for (
        char *token = strtok_r(argstr, " \t", &checkpoint);
        token;
        token = strtok_r(NULL, " \t", &checkpoint)
    ) {
        if (argcount >= max - 1) return -1;
        out[argcount++] = token;
    }
    out[argcount] = NULL;
    return argcount;
}


int valid_target_name(const char *namestr)
{
    if (*namestr == '\0') return 0;

    for (const char *char_p = namestr; *char_p; char_p++) {
        if (*char_p >= 'a' && *char_p <= 'z') continue;
        if (*char_p >= 'A' && *char_p <= 'Z') continue;
        if (*char_p >= '0' && *char_p <= '9') continue;
        if (
            *char_p == '_' ||
            *char_p == '-' ||
            *char_p == '.'
        ) continue;
    }

    if (strstr(namestr, "..")) return 0;

    return 1;
}

int load_config_target(const char *name, struct config_target *t) {
    char config_path[512];

    if (snprintf(config_path, sizeof config_path, "%s/%s.conf", CONF_DIR, name) >= (int)sizeof config_path) return -1;

    FILE *config_file = fopen(config_path, "r");

    if (!config_file) {
        fprintf(stderr, "fopen %s: %s\n", config_path, strerror(errno));
        return -1;
    }

    memset(t, 0, sizeof *t);

    char line[512];

    while (fgets(line, sizeof line, config_file)) {
        line[strcspn(line, "\n")] = '\0';

        char *line_p = line;

        while (*line_p == ' ' || *line_p == '\t') line_p++ ; // skip spaces and #'s

        if (*line_p == '\0' || *line_p == '#') continue;

        char *key = line_p;

        while (*line_p && *line_p != ' ' && *line_p != '\t') line_p++;

        if (*line_p) *line_p++ = '\0';

        while (*line_p == ' ' || *line_p == '\t') line_p++;

        if (strcmp(key, "exec") == 0) {
            if (snprintf(t->exec, sizeof t->exec, "%s", line_p) >= (int)sizeof t->exec) { 
                fclose(config_file); return -1; 
            };
        } else if (strcmp(key, "match") == 0) {
            snprintf(t->match_buf, sizeof t->match_buf, "%s", line_p);
        } else if (strcmp(key, "stop_when") == 0) {
            t->stop_when_none = (strcmp(line_p, "none_present") == 0);
        }
    }

    fclose(config_file);

    if (t->exec[0] == '\0' || t->match_buf[0] == '\0') return -1;

    return 0;
}


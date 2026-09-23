#include "quartermaster-commands.h"


int copy_file(const char *from, const char *to, mode_t rwx) {
    FILE *input_file = fopen(from, "r");

    if (!input_file) {
        fprintf(stderr, "error from fopen read: %s: %s\n", from, strerror(errno));
        return -1;
    }

    FILE *output_file = fopen(to, "w");
   
    if (!output_file) {
        fprintf(stderr, "error from fopen write: %s: %s\n", to, strerror(errno));
        fclose(input_file);
        return -1;
    } 

    char buf[4096];
    size_t output_size;

    while ((output_size = fread(buf, 1, sizeof buf, input_file)) > 0) {
        if (fwrite(buf, 1, output_size, output_file) != output_size) {
            fprintf(stderr, "error from fwrite: %s: %s\n", to, strerror(errno));
            fclose(input_file);
            fclose(output_file);
            return -1;
        }
    }

    fclose(input_file);

    if (fclose(output_file) != 0) {
        fprintf(stderr, "close %s: %s\n", to, strerror(errno));
        return -1;
    }
    
    chmod(to, rwx);

    return 0;

}

int cmd_enable(const char *target_name) {
    if (!valid_target_name(target_name)) {
        fprintf(stderr, "invalid target name: %s\n", target_name);
        return 2;
    }

    char conf[512];
    snprintf(conf, sizeof conf, "%s/%s.conf", CONF_DIR, target_name);

    if (access(conf, R_OK) != 0) {
        fprintf(stderr, "no config present for target: %s (%s)\n", target_name, conf);
        return -1;
    }

    char from[512], to[512];
    snprintf(from, sizeof from, "%s/71-quartermaster-%s.rules", RULES_SRC, target_name);
    snprintf(to, sizeof to, "%s/71-quartermaster-%s.rules", RULES_DST, target_name);

    if (access(from, R_OK) != 0) {
        fprintf(stderr, "no rule found %s (%s)\n", target_name, from); //dont run anything without rules
        return 1;
    }

    if (access(to, F_OK) == 0) {
        fprintf(stderr, "%s is already enabled\n", target_name); //prevent duplicate spawning
        return 0;
    }
    mode_t owner_only_rw = 0644;    
    if (copy_file(from, to, owner_only_rw) < 0) return 1;

    printf("quartermaster has enabled rules at: %s as commanded\n", to);

    if (system("udevadm control --reload-rules") != 0) 
        fprintf(stderr, "WARNING: failed to reload udevadm rules");

    return 0;

}

int cmd_disable(const char *target_name) {
    if (!valid_target_name(target_name)) {
        fprintf(stderr, "invalid target name: %s\n", target_name);
        return 2;
    }

    char to[512];
    snprintf(to, sizeof to, "%s/71-quartermaster-%s.rules", RULES_DST, target_name);

    if (unlink(to) < 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "%s has not been enabled so it cannot be disabled\n", target_name);
            return 0;
        }
        fprintf(stderr, "failed to unlink %s: %s\n", to, strerror(errno));
        return 1;            
    }

    printf("quartermaster has disabled udevadm rules for %s as commanded\n", target_name);

    if (system("udevadm control --reload-rules") != 0)
        fprintf(stderr, "WARNING: failed to reload udevadm rules");
    return 0;

}

int cmd_list(void) {
    DIR *conf_dir = opendir(CONF_DIR);

    if (!conf_dir) {
        fprintf(stderr, "failure to opendir: %s: %s\n", CONF_DIR, strerror(errno));
        return 1;
    }

    struct dirent *entry;
    int configs_found = 0;

    while((entry = readdir(conf_dir))) {
        size_t len = strlen(entry->d_name);

        if (len < 6 || strcmp(entry->d_name + len - 5, ".conf") != 0) continue;

        char target_name[256];

        if (len - 5 >= sizeof target_name) continue;

        memcpy(target_name, entry->d_name, len - 5);

        target_name[len - 5] = '\0';

        char rule[512];

        snprintf(
            rule, sizeof rule, "%s/71-quartermaster-%s.rules", RULES_DST, target_name
        );

        int enabled = (access(rule, F_OK) == 0);

        char shipped[512];
        snprintf(
            shipped, sizeof shipped, "%s/71-quartermaster-%s.rules", RULES_DST, target_name
        );

        int rule_exists = (access(shipped, R_OK) == 0);

        printf(
            "  %-24s %s%s\n", target_name, 
            enabled ? "enabled" : "disabled", 
            (!enabled && !rule_exists) ? " (no udev rule)" : ""
        ); 
        configs_found = 1;
    }

    closedir(conf_dir);

    if (!configs_found) printf("\nno configs in %s\n", CONF_DIR);

    return 0;
}

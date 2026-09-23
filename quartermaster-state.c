#include "quartermaster-state.h"

static struct tstate states[MAX_TARGETS];


int init_state(void){
    if (mkdir(STATE_DIR, 0700) < 0 && errno != EEXIST) {
        fprintf(stderr, "failure: mkdir: %s %s\n", STATE_DIR, strerror(errno));
        return -1;
    }
    return 0;
}

void get_state_path(const char *target_name, char *output, size_t len) {
    snprintf(output, len, STATE_DIR "/%s.state", target_name);
}

void save_state(const struct tstate *state) {
    char state_path[256], tmp[264];

    get_state_path(state->name, state_path, sizeof state_path);

    if (state->pid == 0) {
        unlink(state_path);
        return;
    }

    snprintf(tmp, sizeof tmp, "%s.tmp", state_path);

    FILE *state_f = fopen(tmp, "w");

    if (!state_f) {
        fprintf(stderr, "%s: %s\n", tmp, strerror(errno));
        return;
    }

    fprintf(state_f, "%d %s\n", (int)state->pid, state->comm);

    if (fclose(state_f) != 0 || rename(tmp, state_path) > 0) unlink(tmp);
}

void load_state(struct tstate *state) {

    char state_path[256];
    get_state_path(state->name, state_path, sizeof state_path);

    FILE *state_f = fopen(state_path, "r");
    if (!state_f) return;

    int pid;
    char comm[64];
    int scan_ok = (fscanf(state_f, "%d %63s", &pid, comm) == 2);

    fclose(state_f);

    char current_comm[64];

    if (
            !scan_ok                                                || 
            pid <= 0                                                || 
            read_comm(pid, current_comm, sizeof current_comm) < 0   || 
            strcmp(current_comm, comm) != 0
    ) {
        unlink(state_path);
        return;
    }

    state->pid = pid;
    snprintf(state->comm, sizeof state->comm, "%s", comm);
    fprintf(stderr, "%s: reclaimed previously owned pid:  %d \n", state->name, pid);

}

void catch_child_procs(void) {
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_TARGETS; i++) {
            if (states[i].used && states[i].pid == pid) {
                fprintf(stderr, "%s: exited (pid %d)\n", states[i].name, (int)pid);
                states[i].pid = 0;
                states[i].comm[0] = '\0';
                save_state(&states[i]);
                break;
            }
        }  
    }
}


struct tstate *get_state(const char *name) {
    struct tstate *free_slot = NULL;

    for (int i=0; i < MAX_TARGETS; i++) {
        if (states[i].used && strcmp(states[i].name, name) == 0) return &states[i];

        if (!states[i].used && !free_slot) free_slot = &states[i];
    }

    if (!free_slot) return NULL;

    memset(free_slot, 0, sizeof *free_slot);
    snprintf(free_slot->name, sizeof free_slot->name, "%s", name);
    free_slot->used = 1;

    load_state(free_slot);

    return free_slot;
}

int read_comm(pid_t pid, char *output, size_t len) {
    char comm_path[64];

    snprintf(comm_path, sizeof comm_path, "/proc/%d/comm", (int)pid);

    FILE *comm_f_read = fopen(comm_path, "r");

    if (!comm_f_read) return -1;

    if (!fgets(output, (int)len, comm_f_read)) {
        fclose(comm_f_read);
        return -1;
    }

    fclose(comm_f_read);

    output[strcspn(output, "\n")] = '\0';

    return 0;

}

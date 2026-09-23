#define MAX_TARGETS 16

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#define STATE_DIR "/run/quartermaster"

struct tstate {
    char name[64];
    pid_t pid; // 0 == not running
    char comm[64]; // /proc/PID/comm state at start time
    int foreign; // not our proc
    int used;
};

int init_state(void);

void get_state_path(const char *target_name, char *output, size_t len);

void save_state(const struct tstate *state);

void load_state(struct tstate *state);

void catch_child_procs(void);

struct tstate *get_state(const char *name);

int read_comm(pid_t pid, char *out, size_t len);



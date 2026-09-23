#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include "quartermaster-config.h"
#include "quartermaster-state.h"
#include "quartermaster-commands.h"
#define FIFO_PATH "/run/quartermaster.fifo"
#define FIFO_MODE 0600

static void expected_comm(const char *exec, char *output, size_t len){
    const char *base = strrchr(exec, '/');
    base = base ? base + 1 : exec;
    snprintf(output, len, "%.15s", base);
}

static int stop_target(struct tstate *state) {
    if (state->pid == 0) return 0; // already stopped
    
    if (state->foreign) {
        fprintf(
            stderr, "%s: quartermaster did not start this process, leaving it alone\n", 
            state->name
        );
        return 0;
    }

    char current[64];

    if (read_comm(state->pid, current, sizeof current) < 0) {
        fprintf(stderr, "%s: pid %d is gone\n", state->name, (int)state->pid);
        state->pid = 0;
        state->comm[0] = '\0';
        return 0;
    }

    if (strcmp(current, state->comm) != 0) {
        fprintf(
            stderr, 
            "%s: target pid %d is currently labeled '%s', quartermaster expected '%s' - " 
            "quartermaster is refusing to send the stop signal\n", 
            state->name, 
            (int)state->pid, 
            current, 
            state->comm
        );
        state->pid = 0;
        state->comm[0] = '\0';
        return -1;
    }

    fprintf(stderr, "%s: stopping pid %d\n", state->name, (int)state->pid);

    if (kill(state->pid, SIGTERM) < 0 && errno != ESRCH)
        fprintf(stderr, "%s: kill: %s\n", state->name, strerror(errno));


    for (int i=0; i<50; i++) {
        pid_t r_pid = waitpid(state->pid, NULL, WNOHANG);

        if (r_pid == state->pid) goto done;

        if (r_pid < 0 && errno == ECHILD && kill(state->pid, 0) < 0 && errno == ESRCH) goto done;

        usleep(100000);
    }

    char verify[64];

    if (read_comm(state->pid, verify, sizeof verify) < 0 || strcmp(verify, state->comm) != 0) goto done;

    fprintf(stderr, "%s: did not exit, sending SIGKILL\n", state->name);

    kill(state->pid, SIGKILL);

    waitpid(state->pid, NULL, 0);

done:

    state->pid = 0;
    state->comm[0] = '\0';
    save_state(state);

    return 0;

}

static pid_t find_foreign(const char *exec) {
    DIR *proc_d = opendir("/proc");
    struct dirent *entry;
    pid_t match = 0;
    while ((entry = readdir(proc_d))) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        char *end;
        long pid = strtol(entry->d_name, &end, 10);

        if (*end != '\0' || pid <= 0) continue;

        char link[280], target[512];
        snprintf(link, sizeof link, "/proc/%s/exe", entry->d_name);
        ssize_t link_n = readlink(link, target, sizeof target - 1);
        if (link_n < 0) continue;
        target[link_n] = '\0';
        if (strcmp(target, exec) == 0) {
            match = (pid_t)atoi(entry->d_name);
            break;
        }          
    }
    closedir(proc_d);
    return match;
}

static int start_target(struct tstate *state, const struct config_target *target)
{
    
    pid_t existing = find_foreign(target->exec);

    if (existing > 0) {
        fprintf(
            stderr,
            "%s: already running as pid %d\n"
            "quartermaster did not spawn this process\n" 
            "leaving it alone\n",
            state->name, 
            (int)existing
        );
        state->foreign = 1;
        state->pid = 0;
        return 0;
    }

    int fds[2];

    if (pipe2(fds, O_CLOEXEC) < 0) {
        fprintf(stderr, "Failure: %s: pipe2: %s\n", state->name, strerror(errno));
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        setsid(); // always needs if (pid == 0) check before it
        
        int fd = open("/dev/null", O_RDWR);


        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) close(fd);
        }

        execl(target->exec, target->exec, (char *)NULL);

        int errnoint = errno;

        if (write(fds[1], &errnoint, sizeof errnoint) < 0) { /* do nothing*/ }

        _exit(127);                 
    }

    close(fds[1]);

    int errnoint;
    ssize_t readsize;

    do {
        readsize = read(fds[0], &errnoint, sizeof errnoint);
    } while (readsize < 0 && errno == EINTR);

    close(fds[0]);

    if (readsize == (ssize_t)sizeof errnoint) {
        fprintf(stderr, "failure: %s: exec %s: %s\n", state->name, target->exec, strerror(errnoint));
    }


    state->pid = pid;
    state->foreign = 0;
    expected_comm(target->exec, state->comm, sizeof state->comm);
    save_state(state);


    fprintf(stderr, "%s: started, pid %d\n", state->name, (int)pid);

    return 0;
}

static int count_devices(char *const argv[]) {
    int fds[2];

    if (pipe(fds) < 0) {
        fprintf(stderr, "pipe: %s\n", strerror(errno));
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(fds[1]);

    FILE * f_read = fdopen(fds[0], "r");

    if (!f_read) {
        close(fds[0]);
        return -1;
    }

    int device_count = 0;
    char line[512];

    while (fgets(line, sizeof line, f_read)) if (line[0] == '/') device_count++;

    fclose(f_read);

    int status;
    
    while(waitpid(pid, &status, 0) < 0) if (errno != EINTR) return -1;

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;

    return device_count;

}



static void handle_event(const char *target_name) {
    if (!valid_target_name(target_name)) {
        fprintf(stderr, "rejecting target name: %s\n", target_name);
        return;
    }

    struct config_target target;

    if (load_config_target(target_name, &target) < 0) {
        fprintf(stderr, "%s: no usable config\n", target_name);
        return;
    }

    struct tstate *state = get_state(target_name);

    if (!state) {
        fprintf(stderr, "%s: no free state slot\n", target_name);
        return;
    }

    char *argv[MAX_ARGS + 8];

    int n = 0;

    argv[n++] = "udevadm";
    argv[n++] = "trigger";
    argv[n++] = "--dry-run";
    argv[n++] = "--verbose";

    if (tokenize_args(target.match_buf, target.match, MAX_ARGS) < 0) {
        fprintf(stderr, "%s: too many match arguments\n", target_name);
        return;
    }

    for (int i=0; target.match[i]; i++) argv[n++] = target.match[i];

    argv[n] = NULL;

    int device_count = count_devices(argv);

    if (device_count < 0) {
        fprintf(stderr, "%s: device count failure\n", target_name);
        return;
    }

    fprintf(stderr, "%s: %d device(s) present\n", target_name, device_count);

    if (device_count > 0) {
        if (state->foreign) {
            if (find_foreign(target.exec) == 0) {
                fprintf(
                    stderr,
                    "%s: previous process owner is done, quartermaster resuming ownership\n", 
                    target_name
                );
                state->foreign = 0;

            } else { return; }
        }        
        if (state-> pid!= 0) return;
        start_target(state, &target);
    } else {
        if (!target.stop_when_none) return;
        stop_target(state);
    }
}



int run_daemon(void) {
    

    signal(SIGPIPE, SIG_IGN);

    init_state();

    if (mkfifo(FIFO_PATH, FIFO_MODE) < 0 && errno != EEXIST) {
        fprintf(stderr, "mkfifo %s: %s\n", FIFO_PATH, strerror(errno));
        return 1;
    }

    for (;;) {
        FILE *f = fopen(FIFO_PATH, "r");

        if (!f) {
            fprintf(stderr, "open %s: %s\n", FIFO_PATH, strerror(errno));
            return 1;
        }

        char line[256];

        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\n")] = '\0';
            if (line[0] == '\0') continue;
            catch_child_procs();
            handle_event(line);
        }

        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) return run_daemon();

    if (argc == 3 && strcmp(argv[1], "enable") == 0) return cmd_enable(argv[2]);

    if (argc == 3 &&  strcmp(argv[1], "disable") == 0) return cmd_disable(argv[2]);

    if (argc == 2 &&  strcmp(argv[1], "list") == 0) return cmd_list();

    fprintf(stderr,
            "%s - start and stop device specific daemons based on connection state\n\n"
            "usage:\n\n"
            "%s                         run the daemon\n"
            "%s enable <target_name>    install udev rule for target\n"
            "%s disable <target_name>   remove udev rule for target\n"
            "%s list                    show available and active rules for targets\n",
            argv[0], argv[0], argv[0], argv[0], argv[0]

    );

    return 2;
}

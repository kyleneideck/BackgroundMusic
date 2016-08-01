// TODO: Delete this before merging WIP-MultiprocessAppVols into master.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>
#include <errno.h>

extern int responsibility_get_responsible_for_pid(pid_t, int32_t*, uint64_t*, size_t*, char*);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return -1;
    }
    pid_t pid = (pid_t)atoi(argv[1]);

    {
        char path_buf[PROC_PIDPATHINFO_MAXSIZE] = "";
        size_t path_len = sizeof(path_buf);
        if (proc_pidpath(pid, path_buf, path_len) <= 0) {
            printf("Couldn't get pid path for pid %d\n", pid);
            printf("Error %d: %s\n", errno, strerror(errno));
            return -1;
        }
        path_buf[path_len - 1] = '\0';
        printf("Path for process: %s\n", path_buf);
    }

    {
        int32_t rpid;
        uint64_t urpid;
        char responsible_path_buf[PROC_PIDPATHINFO_MAXSIZE] = "";
        size_t responsible_path_len = sizeof(responsible_path_buf);
        if (responsibility_get_responsible_for_pid(pid, &rpid, &urpid, &responsible_path_len, responsible_path_buf) != 0) {
            printf("Couldn't get responsibility pid for pid %d\n", pid);
            printf("Error %d: %s\n", errno, strerror(errno));
            return -1;
        }
        responsible_path_buf[responsible_path_len - 1] = '\0';
        printf("Path for responsible process: %s\n", responsible_path_buf);
        printf("Responsible PID: %d\n", rpid);
        printf("Responsible unique PID: %llu\n", urpid);
    }

    return 0;
}

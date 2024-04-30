//
// Created by timpo on 30/04/24.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/resource.h>


FILE *log_file;
pid_t pids[16];
char *config;

struct Subprocess {
    int argc;
    char **argv;
    char *stdinn;
    char *stdoutt;
};

struct Subprocess **subprocs;


void close_files() {

    struct rlimit flimit;
    getrlimit(RLIMIT_NOFILE, &flimit);

    for (int fd = 0; fd < flimit.rlim_max; ++fd) {
        close(fd);
    }
}

void logg(FILE *log, const char *format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    vfprintf(log, format, arg_list);
    vfprintf(log, "\n", NULL);
    va_end(arg_list);
    fflush(log);
}

int is_absolute(char *path) {
    return strlen(path) >= 1 && path[0] == '/';
}

int start(struct Subprocess *sp) {
    pid_t pid = fork();

    switch (pid) {
        case -1:
            return -1;
        case 0:
            int input_fd = open(sp->stdinn, O_RDONLY);
            int output_fd = open(sp->stdoutt, O_CREAT | O_WRONLY | O_TRUNC, 0666);

            dup2(input_fd, STDIN_FILENO);
            dup2(output_fd, STDOUT_FILENO);

            if (execv(sp->argv[0], sp->argv) == -1) {
                exit(-1);
            }
            return getpid();
        default:
            return pid;
    }
}

int read_config() {
    FILE *file = fopen(config, "r");
    if (file == NULL) {
        return -1;
    }

    int cline = 0;
    char *currentLine;
    size_t len = 0;

    while (getline(&currentLine, &len, file) != -1) {
        char **argvv = malloc(sizeof(char *) * 8);
        size_t argcc = 0;

        char *arg;
        arg = strtok(currentLine, " \n");

        while (arg != NULL) {
            argvv[argcc] = malloc(sizeof(char) * (strlen(arg) + 1));
            strcpy(argvv[argcc], arg);
            argcc++;

            arg = strtok(NULL, " \n");
        }

        if (argcc < 3) {
            logg(log_file, "error config in line: %d", cline);
            return -1;
        }

        if (!is_absolute(argvv[0]) || !is_absolute(argvv[argcc - 1]) || !is_absolute(argvv[argcc - 2])) {
            logg(log_file, "Not absolute path in config in line: %d", cline);
            return -1;
        }

        char *stdoutFilename = argvv[argcc - 1];

        char *stdinFilename = argvv[argcc - 2];

        char **sp_argv = malloc(sizeof(char *) * (argcc - 2));
        int sp_argc = 0;

        for (int i = 0; i < argcc - 2; i++) {
            sp_argv[sp_argc] = malloc(sizeof(char) * (strlen(argvv[i]) + 1));
            strcpy(sp_argv[sp_argc], argvv[i]);
            sp_argc++;
        }

        subprocs[cline] = (struct Subprocess *) malloc(sizeof(struct Subprocess));
        logg(log_file, "%d", cline);

        subprocs[cline]->argc = sp_argc;
        subprocs[cline]->argv = sp_argv;
        subprocs[cline]->stdinn = stdinFilename;
        subprocs[cline]->stdoutt = stdoutFilename;
        cline++;

    }
    for (int i = 0; i < cline; i++) {
        logg(log_file, "process started: %d", i);
        pids[i] = start(subprocs[i]);
        logg(log_file, "process started: %d", pids[i]);
    }
    fclose(file);
    return cline;
}

void run() {
    int cline;
    logg(log_file, "initiating config");
    if ((cline = read_config()) < 0) {
        exit(-1);
    }
    logg(log_file, "config is initiated");
    while (1) {
        int status;
        pid_t pid = wait(&status);

        for (int i = 0; i < cline; i++) {
            if (pids[i] <= 0) {
                continue;
            }

            if (pids[i] == pid || errno == ECHILD) {
                int old_pid = pids[i];
                logg(log_file, "Process %d finished with status %d", old_pid, status);

                logg(log_file, "Restarting process %d", old_pid);
                pids[i] = start(subprocs[i]);
                logg(log_file, "Process %d restarted with pid %d", old_pid, pids[i]);
            }
        }
    }
}


void handle_sighup(int sig) {
    logg(log_file, "SIGHUP, RESTARTING");

    for (int i = 0; i < 16; i++) {
        if (pids[i] > 0) {
            kill(pids[i], SIGTERM);
        }
    }
    run();

}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./main <config>");
        return -1;
    }

    config = argv[1];

    if (fork() != 0) {
        exit(0);
    }

    if (getpid() != 1) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
    }

    if (setsid() == -1) {
        return -1;
    }

    close_files();


    if (chdir("/") != 0) {
        perror("Error while changing directory to root");
        exit(-1);
    }

    log_file = fopen("/tmp/myinit.logg", "a");
    if (log_file == NULL) {
        return -1;
    }

    logg(log_file, "DAEMON IS STARTED");
    subprocs = malloc(sizeof(struct Subprocess *) * 16);
    signal(SIGHUP, handle_sighup);

    run();
}
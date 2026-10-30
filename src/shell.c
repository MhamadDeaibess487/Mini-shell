/*
 * Copyright (C) 2002, Simon Nieuviarts
 * Job control: one process group per command line, including pipelines.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include "readcmd.h"

struct process {
    pid_t pid;
    volatile sig_atomic_t done, stopped;
};
struct job {
    pid_t pgid;
    size_t count;
    struct process *processes;
    char *command;
    struct termios modes;
    int has_modes;
};
#define MAXJOBS 15
static struct job job_table[MAXJOBS];
static volatile sig_atomic_t foreground_group;
static sigset_t child_mask;
static int interactive;
static pid_t shell_group;
static struct termios shell_modes;

/* No allocation, stdio or free in a signal handler. Table mutations in the
 * main loop happen with SIGCHLD blocked, including allocation and release. */
static void child_changed(int sig)
{
    int saved_errno = errno, status;
    pid_t pid;
    (void)sig;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (int j = 0; j < MAXJOBS; j++) {
            for (size_t p = 0; p < job_table[j].count; p++) {
                struct process *proc = &job_table[j].processes[p];
                if (proc->pid != pid) continue;
                if (WIFEXITED(status) || WIFSIGNALED(status)) proc->done = 1;
                else proc->stopped = WIFSTOPPED(status);
            }
        }
    }
    errno = saved_errno;
}

static void forward_signal(int sig)
{
    int saved_errno = errno;
    if (foreground_group > 0) kill(-(pid_t)foreground_group, sig);
    errno = saved_errno;
}

static void install_handler(int sig, void (*handler)(int))
{
    struct sigaction action = {0};
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(sig, &action, NULL) < 0) { perror("sigaction"); exit(1); }
}

static int finished(const struct job *job)
{
    for (size_t p = 0; p < job->count; p++)
        if (!job->processes[p].done) return 0;
    return 1;
}

static int stopped(const struct job *job)
{
    int live = 0;
    for (size_t p = 0; p < job->count; p++) {
        if (job->processes[p].done) continue;
        live = 1;
        if (!job->processes[p].stopped) return 0;
    }
    return live;
}

static void discard_job(struct job *job)
{
    free(job->processes);
    free(job->command);
    memset(job, 0, sizeof(*job));
}

static void reap_jobs(void)
{
    child_changed(SIGCHLD);
    for (int j = 0; j < MAXJOBS; j++)
        if (job_table[j].pgid && finished(&job_table[j])) discard_job(&job_table[j]);
}

/* SIGCHLD is blocked on entry; sigsuspend atomically unblocks it while waiting. */
static void wait_foreground(struct job *job, const sigset_t *previous)
{
    sigset_t waiting = *previous;
    sigdelset(&waiting, SIGCHLD);
    while (!finished(job) && !stopped(job)) sigsuspend(&waiting);
    if (interactive) {
        if (tcsetpgrp(STDIN_FILENO, shell_group) < 0) perror("tcsetpgrp");
        if (stopped(job) && tcgetattr(STDIN_FILENO, &job->modes) == 0)
            job->has_modes = 1;
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_modes);
    }
    foreground_group = 0;
    if (finished(job)) discard_job(job);
}

static int job_index(char **args)
{
    char *end, *id;
    long number;
    if (!args[1] || args[2]) {
        fprintf(stderr, "%s: usage: %s <job> (exemple: %s %%1)\n", args[0], args[0], args[0]);
        return -1;
    }
    id = args[1];
    if (*id == '%') id++;
    for (char *c = id; *c; c++) {
        if (*c < '0' || *c > '9') {
            fprintf(stderr, "%s: numero de job invalide\n", args[0]);
            return -1;
        }
    }
    errno = 0;
    number = strtol(id, &end, 10);
    if (!*id || *end || errno || number < 1 || number > MAXJOBS) {
        fprintf(stderr, "%s: numero de job invalide\n", args[0]);
        return -1;
    }
    if (!job_table[number - 1].pgid) {
        fprintf(stderr, "%s: job inexistant: %ld\n", args[0], number);
        return -1;
    }
    return (int)number - 1;
}

static int is_builtin(const char *name)
{
    return !strcmp(name, "fg") || !strcmp(name, "bg") || !strcmp(name, "jobs") ||
           !strcmp(name, "cd") || !strcmp(name, "quit") || !strcmp(name, "kill");
}

static void shutdown_jobs(void)
{
    for (int j = 0; j < MAXJOBS; j++) {
        if (job_table[j].pgid) {
            kill(-job_table[j].pgid, SIGHUP);
            kill(-job_table[j].pgid, SIGCONT);
            discard_job(&job_table[j]);
        }
    }
}

static void builtin(char **args, const sigset_t *previous)
{
    if (!strcmp(args[0], "quit")) {
        shutdown_jobs();
        exit(0);
    }
    if (!strcmp(args[0], "cd")) {
        const char *path = args[1] ? args[1] : getenv("HOME");
        if (args[1] && args[2]) fprintf(stderr, "cd: trop d'arguments\n");
        else if (!path) fprintf(stderr, "cd: HOME non defini\n");
        else if (chdir(path) < 0) perror("cd");
        return;
    }
    if (!strcmp(args[0], "jobs")) {
        for (int j = 0; j < MAXJOBS; j++) {
            struct job *job = &job_table[j];
            if (job->pgid)
                printf("[%d]: PID=%ld (%s): %s\n", j + 1, (long)job->pgid,
                       stopped(job) ? "Stopped" : "Background", job->command);
        }
        return;
    }
    int index = job_index(args);
    if (index < 0) return;
    struct job *job = &job_table[index];
    if (!strcmp(args[0], "kill")) {
        if (kill(-job->pgid, SIGTERM) < 0) perror("kill");
        else kill(-job->pgid, SIGCONT);
        return;
    }
    int foreground = !strcmp(args[0], "fg");
    if (foreground) {
        foreground_group = job->pgid;
        if (interactive) {
            if (tcsetpgrp(STDIN_FILENO, job->pgid) < 0) {
                perror("fg"); foreground_group = 0; return;
            }
            if (job->has_modes) tcsetattr(STDIN_FILENO, TCSADRAIN, &job->modes);
        }
    }
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror(args[0]);
        if (foreground && interactive) {
            tcsetpgrp(STDIN_FILENO, shell_group);
            tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_modes);
        }
        foreground_group = 0;
        return;
    }
    for (size_t p = 0; p < job->count; p++) job->processes[p].stopped = 0;
    if (foreground) wait_foreground(job, previous);
    else printf("[%d] %ld\n", index + 1, (long)job->pgid);
}

static void redirect_file(const char *path, int output)
{
    if (!path) return;
    int fd = open(path, output ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY, 0644);
    if (fd < 0 || dup2(fd, output ? STDOUT_FILENO : STDIN_FILENO) < 0) {
        perror(path); _exit(1);
    }
    if (fd > STDERR_FILENO) close(fd);
}

static void launch(struct cmdline *line, const sigset_t *previous)
{
    int index, previous_fd = -1, gate[2];
    size_t count = 0;
    while (line->seq[count]) count++;
    for (index = 0; index < MAXJOBS && job_table[index].pgid; index++);
    if (index == MAXJOBS) { fprintf(stderr, "shell: table des jobs pleine\n"); return; }
    struct job *job = &job_table[index];
    job->processes = calloc(count, sizeof(*job->processes));
    job->command = strdup(line->seq[0][0]);
    if (!job->processes || !job->command) { perror("allocation"); discard_job(job); return; }
    /* Children wait for EOF on this pipe until their group owns the terminal.
     * This also keeps the group leader alive while the pipeline is assembled. */
    if (pipe(gate) < 0) { perror("pipe"); discard_job(job); return; }
    fflush(NULL);
    for (size_t p = 0; p < count; p++) {
        int fd[2] = {-1, -1};
        if (p + 1 < count && pipe(fd) < 0) { perror("pipe"); goto fail; }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (fd[0] >= 0) { close(fd[0]); close(fd[1]); }
            goto fail;
        }
        if (pid == 0) {
            if (setpgid(0, job->pgid) < 0) _exit(1);
            install_handler(SIGCHLD, SIG_DFL);
            install_handler(SIGINT, SIG_DFL);
            install_handler(SIGTSTP, SIG_DFL);
            install_handler(SIGQUIT, SIG_DFL);
            install_handler(SIGTTIN, SIG_DFL);
            install_handler(SIGTTOU, SIG_DFL);
            close(gate[1]);
            char dummy;
            while (read(gate[0], &dummy, 1) < 0 && errno == EINTR);
            close(gate[0]);
            if (previous_fd >= 0 && dup2(previous_fd, STDIN_FILENO) < 0) _exit(1);
            if (fd[1] >= 0 && dup2(fd[1], STDOUT_FILENO) < 0) _exit(1);
            if (previous_fd >= 0) close(previous_fd);
            if (fd[0] >= 0) { close(fd[0]); close(fd[1]); }
            if (p == 0) redirect_file(line->in, 0);
            if (p + 1 == count) redirect_file(line->out, 1);
            sigprocmask(SIG_SETMASK, previous, NULL);
            execvp(line->seq[p][0], line->seq[p]);
            perror(line->seq[p][0]);
            _exit(127);
        }
        if (!job->pgid) job->pgid = pid;
        job->processes[job->count++].pid = pid;
        if (setpgid(pid, job->pgid) < 0) {
            perror("setpgid");
            if (fd[0] >= 0) { close(fd[0]); close(fd[1]); }
            goto fail;
        }
        if (previous_fd >= 0) close(previous_fd);
        if (fd[1] >= 0) close(fd[1]);
        previous_fd = fd[0];
    }
    if (previous_fd >= 0) close(previous_fd);
    close(gate[0]);
    if (!line->background) {
        foreground_group = job->pgid;
        if (interactive && tcsetpgrp(STDIN_FILENO, job->pgid) < 0) perror("tcsetpgrp");
    }
    close(gate[1]);
    if (line->background) printf("[%d] %ld\n", index + 1, (long)job->pgid);
    else wait_foreground(job, previous);
    return;
fail:
    if (previous_fd >= 0) close(previous_fd);
    /* Kill every recorded PID too: a failed setpgid may leave one outside the group. */
    for (size_t p = 0; p < job->count; p++) kill(job->processes[p].pid, SIGKILL);
    close(gate[0]); close(gate[1]);
    for (size_t p = 0; p < job->count; p++)
        while (waitpid(job->processes[p].pid, NULL, 0) < 0 && errno == EINTR);
    discard_job(job);
}

int main(void)
{
    interactive = isatty(STDIN_FILENO);
    shell_group = getpgrp();
    if (interactive) {
        while (tcgetpgrp(STDIN_FILENO) != shell_group) kill(-shell_group, SIGTTIN);
        install_handler(SIGTTIN, SIG_IGN);
        install_handler(SIGTTOU, SIG_IGN);
        if (setpgid(0, 0) < 0 && errno != EPERM) { perror("setpgid"); return 1; }
        shell_group = getpgrp();
        if (tcsetpgrp(STDIN_FILENO, shell_group) < 0 ||
            tcgetattr(STDIN_FILENO, &shell_modes) < 0) { perror("terminal"); return 1; }
    }
    sigemptyset(&child_mask);
    sigaddset(&child_mask, SIGCHLD);
    install_handler(SIGCHLD, child_changed);
    install_handler(SIGINT, forward_signal);
    install_handler(SIGTSTP, forward_signal);
    install_handler(SIGQUIT, SIG_IGN);
    for (;;) {
        sigset_t previous;
        sigprocmask(SIG_BLOCK, &child_mask, &previous);
        reap_jobs();
        sigprocmask(SIG_SETMASK, &previous, NULL);
        printf("shell> "); fflush(stdout);
        struct cmdline *line = readcmd();
        sigprocmask(SIG_BLOCK, &child_mask, &previous);
        reap_jobs();
        if (!line) { shutdown_jobs(); printf("exit\n"); return 0; }
        if (line->err) fprintf(stderr, "error: %s\n", line->err);
        else if (line->seq && line->seq[0]) {
            int has_builtin = 0;
            for (size_t p = 0; line->seq[p]; p++) has_builtin |= is_builtin(line->seq[p][0]);
            if (has_builtin && (line->seq[1] || line->in || line->out || line->background))
                fprintf(stderr, "shell: commandes internes sans pipe, redirection ou & uniquement\n");
            else if (has_builtin) builtin(line->seq[0], &previous);
            else launch(line, &previous);
        }
        sigprocmask(SIG_SETMASK, &previous, NULL);
    }
}

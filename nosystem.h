#ifndef NOSYSTEM_H
#define NOSYSTEM_H

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern __thread FILE* nosystem_stdin;
extern __thread FILE* nosystem_stdout;
extern __thread FILE* nosystem_stderr;

typedef int NoSystemCommand(int argc, char** argv);

extern int nosystem_init(void);
extern void nosystem_addcommand(const char* cmd, NoSystemCommand* func);

extern int nosystem_system(const char* cmd);
extern int nosystem_execv(const char *pathname, char *const argv[]);
extern int nosystem_execvp(const char *pathname, char *const argv[]);
extern int nosystem_execve(const char *pathname, char *const argv[], char *const envp[]);
extern void nosystem_exit(int exit_code);
extern int nosystem_executable(const char* cmd);

extern pid_t nosystem_fork(void);
extern pid_t nosystem_currentPid(void);
extern pid_t nosystem_waitpid(pid_t pid, int *status, int options);

//extern char * nosystem_getenv(const char *name);
//extern int nosystem_setenv(const char* variableName, const char* value, int overwrite);
//int nosystem_unsetenv(const char* variableName);

extern int nosystem_isatty(int fd);
//extern void nosystem_settty(FILE* _tty);
//extern int nosystem_gettty(void);

#ifdef __cplusplus
}
#endif

#endif

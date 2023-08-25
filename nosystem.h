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

extern void nosystem_addcommand(const char* cmd, NoSystemCommand* func);

extern int nosystem_system(const char* cmd);
extern void nosystem_exit(int exit_code);
extern int nosystem_executable(const char* cmd);

extern pid_t nosystem_fork();
extern pid_t nosystem_currentPid();
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

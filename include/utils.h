#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <sys/types.h>

int write_stderr(const char *message);
int write_stdout(const char *message);
int write_stderr_nl(const char *message);
int write_stdout_nl(const char *message);
int write_stderr_nl_2(const char *message, int size);
int write_stdout_nl_2(const char *message, int size);
int write_msg(int fd, const char *message, int size);

char *join_path(char *path, char *filename);
char *join_path2(const char *path, char *filename);
int hasTrailingSlash(char *s);
int hasTrailingSlash2(const char *s);

const char *get_user(void);

pid_t fork_and_save(pid_t **pids_to_terminate, size_t *num_pids);

#endif // UTILS_H

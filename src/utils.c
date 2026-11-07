#include "../include/utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int write_msg(int fd, const char *message, int size) {
  if (fd < 0) {
    dprintf(STDERR_FILENO, "write_msg: bad FD=%d\n", fd);
    return -1;
  }

  if (write(fd, message, size) == -1) {
    dprintf(STDERR_FILENO, "write_msg: write failed (%s)\n", strerror(errno));
    return -1;
  }
  return 0;
}

int write_stderr(const char *message) {
  if (message == NULL) {
    return write(STDERR_FILENO, "(null)", 6);
  }
  if (write(STDERR_FILENO, message, strlen(message)) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }
  return 0;
}

int write_stderr_nl(const char *message) {
  if (message == NULL) {
    return write(STDERR_FILENO, "(null)", 6);
  }

  if (write(STDERR_FILENO, message, strlen(message)) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }

  if (write(STDERR_FILENO, "\n", 1) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }
  return 0;
}

int write_stderr_nl_2(const char *message, int size) {
  if (write(STDERR_FILENO, message, size) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }

  if (write(STDERR_FILENO, "\n", 1) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }
  return 0;
}

int write_stdout(const char *message) {
  if (message == NULL) {
    return write(STDERR_FILENO, "(null)", 6);
  }

  if (write(STDOUT_FILENO, message, strlen(message)) == -1) {
    perror("write : Erreur d'écriture");
    return -1;
  }
  return 0;
}

int write_stdout_nl(const char *message) {
  if (message == NULL) {
    return write(STDERR_FILENO, "(null)", 6);
  }

  if (write(STDOUT_FILENO, message, strlen(message)) == -1) {
    perror("write : Erreur d'écriture");
    return -1;
  }
  if (write(STDOUT_FILENO, "\n", 1) == -1) {
    perror("write : Erreur d'écriture");
    return -1;
  }
  return 0;
}

int write_stdout_nl_2(const char *message, int size) {
  if (write(STDOUT_FILENO, message, size) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }

  if (write(STDOUT_FILENO, "\n", 1) == -1) {
    perror("write : Erreur d'écriture.");
    return -1;
  }
  return 0;
}

char *join_path(char *path, char *filename) {
  size_t size_path = strlen(path);
  size_t size_filename = strlen(filename);

  size_t combined_size = size_path + size_filename + 2;

  char *combined_path = malloc(combined_size);
  if (!combined_path)
    return NULL;

  if (size_path > 0 && path[size_path - 1] == '/') {
    snprintf(combined_path, combined_size, "%s%s", path, filename);
  } else {
    snprintf(combined_path, combined_size, "%s/%s", path, filename);
  }

  return combined_path;
}

char *join_path2(const char *path, char *filename) {
  size_t size_path = strlen(path);
  size_t size_filename = strlen(filename);

  size_t combined_size = size_path + size_filename + 2;

  char *combined_path = malloc(combined_size);
  if (!combined_path)
    return NULL;

  if (size_path > 0 && path[size_path - 1] == '/') {
    snprintf(combined_path, combined_size, "%s%s", path, filename);
  } else {
    snprintf(combined_path, combined_size, "%s/%s", path, filename);
  }

  return combined_path;
}

int hasTrailingSlash(char *s) {
  size_t len = strlen(s);
  if (len == 0)
    return 0;
  return (s[len - 1] == '/');
}

int hasTrailingSlash2(const char *s) {
  size_t len = strlen(s);
  if (len == 0)
    return 0;
  return (s[len - 1] == '/');
}

const char *get_user(void) {
  const char *user = getenv("USER");
  if (user == NULL || user[0] == '\0') {
    user = getenv("LOGNAME");
  }
  return user;
}

pid_t fork_and_save(pid_t **pids_to_terminate, size_t *num_pids) {
  pid_t pid = fork();
  // only save valid pids
  if (pid > 0) {
    if (*pids_to_terminate == NULL) {
      *pids_to_terminate = malloc(sizeof(pid_t));
    } else {
      *pids_to_terminate = realloc(*pids_to_terminate, (*num_pids + 1) * sizeof(pid_t));
    }
    (*pids_to_terminate)[(*num_pids)++] = pid;
  }
  return pid;
}

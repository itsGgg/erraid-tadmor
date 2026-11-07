#include "../include/scheduler.h"
#include "../include/exec.h"
#include "../include/logger.h"
#include "../include/serialisation.h"
#include "../include/types.h"
#include "../include/utils.h"
#include <endian.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_refresh_needed = 0;

static void handle_refresh_signal(int sig) {
  (void)sig;
  g_refresh_needed = 1;
}

void execute_tasks(list_of_tasks **tasks_lists, char *path, pid_t **pids_to_terminate, size_t *num_pids) {
  if (tasks_lists == NULL || *tasks_lists == NULL || path == NULL) {
    return;
  }

  struct sigaction sa;
  sa.sa_handler = handle_refresh_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    LOG_ERR("[SCHEDULER] Failed to set up SIGUSR1 handler\n");
  }

  int last_min = -1;
  while (1) {
    if (g_refresh_needed) {
      g_refresh_needed = 0;
      list_of_tasks *new_list = NULL;
      if (alloc_tasks_list(&new_list) == 0 && new_list != NULL && read_tree(path, new_list)) {
        free_tasks_list(*tasks_lists);
        *tasks_lists = new_list;
      } else if (new_list) {
        free_tasks_list(new_list);
      }
    }
    
    time_t t = time(NULL);
    struct tm tm_buf;
    struct tm *tm = localtime_r(&t, &tm_buf);
    if (tm == NULL) {
      sleep(1);
      continue;
    }
    int current_day = (tm->tm_wday + 1) % 7;
    int current_hour = tm->tm_hour;
    int current_min = tm->tm_min;
    int current_sec = tm->tm_sec;
    if (current_sec != 0) {
      sleep(1);
      continue;
    }

    LOG_INFO("[SCHEDULER] Current_day : %i , current_hour : %i , current_min : %i , current_sec : %i \n", current_day,
             current_hour, current_min, current_sec);

    if (current_min == last_min) {
      LOG_INFO("[SCHEDULER] Attente pour passage à la prochaine minute. current_min : %i ; last_min : %i \n",
               current_min, last_min);
      sleep(1);
      continue;
    }
    last_min = current_min;
    LOG_INFO("[SCHEDULER] MAJ last_min : %i \n", last_min);

    if ((*tasks_lists)->tasks == NULL) {
      LOG_INFO("[SCHEDULER] Attente : liste vide ");
      sleep(1);
      continue;
    }

    LOG_INFO("[SCHEDULER] Parcours liste des tâches \n");
    for (uint32_t i = 0; i < (*tasks_lists)->count; i++) {
      if (i >= (*tasks_lists)->count || (*tasks_lists)->tasks[i] == NULL || (*tasks_lists)->tasks[i]->cmd == NULL) {
        continue;
      }
      if (should_execute((*tasks_lists)->tasks[i], current_day, current_hour, current_min)) {
        pid_t task_executor = fork_and_save(pids_to_terminate, num_pids);
        if (task_executor == -1) {
          continue;
        }

        if (task_executor == 0) {
          pid_t command_executor = fork_and_save(pids_to_terminate, num_pids);
          if (command_executor == -1) {
            exit(1);
          }

          if (command_executor == 0) { // Executor de commande
            print_command((*tasks_lists)->tasks[i]->cmd);

            char *stdout_path = malloc(strlen(path) + 256);
            char *stderr_path = malloc(strlen(path) + 256);
            if (!stdout_path || !stderr_path) {
              exit(1);
            }

            sprintf(stdout_path, "%s/tasks/%d/stdout", path, (*tasks_lists)->tasks[i]->id);
            sprintf(stderr_path, "%s/tasks/%d/stderr", path, (*tasks_lists)->tasks[i]->id);

            int fileSTDOUT = open(stdout_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            int fileSTDErr = open(stderr_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);

            free(stdout_path);
            free(stderr_path);

            if (fileSTDOUT >= 0) {
              dup2(fileSTDOUT, STDOUT_FILENO);
              close(fileSTDOUT);
            }
            if (fileSTDErr >= 0) {
              dup2(fileSTDErr, STDERR_FILENO);
              close(fileSTDErr);
            }

            int ret = exec_command((*tasks_lists)->tasks[i]->cmd, pids_to_terminate, num_pids);
            exit(ret);
          }

          // Task_executor attend command_executor
          int status;
          pid_t pid;
          while ((pid = waitpid(-1, &status, 0)) > 0) {
            if (WIFEXITED(status)) {
              int exit_code = WEXITSTATUS(status);
              LOG_INFO("Command_executor %d exited with %d", pid, exit_code);
            }
          }

          int exit_code = 0xFFFF;
          if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
          }

          char *tx_path = malloc(strlen(path) + 256);
          if (tx_path) {
            sprintf(tx_path, "%s/tasks/%d/times-exitcodes", path, (*tasks_lists)->tasks[i]->id);

            uint64_t timestamp = (uint64_t)time(NULL);
            uint64_t timestamp_be = htobe64(timestamp);
            uint16_t exit_code_be = ((uint16_t)exit_code >> 8) | ((uint16_t)exit_code << 8);

            int tx_file = open(tx_path, O_CREAT | O_WRONLY | O_APPEND, 0666);
            if (tx_file >= 0) {
              write(tx_file, &timestamp_be, 8);
              write(tx_file, &exit_code_be, 2);
              close(tx_file);
            }
            free(tx_path);
          }

          exit(0);
        }
      }
    }
    LOG_INFO("[SCHEDULER] Attente jusqu'a la prochaine minute \n");
    do {
      if (g_refresh_needed) {
        g_refresh_needed = 0;
        list_of_tasks *new_list = NULL;
        if (alloc_tasks_list(&new_list) == 0 && new_list != NULL && read_tree(path, new_list)) {
          free_tasks_list(*tasks_lists);
          *tasks_lists = new_list;
        } else if (new_list) {
          free_tasks_list(new_list);
        }
      }
      // Attente des processus pendant l'attente de la prochaine minute
      int status;
      pid_t pid;

      while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
          int exit_code = WEXITSTATUS(status);
          LOG_INFO("Task_executor %d exited with %d", pid, exit_code);
        }
      }

      sleep(1);
      t = time(NULL);
      tm = localtime_r(&t, &tm_buf);
      if (tm == NULL) {
        break;
      }

      // A pas decommenter à part si c'est pour test
      LOG_INFO("[SCHEDULER] Current_day : %i , current_hour : %i , current_min : %i , current_sec : %i \n", tm->tm_wday,
               tm->tm_hour, tm->tm_min, tm->tm_sec);

    } while (tm->tm_min == current_min);
  }
}

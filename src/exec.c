#include "../include/exec.h"
#include "../include/logger.h"
#include "../include/types.h"
#include "../include/utils.h"
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Il s'agit d'une fonction qui fait appel aux fonctions exec_simple_command et
// exec_sequence_command en fonction du type de la commande
int exec_command(command *cmd, pid_t **pids_to_terminate, size_t *num_pids) {
  if (cmd == NULL) {
    return -1;
  }
  int ret = 0;

  switch (cmd->type) {
  case CMD_TYPE_SI:
    ret = exec_simple_command(&cmd->simple, pids_to_terminate, num_pids);
    break;
  case CMD_TYPE_SQ:
    ret = exec_sequence_command(&cmd->sequence, pids_to_terminate, num_pids);
    break;
  case CMD_TYPE_IF:
    ret = exec_if_command(&cmd->if_cmd, pids_to_terminate, num_pids);
    break;
  case CMD_TYPE_PL:
    ret = exec_pipeline_command(&cmd->pipeline, pids_to_terminate, num_pids);
    break;
  default:
    write(2, "Erreur de type\n", 15);
    ret = -1;
    break;
  }
  return ret;
}

// La fonction qui gère l'exécution des taches simples

int exec_simple_command(simple_command *cmd, pid_t **pids_to_terminate, size_t *num_pids) {
  if (cmd == NULL || cmd->argv == NULL || cmd->argv[0] == NULL) {
    return -1;
  }

  pid_t p = fork_and_save(pids_to_terminate, num_pids);

  if (p == -1) {
    perror("Erreur de fork \n");
    return -1;
  }

  if (p == 0) {
    execvp(cmd->argv[0], cmd->argv);
    perror("Erreur d'exécution");
    exit(-1);
  }
  int status;
  waitpid(p, &status, 0);
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    return -1;
  }
  return 0;
}

// La fonction qui gére l'execution des taches complexes
int exec_sequence_command(sequence_command *cmd, pid_t **pids_to_terminate, size_t *num_pids) {
  if (cmd == NULL || cmd->sub_commands == NULL) {
    return -1;
  }
  int last_exit_code = 0;
  for (uint32_t i = 0; i < cmd->count; i++) {
    if (cmd->sub_commands[i] == NULL) {
      continue;
    }
    last_exit_code = exec_command(cmd->sub_commands[i], pids_to_terminate, num_pids);
    if (last_exit_code != 0) {
      break;
    }
  }
  return last_exit_code;
}
int exec_if_command(if_command *cmd, pid_t **pids_to_terminate, size_t *num_pids) {
  if (cmd == NULL) {
    return -1;
  }

  // 1) Exécuter les conditions (comme une séquence : stop au premier échec)
  int cond_status = 0;
  if (cmd->cond_count > 0 && cmd->cond != NULL) {
    for (uint32_t i = 0; i < cmd->cond_count; i++) {
      if (cmd->cond[i] == NULL) {
        continue;
      }
      cond_status = exec_command(cmd->cond[i], pids_to_terminate, num_pids);
      if (cond_status != 0) {
        break;
      }
    }
  }

  // 2) Choisir then/else en fonction du résultat
  command **branch = NULL;
  uint32_t branch_count = 0;

  if (cond_status == 0) {
    branch = cmd->then_cmd;
    branch_count = cmd->then_count;
  } else {
    branch = cmd->else_cmd;
    branch_count = cmd->else_count;
  }

  // 3) Exécuter la branche choisie (comme une séquence)
  int last_status = 0;
  if (branch_count > 0 && branch != NULL) {
    for (uint32_t i = 0; i < branch_count; i++) {
      if (branch[i] == NULL) {
        continue;
      }
      last_status = exec_command(branch[i], pids_to_terminate, num_pids);
      if (last_status != 0) {
        break;
      }
    }
  }

  return last_status;
}

// La fonction qui gère l'exécution des pipelines
int exec_pipeline_command(pipeline_command *cmd, pid_t **pids_to_terminate, size_t *num_pids) {
  if (cmd == NULL || cmd->sub_commands == NULL || cmd->count == 0) {
    return -1;
  }

  // juste une seule sous commande
  // malgre normalement il n'y a pas de pipeline avec une seule sous commande
  if (cmd->count == 1) {
    return exec_command(cmd->sub_commands[0], pids_to_terminate, num_pids);
  }

  // Sauvegarder stdout/stderr pour la dernière commande
  int saved_stdout = dup(STDOUT_FILENO);
  int saved_stderr = dup(STDERR_FILENO);

  int (*pipes)[2] = NULL;
  pid_t *pids = NULL;
  uint32_t num_forked = 0;

  // Créer les pipes pour connecter les commandes
  // Pour N commandes, on a besoin de N-1 pipes
  pipes = malloc((cmd->count - 1) * sizeof(int[2]));
  if (pipes == NULL) {
    if (saved_stdout >= 0)
      close(saved_stdout);
    if (saved_stderr >= 0)
      close(saved_stderr);
    return -1;
  }

  // Créer tous les pipes
  for (uint32_t i = 0; i < cmd->count - 1; i++) {
    if (pipe(pipes[i]) == -1) {
      perror("pipe");
      goto cleanup_error;
    }
  }

  pids = malloc(cmd->count * sizeof(pid_t));
  if (pids == NULL) {
    goto cleanup_error;
  }

  // execution
  for (uint32_t i = 0; i < cmd->count; i++) {
    if (cmd->sub_commands[i] == NULL) {
      pids[i] = -1;
      continue;
    }

    pids[i] = fork_and_save(pids_to_terminate, num_pids);
    if (pids[i] == -1) {
      perror("fork");
      goto cleanup_error;
    }

    num_forked = i + 1;

    if (pids[i] == 0) {
      // fils
      // si c'est pas la première commande, on redirige le stdin vers le pipe précédent
      if (i > 0) {
        if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
          perror("dup2 stdin");
          exit(1);
        }
      }

      // si c'est pas la dernière commande, on redirige le stdout vers le pipe suivant
      if (i < cmd->count - 1) {
        if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
          perror("dup2 stdout");
          exit(1);
        }
      } else {
        // Dernière commande restaurer stdout/stderr sauvegardés
        if (saved_stdout >= 0) {
          dup2(saved_stdout, STDOUT_FILENO);
        }
        if (saved_stderr >= 0) {
          dup2(saved_stderr, STDERR_FILENO);
        }
      }

      // Fermer les fds sauvegardés et tous les pipes
      if (saved_stdout >= 0)
        close(saved_stdout);
      if (saved_stderr >= 0)
        close(saved_stderr);
      for (uint32_t j = 0; j < cmd->count - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      free(pipes);
      free(pids);

      // Exécuter la commande
      int ret = exec_command(cmd->sub_commands[i], pids_to_terminate, num_pids);
      exit(ret);
    }
  }

  // Processus parent : fermer les fds sauvegardés et tous les pipes
  if (saved_stdout >= 0)
    close(saved_stdout);
  if (saved_stderr >= 0)
    close(saved_stderr);
  for (uint32_t i = 0; i < cmd->count - 1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }
  free(pipes);
  pipes = NULL;

  // Attendre tous les enfants
  int last_exit_code = 0;
  for (uint32_t i = 0; i < cmd->count; i++) {
    if (pids[i] > 0) {
      int status;
      waitpid(pids[i], &status, 0);
      if (i == cmd->count - 1) {
        // L'exit code du pipeline est celui de la dernière commande
        if (WIFEXITED(status)) {
          last_exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
          last_exit_code = -1;
        }
      }
    }
  }
  free(pids);

  return last_exit_code;

cleanup_error:
  // Fermer les fds sauvegardés
  if (saved_stdout >= 0)
    close(saved_stdout);
  if (saved_stderr >= 0)
    close(saved_stderr);

  // Fermer tous les pipes créés
  if (pipes != NULL) {
    for (uint32_t i = 0; i < cmd->count - 1; i++) {
      close(pipes[i][0]);
      close(pipes[i][1]);
    }
    free(pipes);
  }

  // Tuer les processus enfants déjà créés
  if (pids != NULL) {
    for (uint32_t i = 0; i < num_forked; i++) {
      if (pids[i] > 0) {
        kill(pids[i], SIGKILL);
        waitpid(pids[i], NULL, 0);
      }
    }
    free(pids);
  }

  return -1;
}

#include "../include/erraid.h"
#include "../include/logger.h"
#include "../include/scheduler.h"
#include "../include/serialisation.h"
#include "../include/types.h"
#include "../include/utils.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  LOG_INFO("Lancement du programme: LOG_FD : %i ; LOG_CURRENT_CTX : %s \n", LOG_FD,
           log_context_to_string(LOG_CURRENT_CTX));

  int path_provided = 0;
  char path[1024];
  path[sizeof(path) - 1] = '\0';

  int pipes_path_provided = 0;
  char pipes_path[1024];
  pipes_path[sizeof(pipes_path) - 1] = '\0';

  int opt = 0;
  opterr = 0;

  while ((opt = getopt(argc, argv, "FLSR:P:")) != -1) {
    switch (opt) {
    case 'F':
      g_activate_daemon = 0;
      LOG_INFO("Option -F (setsid() deactivated) recognised, g_activate_daemon : %i ; g_log_to_file : %i ; "
               "g_scheduler_log_enabled : %i \n",
               g_activate_daemon, g_log_to_file, g_scheduler_log_enabled);
      break;
    case 'L':
      g_log_to_file = 0;
      LOG_INFO("Option -L (No logging in file) recognised,  g_activate_daemon : %i ; g_log_to_file : %i ; "
               "g_scheduler_log_enabled : %i \n",
               g_activate_daemon, g_log_to_file, g_scheduler_log_enabled);
      break;
    case 'S':
      g_scheduler_log_enabled = 0;
      LOG_INFO("Option -S (Scheduler does not log) recognised,  g_activate_daemon : %i ; g_log_to_file : %i ; "
               "g_scheduler_log_enabled : %i \n",
               g_activate_daemon, g_log_to_file, g_scheduler_log_enabled);
      break;
    case 'R': // A changer en R, une fois que tests mis a jour
      if (optarg != NULL) {
        strncpy(path, optarg, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        LOG_INFO("Chemin utilisé : %s \n", path);
        path_provided = 1;
      } else {
        LOG_ERR("Option -R nécessite un argument \n");
        exit(EXIT_FAILURE);
      }
      break;
    case 'P':
      if (optarg != NULL) {
        strncpy(pipes_path, optarg, sizeof(pipes_path) - 1);
        pipes_path[sizeof(pipes_path) - 1] = '\0';
        LOG_INFO("Chemin pipes utilisé : %s \n", pipes_path);
        pipes_path_provided = 1;
      } else {
        LOG_ERR("Option -P nécessite un argument \n");
        exit(EXIT_FAILURE);
      }
      break;
    case '?': // option inconnue
      if (optopt == 'r' || optopt == 'P') {
        LOG_ERR("Option -%c nécessite un argument\n", optopt);
      } else {
        LOG_ERR("Option inconnue: -%c\n", optopt);
        print_options();
      }
      exit(EXIT_FAILURE);
    }
  }

  if (optind < argc) { // fallback pour des commandes de la forme " ./erraid a"
    LOG_ERR("Argument inattendu : %s \n", argv[optind]);
    exit(EXIT_FAILURE);
  }

  fallback_activated_daemon();

  if (path_provided == 0) {
    const char *user = get_user();
    snprintf(path, sizeof(path), "/tmp/%s/erraid", user);
    path[sizeof(path) - 1] = '\0';

    if (initialise_default_tree() == -1) {
      LOG_ERR("Erreur lors de la création de l'arborescence par défaut : /tmp/$USER/erraid \n");
      exit(EXIT_FAILURE);
    }
  }

  LOG_INFO(" g_activate_daemon : %i ; g_log_to_file : %i ; "
           "g_scheduler_log_enabled : %i \n",
           g_activate_daemon, g_log_to_file, g_scheduler_log_enabled);

  char *base_path = malloc(1024);
  if (!base_path) {
    write_stderr_nl("Erreur allocation base_path");
    return EXIT_FAILURE;
  }
  strncpy(base_path, path, 1024);
  base_path[1023] = '\0';

  list_of_tasks *tasks_list = NULL;
  if (alloc_tasks_list(&tasks_list) == -1) {
    write_stderr_nl("Erreur allocation tasks_list");
    free(base_path);
    return EXIT_FAILURE;
  }

  if (!read_tree(path, tasks_list)) {
    LOG_WARN("Aucune tâche existante trouvée dans %s \n", path);
  }

  print_task_list(tasks_list);

  char *pipes_base_path = pipes_path_provided ? pipes_path : NULL;
  if (create_main_demon(tasks_list, base_path, pipes_base_path) == -1) {
    write_stderr_nl("Erreur lors de la création du démon");
    free_tasks_list(tasks_list);
    free(base_path);
    return EXIT_FAILURE;
  }

  free_tasks_list(tasks_list);
  free(base_path);

  if (g_activate_daemon == 1) {
    return EXIT_SUCCESS;
  }

  while (1) {
    pause();
  }

  return EXIT_SUCCESS;
}

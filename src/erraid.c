#include "../include/erraid.h"
#include "../include/logger.h"
#include "../include/protocol.h"
#include "../include/scheduler.h"
#include "../include/serialisation.h"
#include "../include/utils.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t *pids_to_terminate = NULL;
size_t num_pids = 0;

static pid_t g_scheduler_pid = -1;

static void notify_scheduler_refresh() {
  if (g_scheduler_pid > 0) {
    kill(g_scheduler_pid, SIGUSR1);
  }
}

int initialise_default_tree() {
  char path[PATH_MAX];
  const char *user = get_user();
  if (!user) {
    LOG_ERR("get_user() : Erreur lors de la récupération du nom de l'utilisateur \n");
    return -1;
  }

  const char *subdirs[] = {"", "/erraid", "/erraid/tasks", "/erraid/pipes"};

  for (int i = 0; i < 4; i++) {
    if (snprintf(path, PATH_MAX, "/tmp/%s%s", user, subdirs[i]) >= PATH_MAX) {
      LOG_ERR("snprintf trop long pour %s \n", subdirs[i]);
      return -1;
    }

    if (ensure_dir(path) == -1) {
      LOG_ERR("Erreur création du répertoire : %s \n", path);
      return -1;
    }
  }

  return 0;
}

int ensure_dir(const char *path) {
  if (mkdir(path, 0755) == 0) {
    LOG_INFO("dir: %s created\n", path);
    return 0;
  }

  if (errno == EEXIST) {
    struct stat st;
    if (stat(path, &st) == -1) {
      LOG_ERR("stat failed on %s: %s\n", path, strerror(errno));
      return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
      LOG_ERR("%s exists but is not a directory\n", path);
      return -1;
    }

    LOG_INFO("dir: %s already exists\n", path);
    return 0;
  }

  if (errno == ENOENT) {
    char parent[PATH_MAX];
    strncpy(parent, path, PATH_MAX - 1);
    parent[PATH_MAX - 1] = '\0';

    // Trouver le dernier '/'
    char *last_slash = strrchr(parent, '/');
    if (last_slash != NULL && last_slash != parent) {
      *last_slash = '\0';

      if (ensure_dir(parent) == -1) {
        return -1;
      }
      if (mkdir(path, 0755) == 0) {
        LOG_INFO("dir: %s created\n", path);
        return 0;
      }
    }
  }

  LOG_ERR("mkdir %s failed: %s (errno=%d)\n", path, strerror(errno), errno);
  return -1;
}

int decode_single_bit_u64(uint64_t mask) {
  for (int i = 0; i < 64; i++) {
    if (mask & (1ULL << i))
      return i;
  }
  return -1;
}

int decode_single_bit_u32(uint32_t mask) {
  for (int i = 0; i < 32; i++) {
    if (mask & (1U << i))
      return i;
  }
  return -1;
}

int decode_single_bit_u8(uint8_t mask) {
  for (int i = 0; i < 8; i++) {
    if (mask & (1U << i))
      return i;
  }
  return -1;
}

void decode_file(int fd, int fd_out) {
  uint32_t nb_runs = 0;
  while (1) {
    uint64_t raw_ts;
    uint16_t raw_exit;

    if (read(fd, &raw_ts, 8) != 8)
      break;
    if (read(fd, &raw_exit, 2) != 2)
      break;

    nb_runs++;
  }

  uint32_t nb_runs_be = htobe32(nb_runs);
  write(fd_out, &nb_runs_be, sizeof(nb_runs_be));

  lseek(fd, 0, SEEK_SET);

  while (1) {
    uint64_t raw_ts;
    uint16_t raw_exit;

    if (read(fd, &raw_ts, 8) != 8)
      break;
    if (read(fd, &raw_exit, 2) != 2)
      break;

    uint64_t runtime = raw_ts;
    uint16_t exitcode = htobe16(raw_exit);

    write(fd_out, &runtime, sizeof(runtime));
    write(fd_out, &exitcode, sizeof(exitcode));
  }

  close(fd);
}

int create_main_demon(list_of_tasks *tasks_list, char *path, char *pipes_path) {
  int ret = 0;
  int fd_daemon_log = -1;

  pid_t pid = fork_and_save(&pids_to_terminate, &num_pids);

  if (pid == 0) {
    if (g_activate_daemon == 1) {
      if (setsid() == -1) {
        dprintf(2, "error : setsid() \n");
        exit(-1);
      }

      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);

      int dev_null = open("/dev/null", O_RDWR);
      if (dev_null != -1) {
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        if (dev_null > 2) {
          close(dev_null);
        }
      }

      if (g_log_to_file == 1) {
        init_erraid_log_file(path);
      }

      LOG_INFO("Mode démon activé,  g_activate_daemon : %i ; g_log_to_file : %i ; "
               "g_scheduler_log_enabled : %i \n",
               g_activate_daemon, g_log_to_file, g_scheduler_log_enabled);
    } else {
      if (g_log_to_file == 1) {
        init_erraid_log_file(pipes_path);
      }
      LOG_INFO("Mode démon désactivé \n");
    }

    char req_pipe[PATH_MAX];
    char rep_pipe[PATH_MAX];

    if (init_erraid_pipes(pipes_path, req_pipe, rep_pipe) == -1) {
      LOG_ERR("err : init_erraid_pipes()");
      return -1;
    }

    pid_t scheduler = create_scheduler(tasks_list, path);
    if (scheduler == -1) {
      LOG_ERR("Échec de création du scheduler\n");
      exit(1);
    }

    g_scheduler_pid = scheduler;

    while (1) {
      LOG_INFO("Attente de requêtes sur le tube %s...\n", req_pipe);
      int fd_in = open(req_pipe, O_RDONLY);

      if (fd_in == -1) {
        perror("open request pipe");
        break;
      }

      LOG_INFO("Lecture de la requête sur le tube %s...\n", req_pipe);
      while (1) {
        uint16_t opcode_raw;
        ssize_t nread = read(fd_in, &opcode_raw, sizeof(opcode_raw));

        if (nread <= 0) {
          close(fd_in);
          break;
        }

        uint16_t opcode = be16toh(opcode_raw);
        LOG_INFO("Opcode reçu : 0x%04X\n", opcode);

        switch (opcode) {
        // case 0x4C53: { // OPCODE_LIST - lister toutes les tâches
        case OPCODE_CREATE: {
          handle_create_task(fd_in, rep_pipe, tasks_list, path);
          break;
        }
        case OPCODE_LIST: {
          handle_list_tasks(fd_in, rep_pipe, tasks_list, path);

          break;
        }
        case OPCODE_STDOUT: { // #define OPCODE_STDOUT 0x534f     // 'SO'  STDOUT -- afficher la sortie standard de la
                              // dernière
                              // exécution complète d'une tâche
          handle_task_stdout(fd_in, rep_pipe, tasks_list, path);
          break;
        }

        case OPCODE_TERMINATE: { // #define OPCODE_TERMINATE 0x5452 // 'TR' -- arrêter le démon
          handle_terminate(fd_in, rep_pipe);
          close(fd_in);
          LOG_INFO("Daemon terminé avec succès \n");
          exit(0);
        }

          // case 0x5345: { // #define OPCODE_STDERR 0x5345     // 'SE' STDERR -- afficher la sortie erreur standard de
          // la dernière exécution complète d'une tâche
        case OPCODE_STDERR: {
          handle_task_stderr(fd_in, rep_pipe, tasks_list, path);
          break;
        }

        // case 0x5458: { // #define OPCODE_TIMES_EXIT 0x5458 // 'TX'
        case OPCODE_TIMES_EXIT: {
          handle_task_times_exit(fd_in, rep_pipe, tasks_list, path);
          break;
        }

        case OPCODE_REMOVE: {
          handle_remove_task(fd_in, rep_pipe, tasks_list, path);
          break;
        }

        case OPCODE_COMBINE: {
          handle_combine_tasks(fd_in, rep_pipe, tasks_list, path);
          break;
        }

        default:
          break;
        }
      }
    }

    exit(1);
  }

  return 0;
}

pid_t create_scheduler(list_of_tasks *tasks_list, char *path) {

  LOG_INFO("Scheduler crée \n");
  pid_t pid = fork_and_save(&pids_to_terminate, &num_pids);
  if (pid == 0) {
    // if (SCHEDULER_LOG_ENABLED == 1) {                     // Création du scheduler.log
    // if ((LOG_TO_FILE == 1) || (ACTIVATE_DAEMON == 0)) { // On écrit dans les fichiers de log : scheduler.log
    if (g_scheduler_log_enabled == 1) {
      if ((g_log_to_file == 1) || (g_activate_daemon == 0)) {
        init_scheduler_log_file(path);
      }
    } else {
      LOG_INFO("SCHEDULER_LOG_ENABLED = 0. Le scheduler ne va pas logger \n");
      close_fd();
      set_logger_ctx(LOG_CTX_NONE);
    }
    execute_tasks(&tasks_list, path, &pids_to_terminate, &num_pids);
    exit(0);
  }
  return pid;
}

int stockage_argument(int fd, command *cmd) {
  uint16_t type = htobe16(cmd->type);
  write(fd, &type, sizeof(type));
  int ret = 0;

  switch (cmd->type) {
  case CMD_TYPE_SI:
    ret = stockage_argument_si(fd, cmd);
    break;

  case CMD_TYPE_SQ:
    ret = stockage_argument_sq(fd, cmd);
    break;

  case CMD_TYPE_IF:
    ret = stockage_argument_if(fd, cmd);
    break;
  case CMD_TYPE_PL:
    ret = stockage_argument_pl(fd, cmd);
    break;
  default:
    write(2, "Erreur de type\n", 15);
    ret = -1;
    break;
  }

  return ret;
}

int stockage_argument_si(int fd, command *cmd) {

  uint32_t argc = htobe32(cmd->simple.argc);
  write(fd, &argc, sizeof(argc));

  for (uint32_t i = 0; i < cmd->simple.argc; i++) {
    uint32_t len = htobe32(strlen(cmd->simple.argv[i]));
    write(fd, &len, sizeof(len));
    write(fd, cmd->simple.argv[i], strlen(cmd->simple.argv[i]));
  }

  return 0;
}

int stockage_argument_sq(int fd, command *cmd) {
  uint32_t count = htobe32(cmd->sequence.count);
  write(fd, &count, sizeof(count));
  for (uint32_t i = 0; i < cmd->sequence.count; i++) {
    stockage_argument(fd, cmd->sequence.sub_commands[i]);
  }
  return 0;
}

int stockage_argument_pl(int fd, command *cmd) {
  uint32_t count = htobe32(cmd->pipeline.count);
  write(fd, &count, sizeof(count));
  for (uint32_t i = 0; i < cmd->pipeline.count; i++) {
    stockage_argument(fd, cmd->pipeline.sub_commands[i]);
  }
  return 0;
}
int stockage_argument_if(int fd, command *cmd) {
  if (!cmd || cmd->type != CMD_TYPE_IF)
    return -1;

  uint32_t cond_count = htobe32(cmd->if_cmd.cond_count);
  write(fd, &cond_count, sizeof(cond_count));
  for (uint32_t i = 0; i < cmd->if_cmd.cond_count; i++) {
    stockage_argument(fd, cmd->if_cmd.cond[i]);
  }

  uint32_t then_count = htobe32(cmd->if_cmd.then_count);
  write(fd, &then_count, sizeof(then_count));
  for (uint32_t i = 0; i < cmd->if_cmd.then_count; i++) {
    stockage_argument(fd, cmd->if_cmd.then_cmd[i]);
  }

  uint32_t else_count = htobe32(cmd->if_cmd.else_count);
  write(fd, &else_count, sizeof(else_count));
  for (uint32_t i = 0; i < cmd->if_cmd.else_count; i++) {
    stockage_argument(fd, cmd->if_cmd.else_cmd[i]);
  }

  return 0;
}
int init_erraid_pipes(const char *base_path, char req_pipe[], char rep_pipe[]) {
  char pipes_dir[PATH_MAX];

  if (base_path == NULL) {
    const char *user = get_user();
    snprintf(pipes_dir, sizeof(pipes_dir), "/tmp/%s/erraid/pipes", user);
  } else if (hasTrailingSlash2(base_path) == 1) {
    snprintf(pipes_dir, sizeof(pipes_dir), "%spipes", base_path);
  } else {
    snprintf(pipes_dir, sizeof(pipes_dir), "%s/pipes", base_path);
  }

  LOG_INFO("Pipes dir : %s \n", pipes_dir);

  if (strlen(pipes_dir) > PATH_MAX - 25) {
    LOG_ERR("Le chemin pipes_dir est trop long\n");
    return -1;
  }

  if (ensure_dir(pipes_dir) == -1) {
    LOG_ERR("ensure_dir() : erreur lors de la création de %s \n", pipes_dir);
    return -1;
  }

  int ret1 = snprintf(req_pipe, PATH_MAX, "%s/erraid-request-pipe", pipes_dir);
  if (ret1 >= PATH_MAX) {
    LOG_ERR("Le chemin req_pipe est trop long\n");
    return -1;
  }
  LOG_INFO("req_pipe path : %s \n", req_pipe);

  int ret2 = snprintf(rep_pipe, PATH_MAX, "%s/erraid-reply-pipe", pipes_dir);
  if (ret2 >= PATH_MAX) {
    LOG_ERR("Le chemin rep_pipe est trop long\n");
    return -1;
  }
  LOG_INFO("reply_pipe path : %s \n", rep_pipe);

  if (mkfifo(req_pipe, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo request");
  }
  if (mkfifo(rep_pipe, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo reply");
  }

  return 0;
}

int handle_list_tasks(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {

  int fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    close(fd_in);
    return -1;
  }

  // uint16_t anstype = htobe16(0x4F4B); // ANSTYPE_OK 0x4f4b    // 'OK'  la requête s'est exécutée normalement
  uint16_t anstype = htobe16(ANSTYPE_OK); // ANSTYPE_OK 0x4f4b    // 'OK'  la requête s'est exécutée normalement

  write(fd_out, &anstype, sizeof(anstype));

  uint32_t count = htobe32(tasks_list->count);
  write(fd_out, &count, sizeof(count));

  for (uint32_t i = 0; i < tasks_list->count; i++) {
    task *t = tasks_list->tasks[i];

    uint64_t id = htobe64(t->id);
    write(fd_out, &id, sizeof(id));

    uint64_t minutes_be = htobe64(t->timing.minutes);
    uint32_t hours_be = htobe32(t->timing.hours);
    uint8_t days_be = t->timing.days;

    write(fd_out, &minutes_be, sizeof(uint64_t));
    write(fd_out, &hours_be, sizeof(uint32_t));
    write(fd_out, &days_be, sizeof(uint8_t));

    stockage_argument(fd_out, t->cmd);
  }
  close(fd_out);
  LOG_INFO("Pipe d'écriture fermé. Liste des tâches envoyée avec succès \n");
  return 0;
}

int handle_task_stdout(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {
  uint64_t id_raw;
  read(fd_in, &id_raw, sizeof(id_raw));
  uint64_t id = be64toh(id_raw);

  int fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    return -1;
  }

  uint32_t count = tasks_list->count;

  for (uint32_t i = 0; i < count; i++) {
    task *t = tasks_list->tasks[i];

    if (id != t->id)
      continue;

    DIR *rp;
    char idpath[PATH_MAX];
    snprintf(idpath, sizeof(idpath), "%s/tasks/%lu", path ? path : ".", id);
    rp = opendir(idpath);

    if (rp == NULL) {
      uint16_t anstype = htobe16(ANSTYPE_ERROR);
      write(fd_out, &anstype, sizeof(anstype));
      uint16_t errcode = htobe16(ERRCODE_NOTRUN);
      write(fd_out, &errcode, sizeof(errcode));
      close(fd_out);
      goto tt;
    }

    struct dirent *entry;

    while ((entry = readdir(rp))) {
      if (strcmp(entry->d_name, "stdout") == 0) {
        char fullpath[PATH_MAX];
        int ret = snprintf(fullpath, sizeof(fullpath), "%s/%s", idpath, entry->d_name);
        if (ret <= 0 || ret >= (int)sizeof(fullpath))
          continue;
        int fd = open(fullpath, O_RDONLY);
        if (fd == -1)
          continue;
        struct stat st;
        if (fstat(fd, &st) == -1) {
          close(fd);
          continue;
        }
        off_t file_size = st.st_size;
        if (file_size < 0) {
          close(fd);
          continue;
        }
        char *s = malloc(file_size);
        if (!s) {
          close(fd);
          continue;
        }
        ssize_t n = read(fd, s, file_size);
        if (n < 0)
          n = 0;
        uint16_t anstype = htobe16(ANSTYPE_OK); // ANSTYPE_OK 0x4f4b    // 'OK'  la requête s'est exécutée normalement

        write(fd_out, &anstype, sizeof(anstype));
        uint32_t len = htobe32((uint32_t)n);
        write(fd_out, &len, sizeof(len));
        write(fd_out, s, n);
        free(s);
        close(fd);
        closedir(rp);
        close(fd_out);
        LOG_INFO("Stdout de la tâche %lu envoyé avec succès \n", id);
        goto tt;
      }
      continue;
    }

    closedir(rp);
    uint16_t anstype = htobe16(ANSTYPE_ERROR); // 'ER'
    write(fd_out, &anstype, sizeof(anstype));
    uint16_t errcode = htobe16(ERRCODE_NOTRUN); //'NR'
    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_WARN("Tâche %lu trouvée mais n'a pas encore été exécutée \n", id);

    goto tt;
  }

  uint16_t anstype = htobe16(ANSTYPE_ERROR); // 'ER'

  write(fd_out, &anstype, sizeof(anstype));
  uint16_t errcode = htobe16(ERRCODE_ENOEF); // 'NF'
  write(fd_out, &errcode, sizeof(errcode));
  close(fd_out);
  LOG_WARN("Tâche %lu non trouvée \n", id);

tt:
  return 0;
}

int handle_terminate(int fd_in, char rep_pipe[]) {
  int fd_out = open(rep_pipe, O_WRONLY);

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    close(fd_in);
    return -1;
  }

  LOG_INFO("Terminaison des processus fils...\n");

  // tuer le processus parent (le démon erraid) si -F est utilisé
  pid_t parent_pid = getppid();
  if (parent_pid > 1) {
    if (kill(parent_pid, SIGTERM) == 0) {
      LOG_INFO("Processus parent %d terminé \n", parent_pid);
    }
  }

  for (size_t i = 0; i < num_pids; i++) {
    if (kill(pids_to_terminate[i], SIGKILL) == -1) {
      LOG_ERR("Erreur lors de la terminaison du processus %d : %s\n", pids_to_terminate[i], strerror(errno));
      continue;
    }
    LOG_INFO("Processus %d tué \n", pids_to_terminate[i]);
  }

  uint16_t anstype = htobe16(ANSTYPE_OK); // ANSTYPE_OK 0x4f4b    // 'OK'  la requête s'est exécutée normalement

  write(fd_out, &anstype, sizeof(anstype));
  close(fd_out);
  return 0;
}

int handle_task_stderr(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {

  uint64_t id_raw;
  read(fd_in, &id_raw, sizeof(id_raw));
  uint64_t id = be64toh(id_raw);

  int fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    return -1;
  }

  uint32_t count = tasks_list->count;

  for (uint32_t i = 0; i < count; i++) {
    task *t = tasks_list->tasks[i];

    if (id != t->id)
      continue;
    DIR *rp;
    char idpath[PATH_MAX];
    snprintf(idpath, sizeof(idpath), "%s/tasks/%lu", path, id);
    rp = opendir(idpath);
    struct dirent *entry;

    while ((entry = readdir(rp))) {
      if (strcmp(entry->d_name, "stderr") == 0) {
        char fullpath[PATH_MAX];
        int ret = snprintf(fullpath, sizeof(fullpath), "%s/%s", idpath, entry->d_name);
        if (ret < 0 || ret >= (int)sizeof(fullpath))
          continue;
        int fd = open(fullpath, O_RDONLY);
        if (fd == -1)
          continue;
        struct stat st;
        if (fstat(fd, &st) == -1) {
          close(fd);
          continue;
        }
        off_t file_size = st.st_size;
        if (file_size < 0) {
          close(fd);
          continue;
        }
        char *s = malloc(file_size);
        if (!s) {
          close(fd);
          continue;
        }
        ssize_t n = read(fd, s, file_size);
        if (n < 0)
          n = 0;
        uint16_t anstype = htobe16(ANSTYPE_OK); // ANSTYPE_OK 0x4f4b    // 'OK'  la requête s'est exécutée normalement

        write(fd_out, &anstype, sizeof(anstype));
        uint32_t len = htobe32((uint32_t)n);
        write(fd_out, &len, sizeof(len));
        write(fd_out, s, n);
        free(s);
        close(fd_out);
        LOG_INFO("Stderr de la tâche %lu envoyé avec succès \n", id);
        goto tterr;
      }
      continue;
    }

    // uint16_t anstype = htobe16(0x4552); // #define ANSTYPE_ERROR 0x4552 // 'ER'
    uint16_t anstype = htobe16(ANSTYPE_ERROR); // 'ER'

    write(fd_out, &anstype, sizeof(anstype));
    // uint16_t errcode = htobe16(0x4e52); // #define ERRCODE_NOTRUN 0x4e52 // 'NR'
    uint16_t errcode = htobe16(ERRCODE_NOTRUN); //'NR'

    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_WARN("Tâche %lu trouvée mais n'a pas encore été exécutée \n", id);

    goto tterr;
  }
  // uint16_t anstype = htobe16(0x4552); // #define ANSTYPE_ERROR 0x4552 // 'ER'
  uint16_t anstype = htobe16(ANSTYPE_ERROR); // 'ER'

  write(fd_out, &anstype, sizeof(anstype));
  // uint16_t errcode = htobe16(0x4e46); // #define ERRCODE_ENOEF 0x4e46  // 'NF'
  uint16_t errcode = htobe16(ERRCODE_ENOEF); // 'NF'

  write(fd_out, &errcode, sizeof(errcode));
  close(fd_out);
  LOG_WARN("Tâche %lu non trouvée \n", id);

tterr:
  return 0;
}

int handle_task_times_exit(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {
  uint64_t id_raw;
  read(fd_in, &id_raw, sizeof(id_raw));
  uint64_t id = htobe64(id_raw);

  int fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    return -1;
  }

  int task_found = 0;
  for (uint32_t i = 0; i < tasks_list->count; i++) {
    if (tasks_list->tasks[i]->id == id) {
      task_found = 1;
      break;
    }
  }

  if (!task_found) {
    uint16_t anstype = htobe16(ANSTYPE_ERROR); // #define ANSTYPE_ERROR 0x4552 // 'ER'
    write(fd_out, &anstype, sizeof(anstype));
    uint16_t errcode = htobe16(ERRCODE_ENOEF); // #define ERRCODE_ENOEF 0x4e46  // 'NF'
    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_WARN("Tâche %lu non trouvée pour times-exitcodes \n", id);
    return -1;
  }

  char fullpath[PATH_MAX];
  snprintf(fullpath, sizeof(fullpath), "%s/tasks/%lu/times-exitcodes", path, id);

  int fd = open(fullpath, O_RDONLY);
  if (fd < 0) {
    uint16_t anstype = htobe16(ANSTYPE_ERROR); // #define ANSTYPE_ERROR 0x4552 // 'ER'
    write(fd_out, &anstype, sizeof(anstype));
    uint16_t errcode = htobe16(ERRCODE_ENOEF); // #define ERRCODE_ENOEF 0x4e46  // 'NF'
    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_WARN("Fichier times-exitcodes non trouvé pour tâche %lu \n", id);
    return -1;
  }

  uint16_t anstype = htobe16(ANSTYPE_OK); // #define ANSTYPE_OK 0x4f4b    // 'OK'
  write(fd_out, &anstype, sizeof(anstype));

  decode_file(fd, fd_out);
  close(fd_out);
  LOG_INFO("Times-exitcodes de la tâche %lu envoyés avec succès \n", id);
  return 0;
}

int handle_remove_task(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {

  uint64_t id_raw;
  read(fd_in, &id_raw, sizeof(id_raw));
  uint64_t id = htobe64(id_raw);
  uint16_t id_u16 = (uint16_t)id;

  int fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    return -1;
  }

  uint32_t index = -1;
  for (uint32_t i = 0; i < tasks_list->count; i++) {
    if (tasks_list->tasks[i]->id == id_u16) {
      index = i;
      break;
    }
  }

  if (((int)index) == -1) {
    uint16_t anstype = htobe16(ANSTYPE_ERROR);
    write(fd_out, &anstype, sizeof(anstype));
    uint16_t errcode = htobe16(ERRCODE_ENOEF);
    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_WARN("Tâche %lu non trouvée pour suppression \n", id);
    return -1;
  }

  char tasks_dir[PATH_MAX];
  snprintf(tasks_dir, sizeof(tasks_dir), "%s/tasks", path);

  remove_task(tasks_dir, tasks_list->tasks[index]);

  free_task(tasks_list->tasks[index]);

  for (uint32_t j = index; j + 1 < tasks_list->count; j++) {
    tasks_list->tasks[j] = tasks_list->tasks[j + 1];
  }
  tasks_list->count -= 1;

  if (tasks_list->count == 0) {
    free(tasks_list->tasks);
    tasks_list->tasks = NULL;
  } else {
    task **new_tasks = realloc(tasks_list->tasks, sizeof(task *) * tasks_list->count);
    if (new_tasks != NULL) {
      tasks_list->tasks = new_tasks;
    }
  }

  uint16_t anstype = htobe16(ANSTYPE_OK);
  write(fd_out, &anstype, sizeof(anstype));
  close(fd_out);

  LOG_INFO("Tâche %lu supprimée \n", id);
  notify_scheduler_refresh();
  return 0;
}

int handle_create_task(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {
  if (tasks_list == NULL || path == NULL) {
    LOG_ERR("null args \n");
    return -1;
  }
  int ret = 0;

  task *new_task;
  alloc_task(&new_task);
  if (new_task == NULL) {
    LOG_ERR("alloc_task failed \n");
    ret = -1;
    goto cleanup_return;
  }

  // Allouer la commande simple
  ret = alloc_command(CMD_TYPE_SI, &new_task->cmd);
  if (ret == -1) {
    LOG_ERR("alloc_command failed \n");
    goto cleanup_return;
  }

  ssize_t bytes = 0;

  int fd_out = -1;
  fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  ret = deserialise_task_timing(fd_in, &new_task);
  if (ret == -1) {
    LOG_ERR("deserialise_task_timing \n");
    goto cleanup_return;
  }

  LOG_INFO("After deserialise_task_timing, reading argc\n");

  uint32_t raw_count = 0;
  bytes = read(fd_in, &raw_count, sizeof(uint32_t));
  if (bytes != sizeof(uint32_t)) {
    LOG_ERR("Error reading count (bytes=%zd)\n", bytes);
    ret = -1;
    goto cleanup_return;
  }

  (*new_task).cmd->simple.argc = be32toh(raw_count);
  LOG_INFO("Read argc=%u\n", (*new_task).cmd->simple.argc);

  (*new_task).cmd->simple.argv = malloc((*new_task).cmd->simple.argc * sizeof(char *));
  if (!(*new_task).cmd->simple.argv) {
    LOG_ERR("Failed to allocate argv array\n");
    ret = -1;
    goto cleanup_return;
  }

  for (uint32_t i = 0; i < (*new_task).cmd->simple.argc; i++) {
    uint32_t raw_length = 0;
    uint32_t length = 0;
    bytes = read(fd_in, &raw_length, sizeof(uint32_t));
    if (bytes != sizeof(uint32_t)) {
      LOG_ERR("err reading length \n");
      ret = -1;
      goto cleanup_return;
    }
    length = be32toh(raw_length);

    LOG_INFO("Reading argv[%u], length=%u\n", i, length);

    (*new_task).cmd->simple.argv[i] = malloc(length + 1);
    char *argv = (*new_task).cmd->simple.argv[i];

    bytes = read(fd_in, argv, length);
    if (bytes != length) {
      LOG_ERR("bytes read != length \n");
      ret = -1;
      goto cleanup_return;
    }
    argv[length] = '\0';
    LOG_INFO("argv[%u] = '%s'\n", i, argv);
  }

  new_task->id = g_next_task_id;
  g_next_task_id++;

  LOG_INFO(" add_task_to_list start\n");

  // adding task to the curent list
  ret = add_task_to_list(tasks_list, new_task);
  if (ret == -1) {
    LOG_ERR("error : add_task_to_list() \n");
    goto cleanup_return;
  }
  LOG_INFO("add_task_to_list end \n");

  LOG_INFO(" store_task start\n");

  // add task to the tree
  ret = store_task(path, new_task);
  if (ret == -1) {
    LOG_ERR("error store_task() \n");
    goto cleanup_return;
  }
  LOG_INFO(" store_task end\n");

cleanup_return:
  if (ret == 0) {
    LOG_INFO("sending ok to tadmor \n");

    uint16_t anstype = htobe16(ANSTYPE_OK);
    write(fd_out, &anstype, sizeof(anstype));
    close(fd_out);
    LOG_INFO(" done sending\n");
    notify_scheduler_refresh();
  } else if (ret == -1) {
    LOG_INFO(" sending error message\n");
    uint16_t anstype = htobe16(ANSTYPE_ERROR);
    write(fd_out, &anstype, sizeof(anstype));
    LOG_INFO(" error message sent \n");

    LOG_INFO(" sending enoef \n");

    uint16_t errcode = htobe16(ERRCODE_ENOEF);
    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_INFO(" enoef code sent \n");
  }
  if (fd_in != -1) {
    close(fd_in);
  }

  if (fd_out != -1) {
    close(fd_out);
  }

  return ret;
}

int handle_combine_tasks(int fd_in, char rep_pipe[], list_of_tasks *tasks_list, char *path) {

  LOG_INFO("handle_combine_tasks() \n");

  int ret = 0;
  task *new_task = NULL;
  uint16_t *task_ids = NULL;

  uint32_t nb_tasks = 0;
  ssize_t bytes = 0;

  alloc_task(&new_task);
  if (new_task == NULL) {
    LOG_ERR("alloc_task failed \n");
    ret = -1;
    goto cleanup_return;
  }

  int fd_out = open(rep_pipe, O_WRONLY);
  LOG_INFO("Pipe d'écriture ouvert \n");

  LOG_INFO(" deserialise_task() start\n");
  ret = deserialise_task(fd_in, rep_pipe, &new_task);
  if (ret == -1) {
    LOG_ERR("error :deserialise_task ()\n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO(" deserialise_task() finished\n");

  // reading nb of ids

  bytes = read(fd_in, &nb_tasks, sizeof(uint32_t));
  if (bytes != sizeof(uint32_t)) {
    LOG_ERR("error reading nb of tasks \n");
    ret = -1;
    goto cleanup_return;
  }
  nb_tasks = be32toh(nb_tasks);
  LOG_INFO(" nb tasks lu : %i \n", nb_tasks);

  task_ids = malloc(nb_tasks * sizeof(uint16_t));
  if (task_ids == NULL) {
    LOG_ERR("error malloc \n");
    ret = -1;
    goto cleanup_return;
  }

  LOG_INFO("deserialise_task_id_list() start\n");

  // deserialising each id into the tab : task_ids
  if (deserialise_task_id_list(fd_in, task_ids, nb_tasks) == -1) {
    LOG_ERR("error : deserialise_task_id_list() \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO("deserialise_task_id_list end \n");

  LOG_INFO(" fill task from ids start \n");
  ret = fill_task_from_ids(&new_task, tasks_list, task_ids, nb_tasks);
  if (ret == -1) {
    LOG_ERR("error fill_task_from_ids() \n");
    goto cleanup_return;
  }
  LOG_INFO(" fill task from ids end\n");

  new_task->id = g_next_task_id;
  g_next_task_id++;

  LOG_INFO(" add_task_to_list start\n");

  // adding task to the curent list
  ret = add_task_to_list(tasks_list, new_task);
  if (ret == -1) {
    LOG_ERR("error : add_task_to_list() \n");
    goto cleanup_return;
  }
  LOG_INFO("add_task_to_list end \n");

  LOG_INFO(" store_task start\n");

  // add task to the tree
  ret = store_task(path, new_task);
  if (ret == -1) {
    LOG_ERR("error store_task() \n");
    goto cleanup_return;
  }
  LOG_INFO(" store_task end\n");

  if (fd_out == -1) {
    LOG_ERR("Erreur: impossible d'ouvrir la pipe de réponse \n");
    goto cleanup_return;
  }

cleanup_return:
  if (ret == 0) {
    LOG_INFO("sending ok to tadmor \n");

    uint16_t anstype = htobe16(ANSTYPE_OK);
    write(fd_out, &anstype, sizeof(anstype));
    close(fd_out);
    LOG_INFO(" done sending\n");
    notify_scheduler_refresh();
  } else if (ret == -1) {
    if (new_task != NULL) {
      free_task(new_task);
    }
    LOG_INFO(" sending error message\n");
    uint16_t anstype = htobe16(ANSTYPE_ERROR);
    write(fd_out, &anstype, sizeof(anstype));
    LOG_INFO(" error message sent \n");

    LOG_INFO(" sending enoef \n");

    uint16_t errcode = htobe16(ERRCODE_ENOEF);
    write(fd_out, &errcode, sizeof(errcode));
    close(fd_out);
    LOG_INFO(" enoef code sent \n");
  }

  if (task_ids != NULL) {
    free(task_ids);
  }

  if (fd_out != -1) {
    close(fd_out);
  }
  if (fd_in != -1) {
    close(fd_in);
  }

  return ret;
} // OPCODE_COMBINE (0x4342) Combiner des tâches

int deserialise_task(int fd_in, char rep_pipe[], task **new_task) {
  if (new_task == NULL) {
    LOG_ERR("Null args \n");
    return -1;
  }

  LOG_INFO(" deserialise_task_timing start\n");

  // deserialising timing
  if (deserialise_task_timing(fd_in, new_task) == -1) {
    LOG_ERR("error : deserialise_task_timing() \n");
    return -1;
  }
  LOG_INFO(" deserialise_task_timing end \n");

  LOG_INFO(" deserialise_command_type\n");
  // deserialising cmd type
  if (deserialise_command_type(fd_in, new_task) == -1) {
    LOG_ERR("error : deserialise_task_type() \n");
    return -1;
  }
  LOG_INFO(" deserialise_command_type finished \n");

  return 0;
}

int fill_task_from_ids(task **new_task, list_of_tasks *tasks_list, uint16_t *task_ids,
                       uint32_t nb_tasks) { // to add type of command too here
  if (!new_task || !*new_task || !tasks_list || !tasks_list->tasks || !task_ids) {
    LOG_ERR("fill_task_from_ids: arguments invalides\n");
    return -1;
  }
  LOG_INFO("fill_task_from_ids() \n");
  if (!(*new_task)->cmd) {
    LOG_ERR("fill_task_from_ids: new_task->cmd est NULL (type non initialisé)\n");
    return -1;
  }

  int ret = 0;
  task *task_to_copy;
  uint32_t new_cmd_count = 0;

  switch ((*new_task)->cmd->type) {

  case CMD_TYPE_SQ:
    // malloc puis remplissage

    (*new_task)->cmd->sequence.count = nb_tasks;
    (*new_task)->cmd->sequence.sub_commands = malloc(nb_tasks * sizeof(command *));
    if (!(*new_task)->cmd->sequence.sub_commands) {
      LOG_ERR("fill_task_from_ids: malloc sub_commands\n");
      ret = -1;
      goto cleanup_return;
    }

    for (uint32_t i = 0; i < nb_tasks; i++) {

      task_to_copy = get_task_from_list(tasks_list, task_ids[i]);
      if (task_to_copy == NULL || task_to_copy->cmd == NULL) {
        LOG_ERR("fill_task_from_ids: task source introuvable ou NULL pour id %u\n", task_ids[i]);
        ret = -1;
        goto cleanup_return;
      }
      ret = copy_command(&(task_to_copy->cmd), task_to_copy->cmd->type, ((*new_task)->cmd->sequence.sub_commands),
                         (*new_task)->cmd->type, new_cmd_count);
      if (ret == -1) {
        LOG_ERR("error : copy_command \n");
        goto cleanup_return;
      }
      new_cmd_count++;
    }

    break;
  case CMD_TYPE_PL:
    // malloc puis remplissage
    (*new_task)->cmd->pipeline.count = nb_tasks;
    (*new_task)->cmd->pipeline.sub_commands = malloc(nb_tasks * sizeof(command *));
    if (!(*new_task)->cmd->pipeline.sub_commands) {
      LOG_ERR("error: malloc() \n");
      ret = -1;
      goto cleanup_return;
    }
    new_cmd_count = 0;

    for (uint32_t i = 0; i < nb_tasks; i++) {

      task *task_to_copy = get_task_from_list(tasks_list, task_ids[i]);
      if (task_to_copy == NULL || task_to_copy->cmd == NULL) {
        LOG_ERR("fill_task_from_ids: task d'id %u pas trouvé\n", task_ids[i]);
        ret = -1;
        goto cleanup_return;
      }
      ret = copy_command(&(task_to_copy->cmd), task_to_copy->cmd->type, ((*new_task)->cmd->pipeline.sub_commands),
                         (*new_task)->cmd->type, new_cmd_count);
      if (ret == -1) {
        LOG_ERR("error : copy_command \n");
        goto cleanup_return;
      }
      new_cmd_count++;
    }
    break;
  case CMD_TYPE_IF:
    if (nb_tasks < 2 || nb_tasks > 3) {
      LOG_ERR("fill_task_from_ids: nb_taches : %u \n", nb_tasks);
      ret = -1;
      goto cleanup_return;
    }
    // mallocs
    (*new_task)->cmd->if_cmd.cond_count = 1;
    (*new_task)->cmd->if_cmd.cond = malloc(1 * sizeof(command *));
    if (!(*new_task)->cmd->if_cmd.cond) {
      LOG_ERR("error : malloc()");
      ret = -1;
      goto cleanup_return;
    }

    (*new_task)->cmd->if_cmd.then_count = 1;
    (*new_task)->cmd->if_cmd.then_cmd = malloc(1 * sizeof(command *));
    if (!(*new_task)->cmd->if_cmd.then_cmd) {
      LOG_ERR("error : malloc()");
      ret = -1;
      goto cleanup_return;
    }

    if (nb_tasks == 3) {
      (*new_task)->cmd->if_cmd.else_count = 1;
      (*new_task)->cmd->if_cmd.else_cmd = malloc(1 * sizeof(command *));
      if (!(*new_task)->cmd->if_cmd.else_cmd) {
        LOG_ERR("error : malloc()");
        ret = -1;
        goto cleanup_return;
      }
    } else {
      (*new_task)->cmd->if_cmd.else_count = 0;
      (*new_task)->cmd->if_cmd.else_cmd = NULL;
    }

    // copie de chaque tache dans la partie correspondante
    // tâche[0] - cond
    task_to_copy = get_task_from_list(tasks_list, task_ids[0]);
    if (task_to_copy == NULL || task_to_copy->cmd == NULL) {
      LOG_ERR("fill_task_from_ids: cond task not found or NULL\n");
      ret = -1;
      goto cleanup_return;
    }
    ret = copy_command(&(task_to_copy->cmd), task_to_copy->cmd->type, ((*new_task)->cmd->if_cmd.cond),
                       task_to_copy->cmd->type, 0);
    if (ret == -1) {
      LOG_ERR("error : copy_command \n");
      goto cleanup_return;
    }

    // tache[1] - then
    task_to_copy = get_task_from_list(tasks_list, task_ids[1]);
    if (task_to_copy == NULL || task_to_copy->cmd == NULL) {
      LOG_ERR("fill_task_from_ids: then task not found or NULL\n");
      ret = -1;
      goto cleanup_return;
    }
    ret = copy_command(&(task_to_copy->cmd), task_to_copy->cmd->type, ((*new_task)->cmd->if_cmd.then_cmd),
                       task_to_copy->cmd->type, 0);
    if (ret == -1) {
      LOG_ERR("error : copy_command \n");
      goto cleanup_return;
    }

    // tâche[2] - else (si nb_tasks==3)
    if (nb_tasks == 3) {
      task_to_copy = get_task_from_list(tasks_list, task_ids[2]);
      if (task_to_copy == NULL || task_to_copy->cmd == NULL) {
        LOG_ERR("fill_task_from_ids: else task not found or NULL\n");
        ret = -1;
        goto cleanup_return;
      }
      ret = copy_command(&(task_to_copy->cmd), task_to_copy->cmd->type, ((*new_task)->cmd->if_cmd.else_cmd),
                         task_to_copy->cmd->type, 0);
      if (ret == -1) {
        LOG_ERR("error : copy_command \n");
        goto cleanup_return;
      }
    }
    break;
  default:
    LOG_ERR("fill_task_from_ids: type invalide %u\n", (*new_task)->cmd->type);
    ret = -1;
    break;
  }

cleanup_return:
  return ret;
}

task *get_task_from_list(list_of_tasks *list, uint16_t id) {
  if (!list || !list->tasks)
    return NULL;

  for (uint32_t i = 0; i < list->count; i++) {
    if (list->tasks[i] && list->tasks[i]->id == id) {
      LOG_INFO("Found task of id %u in tasks_list \n", id);
      return list->tasks[i];
    }
  }
  LOG_ERR("error : task of id %u couldn't be found \n", id);
  return NULL; // not found
}

int deserialise_task_id_list(int fd_in, uint16_t *task_ids, int nb_tasks) {
  if (nb_tasks < 0 || nb_tasks == 0) {
    LOG_ERR("nb_tasks not initialised \n");
    return -1;
  }

  ssize_t bytes = 0;
  uint16_t raw_id = 0;
  for (int i = 0; i < nb_tasks; i++) {

    bytes = read(fd_in, &raw_id, sizeof(uint16_t));
    if (bytes != sizeof(uint16_t)) {
      LOG_ERR("Couldn't read id properly \n");
      return -1;
    }
    task_ids[i] = be16toh(raw_id);
    LOG_INFO("Index %i - id : %u \n", i, (unsigned int)task_ids[i]);
  }

  return 0;
}

int deserialise_command_type(int fd_in, task **new_task) {
  if (new_task == NULL) {
    LOG_ERR("new_task is null \n");
    return -1;
  }

  uint16_t raw_type = 0;
  ssize_t bytes = 0;

  bytes = read(fd_in, &raw_type, sizeof(uint16_t));
  if (bytes != sizeof(uint16_t)) {
    LOG_ERR("err reading task type");
    return -1;
  }
  LOG_INFO("read bytes : %i \n", (unsigned int)bytes);

  uint16_t cmd_type = be16toh(raw_type);
  int ret = alloc_command(cmd_type, &(*new_task)->cmd);
  if (ret == -1) {
    LOG_ERR("error alloc_command \n");
    return -1;
  }

  return 0;
}

int deserialise_task_timing(int fd_in, task **new_task) {
  LOG_INFO("deserialise_task_timing() deb \n");
  if (new_task == NULL) {
    LOG_ERR("new_task is null \n");
    return -1;
  }
  LOG_INFO("deserialise_task_timing() apres verif null \n");

  uint64_t raw_mins = 0;
  uint32_t raw_hours = 0;
  ssize_t bytes = 0;

  LOG_INFO("before first read \n");

  bytes = read(fd_in, &raw_mins, sizeof(uint64_t));

  if (bytes == 0) {
    LOG_INFO("minutes bytes is 0 \n");
  }
  if (bytes < 0) {
    LOG_INFO("minutes bytes is <0 \n");
  }
  if (bytes > 0) {
    LOG_INFO("minutes bytes is >0  : %zu \n", bytes);
  }
  LOG_WARN("new_task=%p \n", (void *)new_task);
  LOG_WARN("*new_task=%p \n", (void *)(*new_task));

  (*new_task)->timing.minutes = be64toh(raw_mins);

  LOG_INFO("present!!!! \n");

  LOG_INFO(" deserialise minutes %llu \n", (unsigned long long)(*new_task)->timing.minutes);

  bytes = read(fd_in, &raw_hours, sizeof(uint32_t));
  if (bytes != sizeof(uint32_t)) {
    LOG_ERR("Err reading hours from pipe \n");
    return -1;
  }
  (*new_task)->timing.hours = be32toh(raw_hours);

  LOG_INFO(" deserialise hours %u \n", (*new_task)->timing.hours);

  if (read(fd_in, &(*new_task)->timing.days, sizeof(uint8_t)) != sizeof(uint8_t)) {
    LOG_ERR("Err reading day of execution from pipe: %s\n", strerror(errno));
    return -1;
  }

  LOG_INFO("read days %i \n", (*new_task)->timing.days);

  return 0;
}

int store_timing(char *task_path, task *task) {

  if (task_path == NULL || task == NULL) {
    LOG_ERR("Given arguments are null \n");
    return -1;
  }

  LOG_INFO("Debut stockage du timing d'exécution dans un fichier \n");

  int ret = 0;
  char timing_path[PATH_MAX];
  snprintf(timing_path, PATH_MAX, "%s/%s", task_path, "timing");

  int timing_fd = -1;
  timing_fd = open(timing_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (timing_fd == -1) {
    LOG_ERR("error :open() \n");
    return -1;
  }

  // Conversion host-to-big-endian puis écriture
  uint64_t minutes_be = htobe64(task->timing.minutes);
  if (write(timing_fd, &minutes_be, sizeof(uint64_t)) == -1) {
    LOG_ERR("Error when writing minutes \n");
    ret = -1;
    goto cleanup_return;
  }

  uint32_t hours_be = htobe32(task->timing.hours);
  if (write(timing_fd, &hours_be, sizeof(uint32_t)) == -1) {
    LOG_ERR("Error when writing hours \n");
    ret = -1;
    goto cleanup_return;
  }

  if (write(timing_fd, &task->timing.days, sizeof(uint8_t)) == -1) {
    LOG_ERR("Error when writing day of execution \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO("Stockage du timing d'exécution dans un fichier fini \n");

cleanup_return:
  if (timing_fd != -1) {
    close(timing_fd);
  }

  return ret;
}

int store_task(char *path, task *task) {

  char task_path[PATH_MAX];
  // tmp-username-erraid/tasks/task_id

  if (hasTrailingSlash(path) == 1) {
    snprintf(task_path, sizeof(task_path), "%stasks/%i", path, task->id);
  } else {
    snprintf(task_path, sizeof(task_path), "%s/tasks/%i", path, task->id);
  }

  LOG_INFO("task_path : %s", task_path);

  if (path == NULL || task == NULL || task->cmd == NULL) {
    LOG_ERR("Null args were given");
    return -1;
  }
  if (ensure_dir(task_path) == -1) { // tmp-username-erraid/tasks/task_id dir created
    LOG_ERR("error ensure_dir()");
    return -1;
  }

  char cmd_path[PATH_MAX];
  int cmd_len = snprintf(cmd_path, sizeof(cmd_path), "%s/%s", task_path, "cmd");
  if (cmd_len < 0 || cmd_len >= (int)sizeof(cmd_path)) {
    LOG_ERR("Error when building cmd path (too long)");
    return -1;
  }

  if (ensure_dir(cmd_path) == -1) {
    LOG_ERR("Error when creating directory : %s", cmd_path);
    return -1;
  }

  if (store_timing(task_path, task) == -1) {
    LOG_ERR("error : store_timing() failed \n");
    return -1;
  }

  return store_command(cmd_path, task->cmd);
}

int store_command(char *path, command *cmd) {
  int ret = 0;

  switch (cmd->type) {
  case CMD_TYPE_SI:
    ret = store_simple_command(path, cmd);
    if (ret == -1) {
      LOG_ERR("error store_simple_command \n");
    }
    break;

  case CMD_TYPE_SQ:
    ret = store_sequence_command(path, cmd);
    if (ret == -1) {
      LOG_ERR("error store_sequence_command \n");
    }
    break;

  case CMD_TYPE_PL:
    ret = store_pipeline_command(path, cmd);
    if (ret == -1) {
      LOG_ERR("error store_pipeline_command \n");
    }
    break;
  case CMD_TYPE_IF:
    ret = store_if_command(path, cmd);
    if (ret == -1) {
      LOG_ERR("error store_if_command \n");
    }
    break;
  default:
    LOG_ERR("CMD type not recognised : %i \n", cmd->type);
    ret = -1;
  }

  return ret;
}

int store_sequence_command(char *path, command *cmd) {
  if (path == NULL || cmd == NULL) {
    LOG_ERR("Null args");
    return -1;
  }

  if (cmd->type != CMD_TYPE_SQ) {
    LOG_ERR("store_sequence_command: wrong type %u\n", cmd->type);
    return -1;
  }
  if (store_type(path, CMD_TYPE_SQ) == -1) {
    LOG_ERR("store_sequence_command: failed to write type\n");
    return -1;
  }

  // write each sub command in the tree
  uint32_t cmd_count = cmd->sequence.count;
  if (cmd_count > 0 && cmd->sequence.sub_commands == NULL) {
    LOG_ERR("store_sequence_command: sub_commands is NULL for count %u\n", cmd_count);
    return -1;
  }
  for (uint32_t i = 0; i < cmd_count; i++) {
    char child_path[PATH_MAX];
    int written = snprintf(child_path, PATH_MAX, "%s/%u", path, i);
    LOG_INFO("sequence_cmd path : %s \n", child_path);
    if (written <= 0 || written >= PATH_MAX) {
      LOG_ERR("store_sequence_command: path too long\n");
      return -1;
    }
    if (ensure_dir(child_path) == -1) {
      LOG_ERR("store_sequence_command: cannot create %s\n", child_path);
      return -1;
    }
    if (store_command(child_path, cmd->sequence.sub_commands[i]) == -1) {
      LOG_ERR("store_sequence_command failed at index %u \n", i);
      return -1;
    }
  }

  return 0;
}

int store_pipeline_command(char *path, command *cmd) {
  if (path == NULL || cmd == NULL) {
    LOG_ERR("Null args");
    return -1;
  }

  if (cmd->type != CMD_TYPE_PL) {
    LOG_ERR("store_pipeline_command: wrong type %u\n", cmd->type);
    return -1;
  }

  if (store_type(path, CMD_TYPE_PL) == -1) {
    LOG_ERR("store_pipeline_command: failed to write type file\n");
    return -1;
  }

  uint32_t cmd_count = cmd->pipeline.count;
  if (cmd_count > 0 && cmd->pipeline.sub_commands == NULL) {
    LOG_ERR("store_pipeline_command: sub_commands is NULL for count %u\n", cmd_count);
    return -1;
  }
  for (uint32_t i = 0; i < cmd_count; i++) {
    char child_path[PATH_MAX];
    int written = snprintf(child_path, PATH_MAX, "%s/%u", path, i);
    if (written <= 0 || written >= PATH_MAX) {
      LOG_ERR("store_pipeline_command: path too long\n");
      return -1;
    }
    if (ensure_dir(child_path) == -1) {
      LOG_ERR("store_pipeline_command: cannot create %s\n", child_path);
      return -1;
    }
    if (store_command(child_path, cmd->pipeline.sub_commands[i]) == -1) {
      LOG_ERR("error store_pipeline_command failed at index %u \n", i);
      return -1;
    }
  }

  return 0;
}

int store_if_command(char *path, command *cmd) {
  if (path == NULL || cmd == NULL) {
    LOG_ERR("Null args");
    return -1;
  }

  if (cmd->type != CMD_TYPE_IF) {
    LOG_ERR("store_if_command: wrong type %u\n", cmd->type);
    return -1;
  }

  if (store_type(path, CMD_TYPE_IF) == -1) {
    LOG_ERR("store_if_command: failed to write type\n");
    return -1;
  }
  const char *cond_dir = "cond";
  const char *then_dir = "then";
  const char *else_dir = "else";

  char base_cond_path[PATH_MAX];
  char base_then_path[PATH_MAX];
  char base_else_path[PATH_MAX];

  if (snprintf(base_cond_path, PATH_MAX, "%s/%s", path, cond_dir) >= PATH_MAX ||
      snprintf(base_then_path, PATH_MAX, "%s/%s", path, then_dir) >= PATH_MAX ||
      snprintf(base_else_path, PATH_MAX, "%s/%s", path, else_dir) >= PATH_MAX) {
    LOG_ERR("store_if_command: base path too long\n");
    return -1;
  }

  if (ensure_dir(base_cond_path) == -1 || ensure_dir(base_then_path) == -1 || ensure_dir(base_else_path) == -1) {
    LOG_ERR("store_if_command: cannot create base cond/then/else dirs\n");
    return -1;
  }

  uint32_t cond_count = cmd->if_cmd.cond_count;
  uint32_t then_count = cmd->if_cmd.then_count;
  uint32_t else_count = cmd->if_cmd.else_count;

  if ((cond_count > 0 && cmd->if_cmd.cond == NULL) || (then_count > 0 && cmd->if_cmd.then_cmd == NULL) ||
      (else_count > 0 && cmd->if_cmd.else_cmd == NULL)) {
    LOG_ERR("store_if_command: missing subcommand arrays (cond %u then %u else %u)\n", cond_count, then_count,
            else_count);
    return -1;
  }

  for (uint32_t i = 0; i < cond_count; i++) {
    char cond_path[PATH_MAX];
    int written = snprintf(cond_path, PATH_MAX, "%s/%u", base_cond_path, i);
    if (written <= 0 || written >= PATH_MAX) {
      LOG_ERR("store_if_command: cond path too long\n");
      return -1;
    }
    if (ensure_dir(cond_path) == -1) {
      LOG_ERR("store_if_command: cannot create %s\n", cond_path);
      return -1;
    }
    if (store_command(cond_path, cmd->if_cmd.cond[i]) == -1) {
      LOG_ERR("error if_cmd : on 'cond' index %u : store_command ()\n", i);
      return -1;
    }
  }

  for (uint32_t i = 0; i < then_count; i++) {
    char then_path[PATH_MAX];
    int written = snprintf(then_path, PATH_MAX, "%s/%u", base_then_path, i);
    if (written <= 0 || written >= PATH_MAX) {
      LOG_ERR("store_if_command: then path too long\n");
      return -1;
    }
    if (ensure_dir(then_path) == -1) {
      LOG_ERR("store_if_command: cannot create %s\n", then_path);
      return -1;
    }
    if (store_command(then_path, cmd->if_cmd.then_cmd[i]) == -1) {
      LOG_ERR("error if_cmd : on 'then' index %u : store_command ()\n", i);
      return -1;
    }
  }

  for (uint32_t i = 0; i < else_count; i++) {
    char else_path[PATH_MAX];
    int written = snprintf(else_path, PATH_MAX, "%s/%u", base_else_path, i);
    if (written <= 0 || written >= PATH_MAX) {
      LOG_ERR("store_if_command: else path too long\n");
      return -1;
    }
    if (ensure_dir(else_path) == -1) {
      LOG_ERR("store_if_command: cannot create %s\n", else_path);
      return -1;
    }
    if (store_command(else_path, cmd->if_cmd.else_cmd[i]) == -1) {
      LOG_ERR("error if_cmd : on 'else' index %u : store_command ()\n", i);
      return -1;
    }
  }

  LOG_INFO("Stored if cmd successfully \n");

  return 0;
}

int store_type(char *path, uint16_t cmd_type) {
  char buffer[PATH_MAX];
  snprintf(buffer, PATH_MAX, "%s/%s", path, "type");

  int cmd_type_fd = open(buffer, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (cmd_type_fd == -1) {
    LOG_ERR("store_type() : open failed for %s\n", buffer);
    return -1;
  }

  const char *type_str = NULL;
  switch (cmd_type) {
  case CMD_TYPE_SI:
    type_str = "SI";
    break;
  case CMD_TYPE_SQ:
    type_str = "SQ";
    break;
  case CMD_TYPE_PL:
    type_str = "PL";
    break;
  case CMD_TYPE_IF:
    type_str = "IF";
    break;
  default:
    LOG_ERR("store_type() : unknown cmd_type %u\n", cmd_type);
    close(cmd_type_fd);
    return -1;
  }

  if (write(cmd_type_fd, type_str, 2) == -1) {
    LOG_ERR("store_type() : error writing cmd_type string\n");
    close(cmd_type_fd);
    return -1;
  }

  close(cmd_type_fd);
  return 0;
}

//-----------
int store_simple_command(char *path, command *cmd) {
  LOG_INFO("store_simple_command() \n");

  if (cmd == NULL) {
    LOG_ERR("null cmd \n");
    return -1;
  }
  if (cmd->type != CMD_TYPE_SI) {
    LOG_ERR("wrong cmd type : %i \n ", cmd->type);
    return -1;
  }
  int ret = 0;

  char argv_path[PATH_MAX];
  snprintf(argv_path, PATH_MAX, "%s/%s", path, "argv");

  if (store_type(path, CMD_TYPE_SI) == -1) {
    LOG_ERR("store_simple_command: store_type failed\n");
    return -1;
  }

  int argv_fd = -1;

  LOG_INFO("argv path : %s \n", argv_path);

  argv_fd = open(argv_path, O_CREAT | O_WRONLY | O_TRUNC, 0700);
  if (argv_fd == -1) {
    LOG_ERR("error : open() \n");
    return -1;
  }
  LOG_INFO("argv_fd open \n");

  uint32_t argc_be = htobe32(cmd->simple.argc);
  LOG_INFO("writing argc \n");
  if (write(argv_fd, &argc_be, sizeof(uint32_t)) == -1) {
    LOG_ERR("error writing argc \n");
    ret = -1;
    goto cleanup_return;
  }

  for (uint32_t i = 0; i < cmd->simple.argc; i++) {
    char *argv_i = cmd->simple.argv[i];
    uint32_t argv_i_length = strlen(argv_i);
    uint32_t raw_argv_i_length = htobe32(argv_i_length);

    LOG_INFO("writing argv length \n");
    if (write(argv_fd, &raw_argv_i_length, sizeof(uint32_t)) == -1) {
      LOG_ERR("error writing argv_i length \n");
      ret = -1;
      goto cleanup_return;
    }
    LOG_INFO("writing argv content \n");

    if (write(argv_fd, argv_i, argv_i_length) == -1) {
      LOG_ERR("error writing argv_i : %s \n", argv_i);
      ret = -1;
      goto cleanup_return;
    }
  }

cleanup_return:
  if (argv_fd != -1) {
    close(argv_fd);
  }

  return ret;
}

void print_options() {
  dprintf(2, "Actions disponibles: \n");
  dprintf(2, "  -F : exécution en avant-plan sans démonisation\n");
  dprintf(2, "  -R RUN_DIR : définition du répertoire de stockage (par défaut /tmp/$USER/erraid)\n");
  dprintf(2, "  -P PIPES_DIR : définition du répertoire contenant les tubes de communication avec le client (par "
             "défaut le sous-répertoire pipes de RUN_DIR)\n");
  dprintf(2, "  -L : no log : le log du daemon se fera sur STDERR_FILENO, l'executeur des tâches log toujours sur "
             "scheduler.log\n");
  dprintf(2, "  -S : désactive le logging de l’exécuteur de tâches. Les messages relatifs à l’exécution des tâches ne "
             "seront pas écrits dans scheduler.log(ni sur STDERR_FILENO).\n");
}

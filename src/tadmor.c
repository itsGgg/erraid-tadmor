#include "../include/tadmor.h"
#include "../include/logger.h"
#include "../include/protocol.h"
#include "../include/serialisation.h"
#include "../include/types.h"
#include <linux/limits.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_all_options();
    return 1;
  }

  int ret = 0;
  const char *pipes_dir = NULL;
  uint16_t task_id = -1;
  int opt = 0;
  char main_action = 0;
  task *new_task = NULL;
  int no_schedule = 0;
  int minutes_set = 0, hours_set = 0, days_set = 0;
  int action_count = 0;

  opterr = 0;

  while ((opt = getopt(argc, argv, "L+P:cspinr:lx:o:e:qm:H:d:")) != -1) {
    switch (opt) {
    case 'P':
      pipes_dir = optarg;
      break;
    case 'L':
      g_log_to_file = 0;
      break;
    case 'c':
    case 's':
    case 'p':
    case 'i':
      main_action = opt;
      action_count++;
      if (!new_task && alloc_task(&new_task) < 0)
        return -1;
      break;

    case 'm':
      if (no_schedule)
        break;
      if (no_schedule)
        break;
      if (!new_task && alloc_task(&new_task) < 0)
        return -1;
      if (!minutes_set) {
        new_task->timing.minutes = 0;
        minutes_set = 1;
      }
      // Parse multiple minute values
      for (int i = optind - 1; i < argc && argv[i][0] != '-' && isdigit(argv[i][0]); i++) {
        int val = atoi(argv[i]);
        if (val >= 0 && val < 60) {
          new_task->timing.minutes |= (1ULL << val);
        }
        optind = i + 1;
      }
      break;
    case 'H':
      if (no_schedule)
        break;
      if (no_schedule)
        break;
      if (!new_task && alloc_task(&new_task) < 0)
        return -1;
      if (!hours_set) {
        new_task->timing.hours = 0;
        hours_set = 1;
      }
      // Parse multiple hour values
      for (int i = optind - 1; i < argc && argv[i][0] != '-' && isdigit(argv[i][0]); i++) {
        int val = atoi(argv[i]);
        if (val >= 0 && val < 24) {
          new_task->timing.hours |= (1U << val);
        }
        optind = i + 1;
      }
      break;
    case 'd':
      if (no_schedule)
        break;
      if (no_schedule)
        break;
      if (!new_task && alloc_task(&new_task) < 0)
        return -1;
      if (!days_set) {
        new_task->timing.days = 0;
        days_set = 1;
      }
      // Parse multiple day values
      for (int i = optind - 1; i < argc && argv[i][0] != '-' && isdigit(argv[i][0]); i++) {
        int val = atoi(argv[i]);
        if (val >= 0 && val < 7) {
          new_task->timing.days |= (1U << val);
        }
        optind = i + 1;
      }
      break;
    case 'n':
      no_schedule = 1;
      if (!new_task && alloc_task(&new_task) < 0)
        return -1;
      new_task->timing.minutes = 0;
      new_task->timing.hours = 0;
      new_task->timing.days = 0;
      minutes_set = hours_set = days_set = 1;
      break;

    // autres actions immédiates
    case 'r':
      action_count++;
      main_action = opt;
      task_id = atoi(optarg);
      break;
    case 'l':
      action_count++;
      main_action = opt;
      break;
    case 'x':
      action_count++;
      main_action = opt;
      task_id = atoi(optarg);
      break;
    case 'o':
      action_count++;
      main_action = opt;
      task_id = atoi(optarg);
      break;
    case 'e':
      action_count++;
      main_action = opt;
      task_id = atoi(optarg);
      break;
    case 'q':
      LOG_INFO("q \n");
      return request_terminate(pipes_dir);
    }
  }
  // Set defaults for unspecified timing fields (all bits = every minute/hour/day)
  if (new_task && !no_schedule) {
    if (!minutes_set) {
      new_task->timing.minutes = (1ULL << 60) - 1; // bits 0-59 set
    }
    if (!hours_set) {
      new_task->timing.hours = (1U << 24) - 1; // bits 0-23 set
    }
    if (!days_set) {
      new_task->timing.days = (1U << 7) - 1; // bits 0-6 set
    }
  }

  switch (main_action) {
  case 'c':
    ret = request_create_simple_task(pipes_dir, &new_task, argc, argv, optind);
    break;
  case 's':
    ret = request_combine_task(pipes_dir, &new_task, argc, argv, optind, CMD_TYPE_SQ);
    break;
  case 'p':
    ret = request_combine_task(pipes_dir, &new_task, argc, argv, optind, CMD_TYPE_PL);
    break;
  case 'i':
    ret = request_combine_task(pipes_dir, &new_task, argc, argv, optind, CMD_TYPE_IF);
    break;
  case 'r':
    ret = request_remove_task(pipes_dir, task_id);
    break;
  case 'l':
    ret = request_list_tasks(pipes_dir);
    break;
  case 'x':
    ret = request_task_times_exit(pipes_dir, task_id);
    break;
  case 'o':
    ret = request_task_stdout(pipes_dir, task_id);
    break;
  case 'e':
    ret = request_task_stderr(pipes_dir, task_id);
    break;
  case 'q':
    ret = request_terminate(pipes_dir);
    break;
  default:
    dprintf(2, "Action inconnue\n");
    ret = 1;
    goto cleanup_return;
  }
cleanup_return:
  if (new_task != NULL) {
    free(new_task);
  }
  return ret;
}

void print_actions() {
  dprintf(2, "Erreur: aucune action fournie.\n");
  dprintf(2, "Actions disponibles: \n");
  dprintf(2, "  -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] CMD [ARG_1] ... [ARG_N] : créer une tâche simple\n");
  dprintf(2, "  -s [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N : créer une tâche séquentielle\n");
  dprintf(2, "  -p [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N : créer une tâche parallèle\n");
  dprintf(2, "  -i [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 TASKID_2 [TASKID_3] : créer une tâche "
             "conditionnelle\n");
  dprintf(2, "  -l : liste des tâches avec identifiant \n");
  dprintf(2, "  -o TASKID : sortie standard de la dernière exécution complète de la tâche d'identifiant TASKID\n");
  dprintf(2, "  -e TASKID : erreur standard de la dernière exécution complète de la tâche d'identifiant TASKID\n");
  dprintf(2, "  -x TASKID : liste datée des valeurs de retour des exécutions de la tâche d'identifiant TASKID\n");
  dprintf(2, "  -r TASKID : supprimer la tâche d'identifiant TASKID \n");
  dprintf(2, "  -q : arrêt du daemon\n");
}

void print_all_options() {
  dprintf(2, "Actions disponibles: \n");
  dprintf(2, "  -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] CMD [ARG_1] ... [ARG_N] : créer une tâche simple\n");
  dprintf(2, "  -s [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N : créer une tâche séquentielle\n");
  dprintf(2, "  -p [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N : créer une tâche parallèle\n");
  dprintf(2, "  -i [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 TASKID_2 [TASKID_3] : créer une tâche "
             "conditionnelle\n");
  dprintf(2, "  -l : liste des tâches avec identifiant \n");
  dprintf(2, "  -o TASKID : sortie standard de la dernière exécution complète de la tâche d'identifiant TASKID\n");
  dprintf(2, "  -e TASKID : erreur standard de la dernière exécution complète de la tâche d'identifiant TASKID\n");
  dprintf(2, "  -x TASKID : liste datée des valeurs de retour des exécutions de la tâche d'identifiant TASKID\n");
  dprintf(2, "  -r TASKID : supprimer la tâche d'identifiant TASKID \n");
  dprintf(2, "  -q : arrêt du daemon\n");
  dprintf(2, "Autres options disponibles: \n");
  dprintf(2,
          "  -P PIPES_DIR : définition du répertoire contenant les tubes de communication\n\tavec le démon (par défaut "
          "/tmp/$USER/erraid/pipes)\n");
  dprintf(2, " En combinaison avec une des options : -c ; -s ; -p ; -i :\n");
  dprintf(2, "  -m MINUTES : planifier l'exécution aux minutes spécifiées\n");
  dprintf(2, "  -H HOURS : planifier l'exécution aux heures spécifiées\n");
  dprintf(2, "  -d DAYSOFWEEK : planifier l'exécution aux jours de la semaine spécifiés\n");
  dprintf(2, "  -n : définition d'une tâche sans horaire d'exécution\n");
  dprintf(2, "       (qui ne sera jamais exécutée , et destinée à être combinée avec d'autres tâches "
             "pour créer une commande complexe)\n");
}

int parse_sub_options(int argc, char *argv[], const char *opts, task **task) {
  if (task == NULL || argv == NULL || opts == NULL || argc < 3) {
    LOG_ERR("args givens : %i , %s , %s \n", argc, argv, opts);
    if (task == NULL) {
      LOG_ERR("task given is NULL \n");
    }
    return -1;
  }
  int opt = 0;
  while ((opt = getopt(argc, argv, opts)) != -1) {
    switch (opt) {
    case 'm':
      (*task)->timing.minutes = atoi(optarg);
      break;
    case 'H':
      (*task)->timing.hours = atoi(optarg);
      break;
    case 'd':
      (*task)->timing.days = atoi(optarg);
      break;
    case 'n': // TO-DO
      break;
    }
  }
  return 0;
}

void read_command(int fd_out, int depth) {
  uint16_t type_raw;
  read(fd_out, &type_raw, sizeof(type_raw));
  uint16_t type = be16toh(type_raw);

  if (type == CMD_TYPE_SI) {
    uint32_t argc_raw;
    read(fd_out, &argc_raw, sizeof(argc_raw));
    uint32_t argc = be32toh(argc_raw);

    for (uint32_t i = 0; i < argc; i++) {
      uint32_t len_raw;
      read(fd_out, &len_raw, sizeof(len_raw));
      uint32_t len = be32toh(len_raw);

      char *arg = malloc(len + 1);
      read(fd_out, arg, len);
      arg[len] = '\0';

      dprintf(1, "%s", arg);
      if (i < argc - 1)
        dprintf(1, " ");
      free(arg);
    }
  } else if (type == CMD_TYPE_SQ) {
    uint32_t count_raw;
    read(fd_out, &count_raw, sizeof(count_raw));
    uint32_t count = be32toh(count_raw);

    if (depth > 0)
      dprintf(1, "( ");
    for (uint32_t i = 0; i < count; i++) {
      read_command(fd_out, depth + 1);
      if (i < count - 1)
        dprintf(1, " ; ");
    }
    if (depth > 0)
      dprintf(1, " )");
  } else if (type == CMD_TYPE_PL) {
    uint32_t count_raw;
    read(fd_out, &count_raw, sizeof(count_raw));
    uint32_t count = be32toh(count_raw);

    if (depth > 0)
      dprintf(1, "( ");
    for (uint32_t i = 0; i < count; i++) {
      read_command(fd_out, depth + 1);
      if (i < count - 1)
        dprintf(1, " | ");
    }
    if (depth > 0)
      dprintf(1, " )");
  } else if (type == CMD_TYPE_IF) {
    uint32_t cond_count_raw, then_count_raw, else_count_raw;

    // Read cond_count, then immediately read cond commands
    read(fd_out, &cond_count_raw, sizeof(cond_count_raw));
    uint32_t cond_count = be32toh(cond_count_raw);

    dprintf(1, "( if ");

    for (uint32_t i = 0; i < cond_count; i++) {
      read_command(fd_out, depth + 1);
      if (i < cond_count - 1)
        dprintf(1, " ; ");
    }

    // Now read then_count and then commands
    read(fd_out, &then_count_raw, sizeof(then_count_raw));
    uint32_t then_count = be32toh(then_count_raw);

    dprintf(1, " ; then ");

    for (uint32_t i = 0; i < then_count; i++) {
      read_command(fd_out, depth + 1);
      if (i < then_count - 1)
        dprintf(1, " ; ");
    }

    // Finally read else_count and else commands
    read(fd_out, &else_count_raw, sizeof(else_count_raw));
    uint32_t else_count = be32toh(else_count_raw);

    if (else_count > 0) {
      dprintf(1, " ; else ");
      for (uint32_t i = 0; i < else_count; i++) {
        read_command(fd_out, depth + 1);
        if (i < else_count - 1)
          dprintf(1, " ; ");
      }
    }

    dprintf(1, " ; fi )");
  } else {
    dprintf(2, "Erreur: type de commande inconnu (0x%04x)\n", type);
    return;
  }
}

void format_timing_field(char *buf, uint64_t bits, int max_bits) {
  int all_set = 1;
  int none_set = 1;

  for (int i = 0; i < max_bits; i++) {
    if (bits & (1ULL << i)) {
      none_set = 0;
    } else {
      all_set = 0;
    }
  }

  if (all_set) {
    strcpy(buf, "*");
    return;
  }
  if (none_set) {
    strcpy(buf, "-");
    return;
  }

  buf[0] = '\0';
  int in_range = 0;
  int range_start = -1;

  for (int i = 0; i <= max_bits; i++) {
    int bit_set = (i < max_bits) && (bits & (1ULL << i));

    if (bit_set && !in_range) {
      range_start = i;
      in_range = 1;
    } else if (!bit_set && in_range) {
      if (buf[0] != '\0')
        strcat(buf, ",");

      char tmp[32];
      if (i - 1 == range_start) {
        snprintf(tmp, sizeof(tmp), "%d", range_start);
      } else if (i - 1 == range_start + 1) {
        snprintf(tmp, sizeof(tmp), "%d,%d", range_start, i - 1);
      } else {
        snprintf(tmp, sizeof(tmp), "%d-%d", range_start, i - 1);
      }
      strcat(buf, tmp);
      in_range = 0;
    }
  }
}

void pipe_paths(char *req_pipe, char *rep_pipe, const char *pipes_dir) {
  if (pipes_dir != NULL) {
    // Verifie si le répertoire se fini avec /pipes, sinon on le rajoute
    size_t len = strlen(pipes_dir);
    if (len >= 6 && strcmp(pipes_dir + len - 6, "/pipes") == 0) { // pas besoin de rajouter
      snprintf(req_pipe, PATH_MAX, "%s/erraid-request-pipe", pipes_dir);
      snprintf(rep_pipe, PATH_MAX, "%s/erraid-reply-pipe", pipes_dir);
    } else { // Rajouter pipes
      snprintf(req_pipe, PATH_MAX, "%s/pipes/erraid-request-pipe", pipes_dir);
      snprintf(rep_pipe, PATH_MAX, "%s/pipes/erraid-reply-pipe", pipes_dir);
    }
  } else {
    const char *user = get_user();
    snprintf(req_pipe, PATH_MAX, "/tmp/%s/erraid/pipes/erraid-request-pipe", user);
    snprintf(rep_pipe, PATH_MAX, "/tmp/%s/erraid/pipes/erraid-reply-pipe", user);
    LOG_INFO("pipes_dir was NULL, using default paths: %s , %s \n", req_pipe, rep_pipe);
  }
}

int request_list_tasks(const char *pipes_dir) {

  uint16_t opcode = htobe16(OPCODE_LIST);
  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  int fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    return 1;
  }

  write(fd_in, &opcode, sizeof(opcode));

  int fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    return 1;
  }

  uint16_t anstype_raw;
  read(fd_out, &anstype_raw, sizeof(anstype_raw));
  uint16_t anstype = be16toh(anstype_raw);

  uint32_t count_raw;
  read(fd_out, &count_raw, sizeof(count_raw));
  uint32_t count = be32toh(count_raw);

  for (uint32_t i = 0; i < count; i++) {
    uint64_t id_raw;
    read(fd_out, &id_raw, sizeof(id_raw));
    uint64_t id = be64toh(id_raw);

    uint64_t minute_raw;
    uint32_t hour_raw;
    uint8_t day_raw;

    read(fd_out, &minute_raw, sizeof(uint64_t));
    read(fd_out, &hour_raw, sizeof(uint32_t));
    read(fd_out, &day_raw, sizeof(uint8_t));

    uint64_t minute = be64toh(minute_raw);
    uint32_t hour = be32toh(hour_raw);
    uint8_t day = day_raw;

    char minute_str[64], hour_str[64], day_str[64];
    format_timing_field(minute_str, minute, 60);
    format_timing_field(hour_str, hour, 24);
    format_timing_field(day_str, day, 7);

    dprintf(1, "%lu: %s %s %s ", id, minute_str, hour_str, day_str);

    read_command(fd_out, 0);
    dprintf(1, "\n");
  }
  close(fd_in);
  close(fd_out);
  return 0;
}

int request_task_stdout(const char *pipes_dir, uint64_t task_id) {

  // uint16_t opcode = htobe16(0x534f); // #define OPCODE_STDOUT 0x534f     // 'SO'
  uint16_t opcode = htobe16(OPCODE_STDOUT); // #define OPCODE_STDOUT 0x534f     // 'SO'

  // uint64_t id = htobe64(atoi(argv[2]));
  uint64_t id = htobe64(task_id);

  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  int fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    return 1;
  }

  write(fd_in, &opcode, sizeof(opcode));
  write(fd_in, &id, sizeof(id));

  int fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    return 1;
  }

  uint16_t anstype_raw;
  read(fd_out, &anstype_raw, sizeof(anstype_raw));
  uint16_t anstype = be16toh(anstype_raw);

  // if (anstype == 0x4f4b) { // #define ANSTYPE_OK 0x4f4b    // 'OK'
  if (anstype == ANSTYPE_OK) {
    uint32_t len_raw;
    read(fd_out, &len_raw, sizeof(len_raw));
    uint32_t len = be32toh(len_raw);

    char *s = malloc(len);
    if (!s) {
      close(fd_in);
      close(fd_out);
      return 1;
    }
    uint32_t total_read = 0;
    while (total_read < len) {
      ssize_t n = read(fd_out, s + total_read, len - total_read);
      if (n <= 0)
        break;
      total_read += n;
    }
    write(STDOUT_FILENO, s, total_read);
    free(s);
    close(fd_in);
    close(fd_out);
    return 0;
  } else {
    uint16_t errcode_raw;
    read(fd_out, &errcode_raw, sizeof(errcode_raw));
    uint16_t errcode = be16toh(errcode_raw);

    // if (errcode == 0x4e52) { // #define ERRCODE_NOTRUN 0x4e52 // 'NR'
    if (errcode == ERRCODE_NOTRUN) {
      dprintf(2, "La tâche n'a pas encore été exécutée au moins une fois\n");
    } else {
      dprintf(2, "identifiant de tâche non trouvé\n");
    }
    close(fd_in);
    close(fd_out);
    return 1;
  }
  return 0;
}

int request_task_stderr(const char *pipes_dir, uint64_t task_id) {
  uint16_t opcode = htobe16(OPCODE_STDERR);
  uint64_t id = htobe64(task_id);

  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  int fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    return 1;
  }

  write(fd_in, &opcode, sizeof(opcode));
  write(fd_in, &id, sizeof(id));

  int fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    return 1;
  }

  uint16_t anstype_raw;
  read(fd_out, &anstype_raw, sizeof(anstype_raw));
  uint16_t anstype = be16toh(anstype_raw);

  if (anstype == ANSTYPE_OK) { // #define ANSTYPE_OK 0x4f4b    // 'OK'

    uint32_t len_raw;
    read(fd_out, &len_raw, sizeof(len_raw));
    uint32_t len = be32toh(len_raw);

    char *s = malloc(len);
    if (!s) {
      close(fd_in);
      close(fd_out);
      return 1;
    }
    uint32_t total_read = 0;
    while (total_read < len) {
      ssize_t n = read(fd_out, s + total_read, len - total_read);
      if (n <= 0)
        break;
      total_read += n;
    }
    write(STDOUT_FILENO, s, total_read);
    free(s);
    close(fd_in);
    close(fd_out);
    return 0;
  } else {
    uint16_t errcode_raw;
    read(fd_out, &errcode_raw, sizeof(errcode_raw));
    uint16_t errcode = be16toh(errcode_raw);

    // if (errcode == 0x4e52) { #define ERRCODE_NOTRUN 0x4e52 // 'NR'
    if (errcode == ERRCODE_NOTRUN) {
      dprintf(2, "La tâche n'a pas encore été exécutée au moins une fois\n");
    } else {
      dprintf(2, "identifiant de tâche non trouvé\n");
    }
    close(fd_in);
    close(fd_out);
    return 1;
  }
  return 0;
}

int request_terminate(const char *pipes_dir) {
  uint16_t opcode = htobe16(OPCODE_TERMINATE); // #define OPCODE_STDOUT 0x534f     // 'SO'
  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);
  int fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  LOG_INFO("Ouverture du tube de requete... %d\n", fd_in);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    return 1;
  }

  write(fd_in, &opcode, sizeof(opcode));

  int fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    return 1;
  }

  uint16_t anstype_raw;
  read(fd_out, &anstype_raw, sizeof(anstype_raw));
  uint16_t anstype = be16toh(anstype_raw);

  if (anstype == ANSTYPE_OK) {
    close(fd_in);
    close(fd_out);
    return 0;
  } else {
    uint16_t errcode_raw;
    read(fd_out, &errcode_raw, sizeof(errcode_raw));
    uint16_t errcode = be16toh(errcode_raw);

    dprintf(2, "quelque chose s'est mal passe lors de l'arret du daemon\n");
    close(fd_in);
    close(fd_out);
    return 1;
  }
}

int request_task_times_exit(const char *pipes_dir, uint64_t task_id) {

  uint16_t opcode = htobe16(OPCODE_TIMES_EXIT);
  uint64_t id = htobe64(task_id);

  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  int fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    return 1;
  }

  write(fd_in, &opcode, sizeof(opcode));
  write(fd_in, &id, sizeof(id));

  int fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    return 1;
  }

  uint16_t anstype_raw;
  read(fd_out, &anstype_raw, sizeof(anstype_raw));
  uint16_t anstype = be16toh(anstype_raw);

  // if (anstype == 0x4f4b) { // #define ANSTYPE_OK 0x4f4b    // 'OK'
  if (anstype == ANSTYPE_OK) { // #define ANSTYPE_OK 0x4f4b    // 'OK'

    uint32_t nbruns_raw;
    read(fd_out, &nbruns_raw, sizeof(nbruns_raw));
    uint32_t nbruns = be32toh(nbruns_raw);

    for (uint32_t i = 0; i < nbruns; i++) {
      uint64_t runtime_raw;
      read(fd_out, &runtime_raw, sizeof(runtime_raw));
      uint64_t runtime = be64toh(runtime_raw);

      uint16_t exitcode_raw;
      read(fd_out, &exitcode_raw, sizeof(exitcode_raw));
      uint16_t exitcode = be16toh(exitcode_raw);

      time_t timestamp = (time_t)runtime;
      struct tm tm_buf;
      struct tm *tm_info = localtime_r(&timestamp, &tm_buf);

      if (tm_info != NULL) {
        dprintf(1, "%04d-%02d-%02d %02d:%02d:%02d %hu\n", tm_info->tm_year + 1900, tm_info->tm_mon + 1,
                tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, exitcode);
      } else {
        dprintf(1, "%lu %hu\n", runtime, exitcode);
      }
    }

  } else {
    uint16_t errcode_raw;
    read(fd_out, &errcode_raw, sizeof(errcode_raw));
    uint16_t errcode = be16toh(errcode_raw);
    // if (errcode == 0x4e46) { // #define ERRCODE_ENOEF 0x4e46  // 'NF'
    if (errcode == ERRCODE_ENOEF) {
      dprintf(2, "identifiant de tâche non trouvé\n");
    }
    close(fd_in);
    close(fd_out);
    return 1;
  }

  close(fd_in);
  close(fd_out);
  return 0;
}

int request_remove_task(const char *pipes_dir, uint64_t task_id) {
  uint16_t opcode = htobe16(OPCODE_REMOVE);
  uint64_t id = htobe64(task_id);

  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  int fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    return 1;
  }

  write(fd_in, &opcode, sizeof(opcode));
  write(fd_in, &id, sizeof(id));

  int fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    close(fd_in);
    return 1;
  }

  uint16_t anstype_raw;
  if (read(fd_out, &anstype_raw, sizeof(anstype_raw)) != sizeof(anstype_raw)) {
    dprintf(2, "Erreur: reponse invalide du daemon.\n");
    close(fd_in);
    close(fd_out);
    return 1;
  }
  uint16_t anstype = be16toh(anstype_raw);

  if (anstype == ANSTYPE_OK) {
    dprintf(1, "Tache %lu supprimee\n", task_id);
    close(fd_in);
    close(fd_out);
    return 0;
  }

  uint16_t errcode_raw;
  if (read(fd_out, &errcode_raw, sizeof(errcode_raw)) == sizeof(errcode_raw)) {
    uint16_t errcode = be16toh(errcode_raw);
    if (errcode == ERRCODE_ENOEF) {
      dprintf(2, "identifiant de tache non trouve\n");
    } else {
      dprintf(2, "Echec de la suppression de la tache\n");
    }
  } else {
    dprintf(2, "Erreur: reponse invalide du daemon.\n");
  }

  close(fd_in);
  close(fd_out);
  return 1;
}

int request_create_simple_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind) {
  if (argc == 0 || argc < 1 || argv == NULL) {
    return -1;
  }

  int ret = 0;

  int fd_in = -1;
  int fd_out = -1;

  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  uint16_t opcode = htobe16(OPCODE_CREATE);

  fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    LOG_ERR("Error open() \n");
    return -1;
  }
  // opcode ok
  if (write(fd_in, &opcode, sizeof(uint16_t)) == -1) {
    ret = -1;
    goto cleanup_return;
  }

  // Writing TIMING
  uint64_t raw_mins = htobe64((*new_task)->timing.minutes);
  uint32_t raw_hours = htobe32((*new_task)->timing.hours);

  int nb = write(fd_in, &raw_mins, sizeof(uint64_t));
  if (nb == -1) {
    LOG_ERR("error sending mins via pipe \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO(" wrote serialised minutes : size %i \n", nb);

  if (write(fd_in, &raw_hours, sizeof(uint32_t)) == -1) {
    LOG_ERR("error sending hours via pipe \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO("wrote serialised hours \n");

  if (write(fd_in, &(*new_task)->timing.days, sizeof(uint8_t)) == -1) {
    LOG_ERR("error sending opcode via pipe \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO(" wrote days\n");

  // done with TIMING

  // Write argc

  uint32_t count = argc - optind;
  uint32_t raw_count = htobe32(count);
  if (write(fd_in, &raw_count, sizeof(uint32_t)) == -1) {
    LOG_ERR("Error writing count \n");
    ret = -1;
    goto cleanup_return;
  }

  char *cmd = argv[count];

  for (uint32_t i = 0; i < count; i++) {
    char *s_i = argv[optind + i];
    uint32_t length = strlen(s_i);
    uint32_t raw_length = htobe32(length);

    // write length first then arg
    if (write(fd_in, &raw_length, sizeof(uint32_t)) == -1) {
      LOG_ERR("Error writing length %s : (index) %u \n", s_i, (unsigned)i);
      ret = -1;
      goto cleanup_return;
    }
    if (write(fd_in, s_i, length) == -1) {
      LOG_ERR("Error writing arg %s \n", s_i);
      ret = -1;
      goto cleanup_return;
    }
  }
  fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    close(fd_in);
    return -1;
  }
  LOG_INFO("ouverture tube de reponse correte \n");

  uint16_t anstype_raw;
  if (read(fd_out, &anstype_raw, sizeof(anstype_raw)) != sizeof(anstype_raw)) {
    LOG_ERR("error read ! read : %u \n", (unsigned)anstype_raw);
    dprintf(2, "Erreur: reponse invalide du daemon.\n");
    ret = 1;
    goto cleanup_return;
  }
  LOG_INFO(" lecture du code de retour de daemon correct\n");
  uint16_t anstype = be16toh(anstype_raw);

  if (anstype == ANSTYPE_OK) {
    dprintf(1, "Tâche crée avec succes \n");
    ret = 0;
    close(fd_in);
    close(fd_out);
    return ret;
  }

cleanup_return:
  uint16_t errcode_raw;
  if (read(fd_out, &errcode_raw, sizeof(errcode_raw)) == sizeof(errcode_raw)) {
    uint16_t errcode = be16toh(errcode_raw);
    if (errcode == ERRCODE_ENOEF) {
      LOG_INFO(" enoef \n");
      dprintf(2, "tâche n'a pas pu être crée\n");
    } else {
      LOG_INFO(" autre code\n");
      dprintf(2, "Echec de la suppression de la tache\n");
    }
    ret = 1;
  } else {
    LOG_INFO(" reponse invalide \n");
    dprintf(2, "Erreur: reponse invalide du daemon.\n");
    ret = -1;
  }
  if (fd_in == -1) {
    close(fd_in);
  }
  if (fd_out == -1) {
    close(fd_out);
  }

  return ret;
} // optind = index of the next element to be processed in argv

// to-do : take care of -n option after
int request_combine_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind, uint16_t type) {
  LOG_INFO("request_combine_task \n");
  int ret = 0;

  int fd_in = -1;
  int fd_out = -1;

  char req_pipe[PATH_MAX];
  char rep_pipe[PATH_MAX];
  pipe_paths(req_pipe, rep_pipe, pipes_dir);

  fd_in = open(req_pipe, O_WRONLY | O_NONBLOCK);
  if (fd_in == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de requete.\n");
    dprintf(2, "Verifiez que le daemon erraid est demarre.\n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO("opened req_pipe \n");

  // OPCODE='CB' <uint16>, TIMING <timing>, TYPE <uint16>, NBTASKS=N <uint32>,
  // TASKID[0]<unit64>, ...,
  // TASKID[N - 1]<unit64>

  // Writing CODE
  uint16_t raw_code = htobe16(OPCODE_COMBINE);
  if (write(fd_in, &raw_code, sizeof(uint16_t)) == -1) {
    LOG_ERR("error sending opcode");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO("wrote serialised code \n");

  // Writing TIMING
  uint64_t raw_mins = htobe64((*new_task)->timing.minutes);
  uint32_t raw_hours = htobe32((*new_task)->timing.hours);

  // if (write(fd_in, &raw_mins, sizeof(uint64_t)) == -1) {
  //   LOG_ERR("error sending mins via pipe \n");
  //   ret = -1;
  //   goto cleanup_return;
  // }

  int nb = write(fd_in, &raw_mins, sizeof(uint64_t));
  if (nb == -1) {
    LOG_ERR("error sending mins via pipe \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO(" wrote serialised minutes : size %i \n", nb);

  if (write(fd_in, &raw_hours, sizeof(uint32_t)) == -1) {
    LOG_ERR("error sending hours via pipe \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO("wrote serialised hours \n");

  if (write(fd_in, &(*new_task)->timing.days, sizeof(uint8_t)) == -1) {
    LOG_ERR("error sending opcode via pipe \n");
    ret = -1;
    goto cleanup_return;
  }
  LOG_INFO(" wrote days\n");

  // writing task type
  uint16_t raw_type = htobe16(type);
  if (write(fd_in, &raw_type, sizeof(raw_type)) == -1) {
    LOG_ERR("error task type via pipe \n");
    ret = -1;
    goto cleanup_return;
  }

  LOG_INFO(" wrote task type\n");

  // Writing rest of ARGV after option treatement (task_id1 task_id2 .. task_idn)

  if (type == CMD_TYPE_IF) {
    // Count remaining args
    int remaining = argc - optind;
    if (remaining < 2 || remaining > 3) {
      LOG_ERR("IF command requires 2 or 3 task IDs (cond, then, [else]), got %d\n", remaining);
      dprintf(2, "Erreur: La commande IF requiert 2 ou 3 identifiants de taches (cond, then, [else])\n");
      ret = -1;
      goto cleanup_return;
    }

    // Send count
    uint32_t nb_tasks_be = htobe32(remaining);
    if (write(fd_in, &nb_tasks_be, sizeof(uint32_t)) == -1) {
      LOG_ERR("error writing task count for IF\n");
      ret = -1;
      goto cleanup_return;
    }

    // Send each ID in order: cond, then, [else]
    for (int i = 0; i < remaining; i++) {
      uint16_t task_id = atoi(argv[optind + i]);
      uint16_t task_id_raw = htobe16(task_id);
      if (write(fd_in, &task_id_raw, sizeof(task_id_raw)) == -1) {
        LOG_ERR("error writing task ID %d for IF\n", task_id);
        ret = -1;
        goto cleanup_return;
      }
      LOG_INFO("Wrote IF task ID: %s\n", argv[optind + i]);
    }
  } else {
    if (serialise_task_ids(fd_in, argc, argv, optind) == -1) {
      LOG_ERR("error lors de la serialisation \n");
      ret = -1;
      goto cleanup_return;
    }
  }
  LOG_INFO(" serialised task ids\n");

  fd_out = open(rep_pipe, O_RDONLY);
  if (fd_out == -1) {
    dprintf(2, "Erreur: impossible d'ouvrir le tube de reponse.\n");
    close(fd_in);
    return -1;
  }
  LOG_INFO("ouverture tube de reponse correte \n");

  uint16_t anstype_raw;
  if (read(fd_out, &anstype_raw, sizeof(anstype_raw)) != sizeof(anstype_raw)) {
    LOG_ERR("error read ! read : %u \n", (unsigned)anstype_raw);
    dprintf(2, "Erreur: reponse invalide du daemon.\n");
    ret = 1;
    goto cleanup_return;
  }
  LOG_INFO(" lecture du code de retour de daemon correct\n");
  uint16_t anstype = be16toh(anstype_raw);

  if (anstype == ANSTYPE_OK) {
    LOG_INFO(" tache crée avec succes\n");
    dprintf(1, "Tâche crée avec succés. \n");

    // Delete the individual tasks that were combined
    if (type == CMD_TYPE_IF) {
      // For IF commands, delete cond, then, and optionally else tasks
      int remaining = argc - optind;
      for (int i = 0; i < remaining; i++) {
        uint16_t task_id = atoi(argv[optind + i]);
        LOG_INFO("Deleting combined task ID: %u\n", task_id);
        request_remove_task(pipes_dir, task_id);
      }
    } else if (type == CMD_TYPE_PL) {
      // For Pipeline commands, delete all combined tasks
      for (int i = optind; i < argc; i++) {
        if (!is_shell_symbol(argv[i])) {
          uint16_t task_id = atoi(argv[i]);
          LOG_INFO("Deleting combined task ID: %u\n", task_id);
          request_remove_task(pipes_dir, task_id);
        }
      }
    }

    goto cleanup_return;
  }

  uint16_t errcode_raw;
  if (read(fd_out, &errcode_raw, sizeof(errcode_raw)) == sizeof(errcode_raw)) {
    uint16_t errcode = be16toh(errcode_raw);
    if (errcode == ERRCODE_ENOEF) {
      LOG_INFO(" enoef \n");
      dprintf(2, "identifiant de tache non trouve\n");
    } else {
      LOG_INFO(" autre code\n");
      dprintf(2, "Echec de la suppression de la tache\n");
    }
    ret = 1;
  } else {
    LOG_INFO(" reponse invalide \n");
    dprintf(2, "Erreur: reponse invalide du daemon.\n");
    ret = -1;
  }

cleanup_return:
  if (fd_in != -1) {
    close(fd_in);
  }
  if (fd_out != -1) {
    close(fd_out);
  }
  return ret;
}

// int request_create_pipeline_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind);
// int request_create_conditional_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind);

int is_shell_symbol(const char *arg) {
  return (strcmp(arg, ";") == 0 || strcmp(arg, "|") == 0 || strcmp(arg, "if") == 0 || strcmp(arg, "then") == 0 ||
          strcmp(arg, "else") == 0 || strcmp(arg, "fi") == 0);
}

int serialise_task_ids(int req_pipe, int argc, char *argv[], int optind) {
  if (req_pipe == -1 || argv == NULL) {
    LOG_ERR("null args");
    return -1;
  }
  LOG_INFO(" serialise_task_ids() \n");

  // Compter les IDs (en ignorant les symboles shell)
  int nb_tasks = 0;
  for (int i = optind; i < argc; i++) {
    if (!is_shell_symbol(argv[i])) {
      nb_tasks++;
    }
  }
  LOG_INFO(" nb_tasks : %i\n", nb_tasks);

  uint32_t nb_tasks_be = htobe32(nb_tasks);
  write(req_pipe, &nb_tasks_be, sizeof(uint32_t));
  LOG_INFO(" wrote nb of tasks %u\n", (unsigned)(be32toh(nb_tasks_be)));

  // Envoyer uniquement les IDs numériques
  for (int i = optind; i < argc; i++) {
    if (is_shell_symbol(argv[i])) {
      continue;
    }
    uint16_t task_id = atoi(argv[i]);
    LOG_INFO("Serialising TASK_ID: %s \n", argv[i]);
    uint16_t task_id_raw = htobe16(task_id);
    write(req_pipe, &task_id_raw, sizeof(task_id_raw));
    LOG_INFO("Wrote TASK_ID %s into rep pipe \n", argv[i]);
  }

  return 0;
}

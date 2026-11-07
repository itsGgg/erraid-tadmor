#include "../include/logger.h"
#include "../include/scheduler.h"
#include "../include/types.h"
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

uint64_t change_indian_format(uint64_t num) { return htobe64(num); }

uint16_t swap_uint16(uint16_t val) { return htobe16(val); }

uint32_t swap_uint32(uint32_t val) { return htobe32(val); }

int get_bit_value(uint64_t number, int bit) {
  if (bit < 0 || bit >= 64) {
    return -1;
  }
  return (number & (1ULL << bit)) ? 1 : 0;
}

int should_execute(task *task_entry, int current_day, int current_hour, int current_minute) {
  if (get_bit_value(task_entry->timing.days, current_day - 1) &&
      get_bit_value(task_entry->timing.hours, current_hour) &&
      get_bit_value(task_entry->timing.minutes, current_minute)) {
    return 1;
  }
  return 0;
}

int deserialize_command_internal(char *cmd_path, task *task_entry) {
  struct dirent *cmd_entry;
  char *tmp_path = malloc(strlen(cmd_path) + 256 * sizeof(char));

  sprintf(tmp_path, "%s/type", cmd_path);
  int cmd_type = open(tmp_path, O_RDONLY);
  if (cmd_type < 0) {
    perror("ficher type introuvable ou inaccessible (verifiez les "
           "existences et droits)");
    free(tmp_path);
    return 0;
  }

  char command_type_str[3];
  int bytes_read = read(cmd_type, command_type_str, 2);
  if (bytes_read != 2) {
    perror("le ficher type est corrompu (impossible de lire le type de "
           "commande)");
    close(cmd_type);
    free(tmp_path);
    return 0;
  }
  command_type_str[2] = '\0';

  task_entry->cmd = malloc(sizeof(command));

  if (strcmp(command_type_str, "SI") == 0) {
    task_entry->cmd->type = CMD_TYPE_SI;

    sprintf(tmp_path, "%s/argv", cmd_path);
    int argv_file = open(tmp_path, O_RDONLY);
    if (argv_file < 0) {
      perror("ficher argv introuvable ou inaccessible (verifiez les "
             "existences et droits)");
      close(cmd_type);
      free(task_entry->cmd);
      free(tmp_path);
      return 0;
    }

    char buffer[256];
    uint32_t argc;
    bytes_read = read(argv_file, &argc, sizeof(uint32_t));
    if (bytes_read != sizeof(uint32_t)) {
      perror("filer argv corrompu (impossible de lire argc)");
      close(argv_file);
      close(cmd_type);
      free(task_entry->cmd);
      free(tmp_path);
      return 0;
    }

    argc = be32toh(argc);
    task_entry->cmd->simple.argc = argc;

    task_entry->cmd->simple.argv = malloc(sizeof(char *) * (argc + 1));

    for (uint32_t i = 0; i < argc; i++) {
      uint32_t arg_length;
      bytes_read = read(argv_file, &arg_length, sizeof(uint32_t));
      if (bytes_read != sizeof(uint32_t)) {
        perror("Failed to read argument length");
        for (uint32_t j = 0; j < i; j++) {
          free(task_entry->cmd->simple.argv[j]);
        }
        free(task_entry->cmd->simple.argv);
        close(argv_file);
        close(cmd_type);
        free(task_entry->cmd);
        free(tmp_path);
        return 0;
      }

      arg_length = be32toh(arg_length);

      bytes_read = read(argv_file, buffer, arg_length);
      if (bytes_read != (int)arg_length) {
        perror("Failed to read argument data");
        for (uint32_t j = 0; j < i; j++) {
          free(task_entry->cmd->simple.argv[j]);
        }
        free(task_entry->cmd->simple.argv);
        close(argv_file);
        close(cmd_type);
        free(task_entry->cmd);
        free(tmp_path);
        return 0;
      }
      buffer[arg_length] = '\0';

      task_entry->cmd->simple.argv[i] = malloc(arg_length + 1);
      strcpy(task_entry->cmd->simple.argv[i], buffer);
    }

    task_entry->cmd->simple.argv[argc] = NULL;
    close(argv_file);
  } else if (strcmp(command_type_str, "SQ") == 0 || strcmp(command_type_str, "PL") == 0) {
    if (strcmp(command_type_str, "SQ") == 0) {
      task_entry->cmd->type = CMD_TYPE_SQ;
    } else {
      task_entry->cmd->type = CMD_TYPE_PL;
    }

    DIR *cmd_dir = opendir(cmd_path);
    if (cmd_dir == NULL) {
      perror("ficher cmd introuvable ou inaccessible (verifiez les "
             "existences et droits)");
      close(cmd_type);
      free(task_entry->cmd);
      free(tmp_path);
      return 0;
    }

    uint32_t sub_count = 0;
    while ((cmd_entry = readdir(cmd_dir))) {
      if (strcmp(cmd_entry->d_name, ".") != 0 && strcmp(cmd_entry->d_name, "..") != 0 &&
          strcmp(cmd_entry->d_name, "type") != 0) {
        sub_count++;
      }
    }
    closedir(cmd_dir);

    if (strcmp(command_type_str, "SQ") == 0) {
      task_entry->cmd->sequence.count = sub_count;
      task_entry->cmd->sequence.sub_commands = malloc(sizeof(command *) * sub_count);
    } else {
      task_entry->cmd->pipeline.count = sub_count;
      task_entry->cmd->pipeline.sub_commands = malloc(sizeof(command *) * sub_count);
    }

    command **sub_commands = (strcmp(command_type_str, "SQ") == 0) ? task_entry->cmd->sequence.sub_commands
                                                                   : task_entry->cmd->pipeline.sub_commands;

    for (uint32_t i = 0; i < sub_count; i++) {
      char sub_name[16];
      sprintf(sub_name, "%u", i);
      char *sub_cmd_path = malloc(strlen(cmd_path) + strlen(sub_name) + 5);
      sprintf(sub_cmd_path, "%s/%s", cmd_path, sub_name);

      task temp_task;
      temp_task.id = task_entry->id;
      temp_task.cmd = NULL;

      if (!deserialize_command_internal(sub_cmd_path, &temp_task)) {
        printf("Erreur lors de la désérialisation de la commande "
               "du task %d, sub-command %s\n",
               task_entry->id, sub_name);
        free(sub_cmd_path);
        for (uint32_t j = 0; j < i; j++) {
          free_command(sub_commands[j]);
        }
        free(sub_commands);
        close(cmd_type);
        free(task_entry->cmd);
        free(tmp_path);
        return 0;
      }

      sub_commands[i] = temp_task.cmd;
      free(sub_cmd_path);
    }
  } else if (strcmp(command_type_str, "IF") == 0) {
    task_entry->cmd->type = CMD_TYPE_IF;

    task_entry->cmd->if_cmd.cond_count = 0;
    task_entry->cmd->if_cmd.cond = NULL;
    task_entry->cmd->if_cmd.then_count = 0;
    task_entry->cmd->if_cmd.then_cmd = NULL;
    task_entry->cmd->if_cmd.else_count = 0;
    task_entry->cmd->if_cmd.else_cmd = NULL;

    char *cond_path = malloc(strlen(cmd_path) + 10);
    char *then_path = malloc(strlen(cmd_path) + 10);
    char *else_path = malloc(strlen(cmd_path) + 10);
    sprintf(cond_path, "%s/cond", cmd_path);
    sprintf(then_path, "%s/then", cmd_path);
    sprintf(else_path, "%s/else", cmd_path);

    DIR *cond_dir = opendir(cond_path);
    if (cond_dir) {
      uint32_t cond_count = 0;
      while ((cmd_entry = readdir(cond_dir))) {
        if (strcmp(cmd_entry->d_name, ".") != 0 && strcmp(cmd_entry->d_name, "..") != 0 &&
            strcmp(cmd_entry->d_name, "type") != 0) {
          cond_count++;
        }
      }
      closedir(cond_dir);

      if (cond_count > 0) {
        task_entry->cmd->if_cmd.cond_count = cond_count;
        task_entry->cmd->if_cmd.cond = malloc(sizeof(command *) * cond_count);
        for (uint32_t i = 0; i < cond_count; i++) {
          char *sub_path = malloc(strlen(cond_path) + 20);
          sprintf(sub_path, "%s/%u", cond_path, i);
          task temp_task;
          temp_task.id = task_entry->id;
          temp_task.cmd = NULL;
          if (deserialize_command_internal(sub_path, &temp_task)) {
            task_entry->cmd->if_cmd.cond[i] = temp_task.cmd;
          }
          free(sub_path);
        }
      }
    }

    DIR *then_dir = opendir(then_path);
    if (then_dir) {
      uint32_t then_count = 0;
      while ((cmd_entry = readdir(then_dir))) {
        if (strcmp(cmd_entry->d_name, ".") != 0 && strcmp(cmd_entry->d_name, "..") != 0 &&
            strcmp(cmd_entry->d_name, "type") != 0) {
          then_count++;
        }
      }
      closedir(then_dir);

      if (then_count > 0) {
        task_entry->cmd->if_cmd.then_count = then_count;
        task_entry->cmd->if_cmd.then_cmd = malloc(sizeof(command *) * then_count);
        for (uint32_t i = 0; i < then_count; i++) {
          char *sub_path = malloc(strlen(then_path) + 20);
          sprintf(sub_path, "%s/%u", then_path, i);
          task temp_task;
          temp_task.id = task_entry->id;
          temp_task.cmd = NULL;
          if (deserialize_command_internal(sub_path, &temp_task)) {
            task_entry->cmd->if_cmd.then_cmd[i] = temp_task.cmd;
          }
          free(sub_path);
        }
      }
    }

    DIR *else_dir = opendir(else_path);
    if (else_dir) {
      uint32_t else_count = 0;
      while ((cmd_entry = readdir(else_dir))) {
        if (strcmp(cmd_entry->d_name, ".") != 0 && strcmp(cmd_entry->d_name, "..") != 0 &&
            strcmp(cmd_entry->d_name, "type") != 0) {
          else_count++;
        }
      }
      closedir(else_dir);

      if (else_count > 0) {
        task_entry->cmd->if_cmd.else_count = else_count;
        task_entry->cmd->if_cmd.else_cmd = malloc(sizeof(command *) * else_count);
        for (uint32_t i = 0; i < else_count; i++) {
          char *sub_path = malloc(strlen(else_path) + 20);
          sprintf(sub_path, "%s/%u", else_path, i);
          task temp_task;
          temp_task.id = task_entry->id;
          temp_task.cmd = NULL;
          if (deserialize_command_internal(sub_path, &temp_task)) {
            task_entry->cmd->if_cmd.else_cmd[i] = temp_task.cmd;
          }
          free(sub_path);
        }
      }
    }

    free(cond_path);
    free(then_path);
    free(else_path);
  } else {
    printf("Type de commande inconnu dans le fichier %s/type type : "
           "%s\n",
           cmd_path, command_type_str);
    close(cmd_type);
    free(task_entry->cmd);
    free(tmp_path);
    return 0;
  }

  free(tmp_path);
  close(cmd_type);
  return 1;
}

int deserialize_command(char *path, task *task_entry) {
  char *cmd_path = malloc(strlen(path) + 10 * sizeof(char));
  if (!cmd_path) {
    perror("malloc failed for cmd_path");
    return 0;
  }
  sprintf(cmd_path, "%s/cmd", path);
  int result = deserialize_command_internal(cmd_path, task_entry);
  free(cmd_path);
  return result;
}

int read_tree(char *path, list_of_tasks *tasks) {
  char tasks_path[PATH_MAX];
  snprintf(tasks_path, sizeof(tasks_path), "%s/tasks", path);
  
  DIR *tree = opendir(tasks_path);
  if (tree == NULL) {
    printf("le chemin %s n'a pas pu être ouvert (vérifiez votre chemin)\n", tasks_path);
    return 0;
  }
  
  struct dirent *entry;
  while ((entry = readdir(tree))) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      task task_entry;
      task_entry.id = atoi(entry->d_name);
      
      if (task_entry.id >= g_next_task_id) {
        g_next_task_id = (uint32_t)task_entry.id + 1;
        if (g_next_task_id > UINT16_MAX) {
          g_next_task_id = UINT16_MAX;
        }
      }
      char *full_path = malloc(strlen(tasks_path) + strlen(entry->d_name) + strlen("/timing") + 2);
      sprintf(full_path, "%s/%s/timing", tasks_path, entry->d_name);
      int timing_file = open(full_path, O_RDONLY);
      if (timing_file < 0) {
        printf("path %s", full_path);
        LOG_INFO("path : %s \n", full_path);
        perror("ficher timing introuvable ou inaccessible (verifiez les "
               "existences et droits)");
        free(full_path);
        continue;
      }
      uint64_t minutes_raw;
      uint32_t hours_raw;
      uint8_t days_raw;
      read(timing_file, &minutes_raw, 8);
      read(timing_file, &hours_raw, 4);
      read(timing_file, &days_raw, 1);
      task_entry.timing.minutes = be64toh(minutes_raw);
      task_entry.timing.hours = be32toh(hours_raw);
      task_entry.timing.days = days_raw;
      sprintf(full_path, "%s/%s", tasks_path, entry->d_name);
      if (!deserialize_command(full_path, &task_entry)) {
        printf("Erreur lors de la désérialisation de la commande du task %d\n", task_entry.id);
        close(timing_file);
        free(full_path);
        continue;
      }

      close(timing_file);
      free(full_path);
      // ajout du task à la liste
      task **new_tasks = realloc(tasks->tasks, sizeof(task *) * (tasks->count + 1));
      if (!new_tasks) {
        perror("realloc failed");
        free_command(task_entry.cmd);
        closedir(tree);
        return 0;
      }
      tasks->tasks = new_tasks;
      tasks->tasks[tasks->count] = malloc(sizeof(task));
      if (!tasks->tasks[tasks->count]) {
        perror("malloc failed ");
        free_command(task_entry.cmd);
        closedir(tree);
        return 0;
      }
      *(tasks->tasks[tasks->count]) = task_entry;
      tasks->count += 1;
    }
  }
  closedir(tree);
  return 1;
}
int remove_tree(const char *path) {
  struct stat st;

  if (lstat(path, &st) == -1) {
    if (errno == ENOENT)
      return 0;
    perror(path);
    return -1;
  }

  if (!S_ISDIR(st.st_mode)) {
    if (unlink(path) == -1 && errno != ENOENT)
      perror(path);
    return 0;
  }

  DIR *dir = opendir(path);
  if (!dir) {
    if (errno == ENOENT)
      return 0;
    perror(path);
    return -1;
  }

  struct dirent *entry;
  char full_path[PATH_MAX];

  while ((entry = readdir(dir)) != NULL) {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
      continue;

    snprintf(full_path, PATH_MAX, "%s/%s", path, entry->d_name);
    remove_tree(full_path);
  }

  closedir(dir);

  if (rmdir(path) == -1 && errno != ENOENT)
    perror(path);

  return 0;
}

int remove_task(char *path, task *task) {
  char task_path[PATH_MAX];
  snprintf(task_path, PATH_MAX, "%s/%i", path, task->id);
  remove_tree(task_path);

  return 0;
}
#include "../include/types.h"
#include "../include/logger.h"
#include <arpa/inet.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

uint32_t g_next_task_id = 1;

int alloc_command(uint16_t type, command **command) {
  switch (type) {
  case CMD_TYPE_SI:
    alloc_simple_command(command);
    break;
  case CMD_TYPE_SQ:
    alloc_sequence_command(command);
    break;
  case CMD_TYPE_PL:
    alloc_pipeline_command(command);
    break;
  case CMD_TYPE_IF:
    alloc_if_command(command);
    break;
  default:
    LOG_ERR("type %u not recognised \n", (unsigned)type);
    return -1;
    break;
  }
  if (command == NULL) {
    LOG_ERR("alloc failed \n");
    return -1;
  }
  return 0;
}

int alloc_simple_command(command **command) {
  *command = malloc(sizeof(**command));
  if (!*command) {
    perror("alloc_simple_command : Erreur allocation de mémoire");
    return -1;
  }
  (*command)->type = CMD_TYPE_SI;
  (*command)->simple.argc = 0;
  (*command)->simple.argv = NULL;
  return 0;
}
int alloc_sequence_command(command **command) {
  *command = malloc(sizeof(**command));
  if (!*command) {
    perror("alloc_sequence_command : Erreur allocation de mémoire");
    return -1;
  }
  (*command)->type = CMD_TYPE_SQ;
  (*command)->sequence.count = 0;
  (*command)->sequence.sub_commands = NULL;
  return 0;
}

int alloc_pipeline_command(command **command) {
  *command = malloc(sizeof(**command));
  if (!*command) {
    perror("alloc_pipeline_command : Erreur allocation de mémoire");
    return -1;
  }
  (*command)->type = CMD_TYPE_PL;
  (*command)->pipeline.count = 0;
  (*command)->pipeline.sub_commands = NULL;
  return 0;
}

int alloc_if_command(command **command) {
  *command = malloc(sizeof(**command));
  if (!*command) {
    perror("alloc_if_command : Erreur allocation de mémoire");
    return -1;
  }
  (*command)->type = CMD_TYPE_IF;
  (*command)->if_cmd.cond_count = 0;
  (*command)->if_cmd.cond = NULL;
  (*command)->if_cmd.then_count = 0;
  (*command)->if_cmd.then_cmd = NULL;
  (*command)->if_cmd.else_count = 0;
  (*command)->if_cmd.else_cmd = NULL;
  return 0;
}

int alloc_task(task **task) {
  *task = malloc(sizeof **task);
  if (!*task) {
    perror("alloc_task : Erreur allocation de mémoire");
    return -1;
  }
  (*task)->id = 0;
  (*task)->cmd = NULL;
  (*task)->timing.days = 0;
  (*task)->timing.hours = 0;
  (*task)->timing.minutes = 0;
  return 0;
}

int alloc_tasks_list(list_of_tasks **list) {
  (*list) = malloc(sizeof(list_of_tasks));
  if (!(*list)) {
    perror("alloc_tasks_list : Erreur allocation de mémoire");
    return -1;
  }
  (*list)->tasks = NULL;
  (*list)->count = 0;

  return 0;
}

int free_command(command *command) {
  if (command == NULL)
    return 0;

  switch (command->type) {
  case CMD_TYPE_SI:
    return free_simple_command(command);
  case CMD_TYPE_SQ:
    return free_sequence_command(command);
  case CMD_TYPE_PL:
    return free_pipeline_command(command);
  case CMD_TYPE_IF:
    return free_if_command(command);
  default:
    free(command);
    return 0;
  }
}
int free_simple_command(command *command) {
  if (command != NULL) {
    if (command->simple.argv != NULL) {
      for (uint32_t i = 0; i < command->simple.argc; i++) {
        free(command->simple.argv[i]);
      }
      free(command->simple.argv);
    }
    command->simple.argc = 0;
    free(command);
  }
  return 0;
}

int free_sequence_command(command *command) {
  if (command != NULL) {
    if (command->sequence.sub_commands != NULL) {
      for (uint32_t i = 0; i < command->sequence.count; i++) {
        free_command(command->sequence.sub_commands[i]);
      }
      free(command->sequence.sub_commands);
    }
    command->sequence.count = 0;
    free(command);
  }
  return 0;
}

int free_pipeline_command(command *command) {
  if (command != NULL) {
    if (command->pipeline.sub_commands != NULL) {
      for (uint32_t i = 0; i < command->pipeline.count; i++) {
        free_command(command->pipeline.sub_commands[i]);
      }
      free(command->pipeline.sub_commands);
    }
    command->pipeline.count = 0;
    free(command);
  }
  return 0;
}

int free_if_command(command *command) {
  if (command != NULL) {
    // Libérer les tableaux de commandes conditionnelles
    if (command->if_cmd.cond != NULL) {
      for (uint32_t i = 0; i < command->if_cmd.cond_count; i++) {
        free_command(command->if_cmd.cond[i]);
      }
      free(command->if_cmd.cond);
    }
    if (command->if_cmd.then_cmd != NULL) {
      for (uint32_t i = 0; i < command->if_cmd.then_count; i++) {
        free_command(command->if_cmd.then_cmd[i]);
      }
      free(command->if_cmd.then_cmd);
    }
    if (command->if_cmd.else_cmd != NULL) {
      for (uint32_t i = 0; i < command->if_cmd.else_count; i++) {
        free_command(command->if_cmd.else_cmd[i]);
      }
      free(command->if_cmd.else_cmd);
    }
    command->if_cmd.cond_count = 0;
    command->if_cmd.then_count = 0;
    command->if_cmd.else_count = 0;
    free(command);
  }
  return 0;
}

int free_task(task *task) {
  if (task != NULL) {
    task->id = 0;
    task->timing.days = 0;
    task->timing.hours = 0;
    task->timing.minutes = 0;
    free_command(task->cmd);
    free(task);
  }
  return 0;
}
int free_tasks_list(list_of_tasks *list) {
  if (list != NULL) {
    if (list->tasks != NULL) {
      for (uint32_t i = 0; i < list->count; i++) {
        free_task(list->tasks[i]);
      }
      free(list->tasks);
    }
    list->count = 0;
    free(list);
  }
  return 0;
}

// Format command to buffer (helper for compact display)
static void format_command_to_buffer(command *cmd, char *buffer, size_t size) {
  if (!cmd || !buffer) {
    snprintf(buffer, size, "[NULL]");
    return;
  }

  int pos = 0;
  if (cmd->type == CMD_TYPE_SI && cmd->simple.argv && cmd->simple.argc > 0) {
    for (uint32_t i = 0; i < cmd->simple.argc && pos < (int)size - 1; i++) {
      if (i > 0)
        pos += snprintf(buffer + pos, size - pos, " ");
      pos += snprintf(buffer + pos, size - pos, "%s", cmd->simple.argv[i]);
    }
  } else if (cmd->type == CMD_TYPE_SQ && cmd->sequence.sub_commands && cmd->sequence.count > 0) {
    for (uint32_t i = 0; i < cmd->sequence.count && pos < (int)size - 1; i++) {
      char tmp[256];
      format_command_to_buffer(cmd->sequence.sub_commands[i], tmp, sizeof(tmp));
      if (i > 0)
        pos += snprintf(buffer + pos, size - pos, " ; ");
      pos += snprintf(buffer + pos, size - pos, "%s", tmp);
    }
  } else if (cmd->type == CMD_TYPE_PL && cmd->pipeline.sub_commands && cmd->pipeline.count > 0) {
    for (uint32_t i = 0; i < cmd->pipeline.count && pos < (int)size - 1; i++) {
      char tmp[256];
      format_command_to_buffer(cmd->pipeline.sub_commands[i], tmp, sizeof(tmp));
      if (i > 0)
        pos += snprintf(buffer + pos, size - pos, " | ");
      pos += snprintf(buffer + pos, size - pos, "%s", tmp);
    }
  } else if (cmd->type == CMD_TYPE_IF) {
    snprintf(buffer, size, "[If]");
  } else {
    snprintf(buffer, size, "[Unknown]");
  }
}

int print_command(command *cmd) {
  if (cmd->type == CMD_TYPE_SI) {
    print_simple_command(&cmd->simple);
  } else if (cmd->type == CMD_TYPE_SQ) {
    print_seq_command(&cmd->sequence);
  } else if (cmd->type == CMD_TYPE_PL) {
    print_pipeline_command(&cmd->pipeline);
  } else if (cmd->type == CMD_TYPE_IF) {
    LOG_INFO("\t  [If command]\n");
  } else {
    LOG_ERR("Cmd type not recognized : %i \n", cmd->type);
    return -1;
  }
  return 0;
}
int print_simple_command(simple_command *cmd) {
  if (cmd == NULL) {
    LOG_WARN("\t[NULL]\n");
    return -1;
  }

  if (cmd->argv != NULL && cmd->argc > 0) {
    char buffer[512];
    int pos = 0;
    for (uint32_t i = 0; i < cmd->argc; i++) {
      if (i > 0)
        pos += sprintf(buffer + pos, " ");
      pos += sprintf(buffer + pos, "%s", cmd->argv[i]);
    }
    LOG_INFO("\t  %s\n", buffer);
  }
  return 0;
}

int print_seq_command(sequence_command *cmd) {
  if (cmd == NULL) {
    LOG_WARN("\t[NULL]");
    return -1;
  }

  for (uint32_t i = 0; i < cmd->count; i++) {
    if (cmd->sub_commands[i]) {
      LOG_INFO("\t[Sous-commande %u]\n", i);
      print_command(cmd->sub_commands[i]);
      LOG_INFO("\n");
    }
  }
  return 0;
}

int print_pipeline_command(pipeline_command *cmd) {
  if (cmd == NULL) {
    LOG_WARN("\t[NULL]");
    return -1;
  }

  for (uint32_t i = 0; i < cmd->count; i++) {
    if (cmd->sub_commands[i]) {
      LOG_INFO("\t[Sous-commande %u]\n", i);
      print_command(cmd->sub_commands[i]);
      LOG_INFO("\n");
    }
  }
  return 0;
}
// Fonction auxiliaire pour convertir un bitmask en liste de valeurs
// Exemple: bitmask=0x7 (bits 0,1,2 activés), max_bits=7, offset=1
// Résultat: buffer = "1, 2, 3"
static void format_bitmask(char *buffer, uint64_t bitmask, int max_bits, int offset, const char *empty_msg) {
  // Cas vides / plein
  if (bitmask == 0) {
    sprintf(buffer, "%s", empty_msg);
    return;
  }

  uint64_t full_mask = (max_bits >= 64) ? ~0ULL : ((1ULL << max_bits) - 1);
  if ((bitmask & full_mask) == full_mask) {
    strcpy(buffer, "*");
    return;
  }

  // Compression par intervalles: "a-b" ou valeur seule
  int pos = 0;
  int in_run = 0;
  int run_start = -1;

  for (int i = 0; i <= max_bits; i++) {
    int set = (i < max_bits) && (bitmask & (1ULL << i));

    if (set && !in_run) {
      in_run = 1;
      run_start = i;
    } else if (!set && in_run) {
      int run_end = i - 1;
      if (pos > 0)
        pos += snprintf(buffer + pos, 512 - pos, " ");
      if (run_start == run_end) {
        pos += snprintf(buffer + pos, 512 - pos, "%d", run_start + offset);
      } else {
        pos += snprintf(buffer + pos, 512 - pos, "%d-%d", run_start + offset, run_end + offset);
      }
      in_run = 0;
    }
  }
}

int print_task_list(list_of_tasks *list) {
  if (list == NULL) {
    LOG_WARN("Liste de taches [NULL]\n");
    return -1;
  }

  LOG_INFO("========== LISTE DES TACHES (%u tache(s)) ==========\n", list->count);

  for (uint32_t i = 0; i < list->count; i++) {
    if (list->tasks[i] != NULL) {
      task *t = list->tasks[i];
      char days_buf[256], hours_buf[256], mins_buf[256], cmd_buf[512];

      // Format timing
      format_bitmask(days_buf, t->timing.days, 7, 1, "-");
      format_bitmask(hours_buf, t->timing.hours, 24, 0, "-");
      format_bitmask(mins_buf, t->timing.minutes, 60, 0, "-");

      // Format command
      if (t->cmd != NULL) {
        format_command_to_buffer(t->cmd, cmd_buf, sizeof(cmd_buf));
      } else {
        snprintf(cmd_buf, sizeof(cmd_buf), "[Aucune commande]");
      }

      // Print on 3 lines
      LOG_INFO("[Tache %u] ID: %u\n", i, t->id);
      LOG_INFO("  Timing: j=%s h=%s m=%s\n", days_buf, hours_buf, mins_buf);
      LOG_INFO("  Cmd: %s\n", cmd_buf);
    }
  }

  LOG_INFO("====================================================\n");
  return 0;
}

// Copie profonde de simple_command->argv
static int copy_simple_argv(simple_command *dst, const simple_command *src) {
  if (!dst || !src)
    return -1;

  dst->argc = src->argc;
  dst->argv = NULL;

  if (src->argc == 0)
    return 0;

  dst->argv = calloc(src->argc, sizeof(char *));
  if (!dst->argv) {
    perror("copy_simple_argv : Erreur allocation de mémoire");
    return -1;
  }

  for (uint32_t i = 0; i < src->argc; i++) {
    if (src->argv && src->argv[i]) {
      dst->argv[i] = strdup(src->argv[i]);
      if (!dst->argv[i]) {
        perror("copy_simple_argv : Erreur allocation de mémoire");
        return -1;
      }
    }
  }
  return 0;
}

int copy_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index) {
  if (src == NULL || *src == NULL || dst == NULL) {
    if (src == NULL) {
      LOG_ERR("src NULL \n");
    }
    if (*src == NULL) {
      LOG_ERR("*src NULL \n");
    }
    if (dst == NULL) {
      LOG_ERR("dst NULL \n");
    }
    return -1;
  }

  switch ((*src)->type) {
  case CMD_TYPE_SI:
    return copy_simple_command(src, src_type, dst, dst_type, index);

  case CMD_TYPE_SQ:
    return copy_sequence_command(src, src_type, dst, dst_type, index);

  case CMD_TYPE_PL:
    return copy_pipeline_command(src, src_type, dst, dst_type, index);

  case CMD_TYPE_IF:
    return copy_if_command(src, src_type, dst, dst_type, index);

  default:
    LOG_ERR("type not recognised : %i \n", (*src)->type);
  }

  return -1;
}

int alloc_dst_subcommand(command **dst, uint32_t src_type, uint32_t index) {
  if (dst == NULL) {
    LOG_ERR("dst is NULL");
    return -1;
  }

  int ret = 0;
  switch (src_type) {
  case CMD_TYPE_SI:
    ret = alloc_simple_command(&dst[index]);
    if (ret == -1) {
      LOG_ERR("error alloc_simple_command \n");
      return ret;
    }
    break;

  case CMD_TYPE_SQ:
    ret = alloc_sequence_command(&dst[index]);
    if (ret == -1) {
      LOG_ERR("error alloc_sequence_command \n");
      return ret;
    }
    break;

  case CMD_TYPE_PL:
    ret = alloc_pipeline_command(&dst[index]);
    if (ret == -1) {
      LOG_ERR("error alloc_pipeline_command \n");
      return ret;
    }
    break;

  case CMD_TYPE_IF:
    ret = alloc_if_command(&dst[index]);
    if (ret == -1) {
      LOG_ERR("error alloc_if_command \n");
      return ret;
    }
    break;

  default:
    LOG_ERR("unkown type \n");
    break;
  }
  LOG_INFO("alloc_dst_subcommand : Allocated memory successfuly \n");

  return ret;
}

int copy_simple_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index) {
  if (src_type != CMD_TYPE_SI) {
    LOG_ERR("Wrong cmd type \n");
    return -1;
  }

  int ret = 0;
  ret = alloc_dst_subcommand(dst, src_type, index);
  if (ret == -1) {
    LOG_ERR("alloc_dst_subcommand \n");
    return ret;
  }

  command *dst_cmd = dst[index];
  uint32_t argc = (*src)->simple.argc;
  char **dst_argv = NULL;

  if (argc > 0) {
    dst_argv = malloc(argc * sizeof(char *));
    if (!dst_argv) {
      LOG_ERR("cpy_simple_command: malloc argv \n");
      return -1;
    }
    memset(dst_argv, 0, argc * sizeof(char *));
    dst_cmd->simple.argv = dst_argv;
  }

  dst_cmd->simple.argc = argc;

  for (uint32_t i = 0; i < argc; i++) {
    if ((*src)->simple.argv && (*src)->simple.argv[i]) {
      dst_argv[i] = strdup((*src)->simple.argv[i]);
      if (!dst_argv[i]) {
        LOG_ERR("cpy_simple_command: strdup \n");
        return -1;
      }
    }
  }

  return ret;
}

int copy_sequence_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index) {
  LOG_INFO("copy_sequence_command \n");
  if (src_type != CMD_TYPE_SQ) {
    LOG_ERR("Wrong cmd type \n");
    return -1;
  }

  int ret = 0;
  ret = alloc_dst_subcommand(dst, src_type, index);
  if (ret == -1) {
    LOG_ERR("alloc_dst_subcommand \n");
    return ret;
  }

  command *dst_cmd = dst[index];
  uint32_t dst_cmd_count = (*src)->sequence.count;

  // Allouer le tableau de sous-commandes
  if (dst_cmd_count > 0) {
    dst_cmd->sequence.sub_commands = calloc(dst_cmd_count, sizeof(command *));
    if (!dst_cmd->sequence.sub_commands) {
      LOG_ERR("cpy_sequence_command: calloc sub_commands \n");
      return -1;
    }
    dst_cmd->sequence.count = dst_cmd_count;

    // Allouer chaque sous-commande et la copier
    for (uint32_t i = 0; i < dst_cmd_count; i++) {
      if ((*src)->sequence.sub_commands && (*src)->sequence.sub_commands[i]) {
        ret = copy_command(&((*src)->sequence.sub_commands[i]), (*src)->sequence.sub_commands[i]->type,
                           &(dst_cmd->sequence.sub_commands[i]), (*src)->sequence.sub_commands[i]->type, 0);
        if (ret == -1) {
          LOG_ERR("copy_commmand failed at index %u\n", i);
          return ret;
        }
      }
    }
  }

  return ret;
}

int copy_pipeline_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index) {
  if (src_type != CMD_TYPE_PL) {
    LOG_ERR("Wrong cmd type \n");
    return -1;
  }

  int ret = 0;
  ret = alloc_dst_subcommand(dst, src_type, index);
  if (ret == -1) {
    LOG_ERR("alloc_dst_subcommand \n");
    return ret;
  }

  // Allouer le tableau de sous-commandes
  uint32_t count = (*src)->pipeline.count;
  if (count > 0) {
    dst[index]->pipeline.sub_commands = calloc(count, sizeof(command *));
    if (!dst[index]->pipeline.sub_commands) {
      LOG_ERR("cpy_pipeline_command: calloc sub_commands");
      return -1;
    }
    dst[index]->pipeline.count = count;

    // Allouer chaque sous-commande et la copier
    for (uint32_t i = 0; i < count; i++) {
      if ((*src)->pipeline.sub_commands && (*src)->pipeline.sub_commands[i]) {
        ret = copy_command(&((*src)->pipeline.sub_commands[i]), (*src)->pipeline.sub_commands[i]->type,
                           &(dst[index]->pipeline.sub_commands[i]), (*src)->pipeline.sub_commands[i]->type, 0);
        if (ret == -1) {
          LOG_ERR("cpy_command failed at index %u\n", i);
          return ret;
        }
      }
    }
  }

  return ret;
}

int copy_if_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index) {
  if (src_type != CMD_TYPE_IF) {
    LOG_ERR("Wrong cmd type \n");
    return -1;
  }

  int ret = 0;
  ret = alloc_dst_subcommand(dst, src_type, index);
  if (ret == -1) {
    LOG_ERR("error: alloc_dst_subcommand\n");
    return ret;
  }

  command *dst_cmd = dst[index];
  uint32_t cond_count = (*src)->if_cmd.cond_count;
  uint32_t then_count = (*src)->if_cmd.then_count;
  uint32_t else_count = (*src)->if_cmd.else_count;

  // Allouer et copier le tableau cond
  if (cond_count > 0) {
    dst_cmd->if_cmd.cond = malloc(cond_count * sizeof(command *));
    if (!dst_cmd->if_cmd.cond) {
      perror("cpy_if_command: malloc cond");
      return -1;
    }
    memset(dst_cmd->if_cmd.cond, 0, cond_count * sizeof(command *));
    dst_cmd->if_cmd.cond_count = cond_count;

    for (uint32_t i = 0; i < cond_count; i++) {
      if ((*src)->if_cmd.cond && (*src)->if_cmd.cond[i]) {
        ret = copy_command(&((*src)->if_cmd.cond[i]), (*src)->if_cmd.cond[i]->type, &(dst_cmd->if_cmd.cond[i]),
                           (*src)->if_cmd.cond[i]->type, 0);
        if (ret == -1) {
          LOG_ERR("cpy_command failed for cond at index %u\n", i);
          return ret;
        }
      }
    }
  }

  // Allouer et copier le tableau then_cmd
  if (then_count > 0) {
    dst_cmd->if_cmd.then_cmd = malloc(then_count * sizeof(command *));
    if (!dst_cmd->if_cmd.then_cmd) {
      perror("cpy_if_command: malloc then_cmd");
      return -1;
    }
    memset(dst_cmd->if_cmd.then_cmd, 0, then_count * sizeof(command *));
    dst_cmd->if_cmd.then_count = then_count;

    for (uint32_t i = 0; i < then_count; i++) {
      if ((*src)->if_cmd.then_cmd && (*src)->if_cmd.then_cmd[i]) {
        ret = copy_command(&((*src)->if_cmd.then_cmd[i]), (*src)->if_cmd.then_cmd[i]->type,
                           &(dst_cmd->if_cmd.then_cmd[i]), (*src)->if_cmd.then_cmd[i]->type, 0);
        if (ret == -1) {
          LOG_ERR("cpy_command failed for then_cmd at index %u\n", i);
          return ret;
        }
      }
    }
  }

  // Allouer et copier le tableau else_cmd
  if (else_count > 0) {
    dst_cmd->if_cmd.else_cmd = malloc(else_count * sizeof(command *));
    if (!dst_cmd->if_cmd.else_cmd) {
      perror("cpy_if_command: malloc else_cmd");
      return -1;
    }
    memset(dst_cmd->if_cmd.else_cmd, 0, else_count * sizeof(command *));
    dst_cmd->if_cmd.else_count = else_count;

    for (uint32_t i = 0; i < else_count; i++) {
      if ((*src)->if_cmd.else_cmd && (*src)->if_cmd.else_cmd[i]) {
        ret = copy_command(&((*src)->if_cmd.else_cmd[i]), (*src)->if_cmd.else_cmd[i]->type,
                           &(dst_cmd->if_cmd.else_cmd[i]), (*src)->if_cmd.else_cmd[i]->type, 0);
        if (ret == -1) {
          LOG_ERR("cpy_command failed for else_cmd at index %u\n", i);
          return ret;
        }
      }
    }
  }

  return ret;
}

int add_task_to_list(list_of_tasks *list, task *new_task) {
  if (!list || !new_task)
    return -1;

  // Assign a unique ID if the task doesn't have one
  if (new_task->id == 0) {
    if (g_next_task_id > UINT16_MAX) {
      LOG_ERR("Cannot assign task ID: limit reached\n");
      return -1;
    }
    new_task->id = (uint16_t)g_next_task_id;
    g_next_task_id++;
  }

  task **tmp = realloc(list->tasks, sizeof(task *) * (list->count + 1));
  if (!tmp)
    return -1;

  list->tasks = tmp;
  list->tasks[list->count] = new_task;
  list->count++;

  return 0;
}

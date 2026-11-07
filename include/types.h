#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdio.h>

#define CMD_TYPE_SI 0x5349
#define CMD_TYPE_SQ 0x5351
#define CMD_TYPE_PL 0x504C
#define CMD_TYPE_IF 0x4946
#define MAX_ARGV 1024

struct command;

typedef struct {
  uint32_t length;
  char *data;
} string;

typedef struct timing {
  uint64_t minutes;
  uint32_t hours;
  uint8_t days;
} timing;

typedef struct {
  uint32_t argc;
  char **argv;
} simple_command;

typedef struct {
  uint32_t count;
  struct command **sub_commands;
} sequence_command;

typedef struct {
  uint32_t count;
  struct command **sub_commands; // chaque command sera exécutée en pipeline
} pipeline_command;

typedef struct {
  uint32_t cond_count;
  struct command **cond; // tableau de commandes conditionnelles

  uint32_t then_count;
  struct command **then_cmd; // tableau de commandes à exécuter si cond réussit

  uint32_t else_count;
  struct command **else_cmd; // tableau de commandes à exécuter si cond échoue

} if_command;

typedef struct command {
  uint16_t type;

  union {
    simple_command simple;
    sequence_command sequence;
    pipeline_command pipeline;
    if_command if_cmd;
  };
} command;

typedef struct task {
  uint16_t id;
  timing timing;
  command *cmd;
} task;

typedef struct {
  task **tasks;
  uint32_t count;
} list_of_tasks;

extern uint32_t g_next_task_id;

int alloc_command(uint16_t type, command **command);
int alloc_simple_command(command **command);
int alloc_sequence_command(command **command);
int alloc_pipeline_command(command **command);
int alloc_if_command(command **command);
int alloc_task(task **task);
int alloc_tasks_list(list_of_tasks **list);

int free_command(command *command);
int free_simple_command(command *command);
int free_sequence_command(command *command);
int free_pipeline_command(command *command);
int free_if_command(command *command);
int free_task(task *task);
int free_tasks_list(list_of_tasks *list);

int print_command(command *cmd);
int print_simple_command(simple_command *cmd);
int print_seq_command(sequence_command *cmd);
int print_pipeline_command(pipeline_command *cmd);
int print_task_list(list_of_tasks *list);

// command *copy_command(command *src);

int deserialise_task(int fd_in, char rep_pipe[], task **new_task);
int deserialise_task_timing(int fd_in, task **new_task);
int deserialise_command_type(int fd_in, task **new_task);
int deserialise_task_id_list(int fd_in, uint16_t task_ids[], int nb_tasks);
// copies a command from src into dst

int copy_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index);
int copy_simple_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index);
int copy_sequence_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index);
int copy_pipeline_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index);
int copy_if_command(command **src, uint32_t src_type, command **dst, uint32_t dst_type, uint32_t index);

int alloc_dst_subcommand(command **dst, uint32_t src_type, uint32_t index);

int add_task_to_list(list_of_tasks *list, task *new_task);

#endif

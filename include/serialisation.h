#include "types.h"
#include <stdint.h>
#include <stdio.h>

int read_tree(char *path, list_of_tasks *tasks);
int get_bit_value(uint64_t number, int bit);
int should_execute(task *task_entry, int current_day, int current_hour, int current_minute);
uint64_t change_indian_format(uint64_t num);
uint16_t swap_uint16(uint16_t val);
uint32_t swap_uint32(uint32_t val);

int deserialize_command_internal(char *cmd_path, task *task_entry);
int deserialize_command(char *path, task *task_entry) ;
int remove_task(char *path, task *task);
int remove_tree(const char *path);


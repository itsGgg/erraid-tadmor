#include "types.h"
#include <stddef.h>
#include <sys/types.h>

int exec_command(command *cmd, pid_t **pids_to_terminate, size_t *num_pids);
int exec_simple_command(simple_command *cmd, pid_t **pids_to_terminate, size_t *num_pids);
int exec_sequence_command(sequence_command *cmd, pid_t **pids_to_terminate, size_t *num_pids);
int exec_pipeline_command(pipeline_command *cmd, pid_t **pids_to_terminate, size_t *num_pids);
int exec_if_command(if_command *cmd, pid_t **pids_to_terminate, size_t *num_pids);

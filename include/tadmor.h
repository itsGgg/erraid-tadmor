#include "../include/types.h"
#include <stdint.h>

void print_all_options();
void print_actions();

void read_command(int fd_out, int depth);
void format_timing_field(char *buf, uint64_t bits, int max_bits);
void pipe_paths(char *req_pipe, char *rep_pipe, const char *pipes_dir);
void read_command(int fd_out, int depth);

int parse_sub_options(int argc, char *argv[], const char *opts, task **task);

int request_list_tasks(const char *pipes_dir);
int request_task_stdout(const char *pipes_dir, uint64_t task_id);
int request_task_stderr(const char *pipes_dir, uint64_t task_id);
int request_task_times_exit(const char *pipes_dir, uint64_t task_id);
int request_remove_task(const char *pipes_dir, uint64_t task_id);
int request_terminate(const char *pipes_dir);

int request_create_simple_task(const char *pipes_dir, task **new_task, int argc, char *argv[],
                               int optind); // optind = index of the next element to be processed in argv
int request_create_sequence_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind);
int request_create_pipeline_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind);
int request_create_conditional_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind);

int request_combine_task(const char *pipes_dir, task **new_task, int argc, char *argv[], int optind, uint16_t type);

int is_shell_symbol(const char *arg);
int serialise_task_ids(int req_pipe, int argc, char *argv[], int optind);

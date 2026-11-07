#include "types.h"
#include <sys/types.h>

void print_options();
int create_main_demon(list_of_tasks *tasks_list, char *path, char *pipes_path);
int init_erraid_pipes(const char *base_path, char req_pipe[], char rep_pipe[]);
pid_t create_scheduler(list_of_tasks *tasks_list, char *path);

int decode_single_bit_u64(uint64_t mask);
int decode_single_bit_u32(uint32_t mask);
int decode_single_bit_u8(uint8_t mask);
int stockage_argument(int fd, command *cmd);
int stockage_argument_si(int fd, command *cmd);
int stockage_argument_sq(int fd, command *cmd);
int stockage_argument_if(int fd, command *cmd);
int stockage_argument_pl(int fd, command *cmd);
int initialise_default_tree();
int ensure_dir(const char *path);

// Handlers des rêquetes client
int handle_list_tasks(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                      char *path); // OPCODE_LIST (0x4C53) Lister toutes les tâches
int handle_task_stdout(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                       char *path); // OPCODE_STDOUT (0x534F) Retour stdout dernière exécution
int handle_task_stderr(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                       char *path); // OPCODE_STDERR (0x5345) Retour stderr dernière exécution
int handle_task_times_exit(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                           char *path); // OPCODE_TIMES_EXIT (0x5458) Historique des sorties
int handle_remove_task(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                       char *path); // OPCODE_REMOVE (0x524D) Supprimer une tâche
int handle_create_task(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                       char *path); // OPCODE_CREATE (0x4352) Créer une tâche
int handle_combine_tasks(int fd_in, char rep_pipe[], list_of_tasks *tasks_list,
                         char *path);             // OPCODE_COMBINE (0x4342) Combiner des tâches
int handle_terminate(int fd_in, char rep_pipe[]); // OPCODE_TERMINATE (0x4B49) Terminer le daemon ou une tâche

// int create_task_on_disk(char *path, task *task, char *task_path);
int store_command(char *path, command *cmd);
int store_simple_command(char *path, command *cmd);
int store_sequence_command(char *path, command *cmd);
int store_pipeline_command(char *path, command *cmd);
int store_if_command(char *path, command *cmd);

int store_timing(char *task_path, task *task);
int store_task(char *path, task *task);

task *get_task_from_list(list_of_tasks *list, uint16_t id);
int fill_task_from_ids(task **new_task, list_of_tasks *tasks_list, uint16_t *task_ids, uint32_t nb_tasks);

int store_argv();
int store_type(char *path, uint16_t cmd_type);

int deserialise_task_id_list(int fd_in, uint16_t *task_ids, int nb_tasks);

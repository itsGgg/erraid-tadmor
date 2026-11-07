
#include "../include/logger.h"
#include <limits.h>
#include <linux/limits.h>

// ------------  Variables globales ------------
log_context_t LOG_CURRENT_CTX = LOG_CTX_STDERR; // Ecriture par défaut des logs sur STDERR_FILENO
int LOG_FD = STDERR_FILENO;

int g_activate_daemon = DEFAULT_ACTIVATE_DAEMON;
int g_log_to_file = DEFAULT_LOG_TO_FILE;
int g_scheduler_log_enabled = DEFAULT_SCHEDULER_LOG_ENABLED;

// ----------------------------------------------

void logger_write_ctx(log_context_t log_context_type, const char *log_type, const char *file, int line, const char *fmt,
                      ...) {

  if (log_context_type >= LOG_CTX_NONE || log_context_type < 0) {
    return;
  }

  if (log_context_type == LOG_CTX_TADMOR && log_context_type == 0) { // évite doublons côté tadmor
    return;
  }

  char buffer[BUFFER_SIZE];
  const char *truncated_log = "[LOG_TRUNCATED]";

  int prefix_size = 0;
  int content_size = 0;

  // On met le prefixe dans le buffer. Exemple prefixe : "[INFO] src/main.c:28 - "
  if ((LOG_CURRENT_CTX != LOG_CTX_STDERR) || !isatty(LOG_FD)) { // Ecriture sans couleur si fichier
    prefix_size = snprintf(buffer, BUFFER_SIZE, "%s %s:%i - ", log_type, file, line);
  } else { // Ecriture avec couleur si terminal
    if (strcmp(log_type, "[INFO]") == 0) {
      prefix_size = snprintf(buffer, BUFFER_SIZE, "\033[34m%s\033[34m\033[0m %s:%i - ", log_type, file, line); // Bleu

    } else if (strcmp(log_type, "[WARNING]") == 0) {
      prefix_size = snprintf(buffer, BUFFER_SIZE, "\033[33m%s\033[33m\033[0m %s:%i - ", log_type, file, line); // Jaune

    } else if (strcmp(log_type, "[ERROR]") == 0) {
      prefix_size = snprintf(buffer, BUFFER_SIZE, "\033[31m%s\033[31m\033[0m %s:%i - ", log_type, file, line); // Rouge
    } else {
      prefix_size = snprintf(buffer, BUFFER_SIZE, "%s %s:%i - ", log_type, file, line); // Failback si type pas reconnu
    }
  }

  // Initialisation va_list
  va_list args_list;
  // Fais pointer args_list vers le premier argument se trouvant après fmt
  va_start(args_list, fmt);

  // Mettre le contenu dans le buffer en remplaçant chaque % de la chaîne fmt
  content_size = vsnprintf(buffer + prefix_size, BUFFER_SIZE - prefix_size, fmt, args_list);
  if (content_size < 0)
    content_size = 0;

  va_end(args_list);

  // Calcul de la taille à écrire
  int total_size = prefix_size + content_size;
  // Si taille prefixe + taille contenu dépasse taille du buffer on tronque truncated_size + 1 octets pour rajouter
  // [LOG_TRUNCATED]\0
  if (total_size >= BUFFER_SIZE) {
    int truncated_size = strlen(truncated_log);

    snprintf(buffer + (BUFFER_SIZE - truncated_size - 1), truncated_size + 1, "%s", truncated_log);
    total_size = BUFFER_SIZE - 1;
    buffer[BUFFER_SIZE - 1] = '\0';
  }

  // Ecriture dans un fd basé sur le contexte
  write_msg(LOG_FD, buffer, total_size);
}

// ctx <=3
int init_logger_fd(char *path) {
  if (LOG_CURRENT_CTX > 3 || LOG_CURRENT_CTX < 0) {
    perror("Log_context doit être <= 3 && >=0");
    return -1;
  }
  LOG_FD = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (LOG_FD == -1) {
    perror("open() : LOGGER_FD");
    return -1;
  }

  return 0;
}

void init_erraid_log_file(const char *path) {

  char base_path[PATH_MAX];
  if (path == NULL) {
    const char *user = get_user();
    snprintf(base_path, sizeof(base_path), "/tmp/%s/erraid", user);
  } else {
    strncpy(base_path, path, PATH_MAX - 1);
    base_path[PATH_MAX - 1] = '\0';
  }

  char *daemon_log_path = join_path2(base_path, ERRAID_LOG_FILE);

  set_logger_ctx(LOG_CTX_ERRAID);
  init_logger_fd(daemon_log_path);

  // Maintenant on peut logger dans le fichier
  LOG_INFO("Ouverture de %s et mise en place du nouveau contexte - LOG_CURRENT_CTX : %s \n", daemon_log_path,
           log_context_to_string(LOG_CURRENT_CTX));
  LOG_INFO("LOG_FD : %i ; LOG_CURRENT_CTX : %s \n", LOG_FD, log_context_to_string(LOG_CURRENT_CTX));
  free(daemon_log_path);
}

void init_scheduler_log_file(const char *path) {
  int fd_scheduler_log = -1;
  char *scheduler_log_path = join_path2(path, SCHEDULER_LOG_FILE);

  close_fd();
  set_logger_ctx(LOG_CTX_SCHEDULER);
  init_logger_fd(scheduler_log_path);

  LOG_INFO("[SCHEDULER] LOG_FD : %i ; LOG_CURRENT_CTX : %s \n", LOG_FD, log_context_to_string(LOG_CURRENT_CTX));
  LOG_INFO("SCHEDULER] Mise en place du nouveau contexte - LOG_CURRENT_CTX : %s \n",
           log_context_to_string(LOG_CURRENT_CTX));
  LOG_INFO("SCHEDULER] Ouverture de %s \n", scheduler_log_path);

  free(scheduler_log_path);
}

void init_tadmor_log_file(const char *path) {
  char *tadmor_log_path = join_path2(path, TADMOR_LOG_FILE);
  set_logger_ctx(LOG_CTX_TADMOR);
  init_logger_fd(tadmor_log_path);
  LOG_INFO("Ouverture de  %s et mise en place du nouveau contexte - LOG_CURRENT_CTX : %s \n", tadmor_log_path,
           log_context_to_string(LOG_CURRENT_CTX));

  free(tadmor_log_path);
}

void set_logger_ctx(log_context_t ctx) { LOG_CURRENT_CTX = ctx; }

int close_fd() {
  if (LOG_FD != -1) {
    if (close(LOG_FD) == -1) {
      if (fcntl(LOG_FD, F_GETFD) == -1) {
        if (errno == EBADF) {
          perror("fd is closed / never initialised");
        }
      }

      return -1;
    }
    dprintf(2, "Fermeture du fd:  %i , de contexte : %i \n", LOG_FD, LOG_CURRENT_CTX);
    LOG_FD = -1;
    set_logger_ctx(LOG_CTX_NONE);
    dprintf(2, "Nouveau fd : %i ; nouveau contexte :  %i \n", LOG_FD, LOG_CURRENT_CTX);
  }
  return 0;
}

const char *log_context_to_string(log_context_t log_context) {
  switch (log_context) {
  case LOG_CTX_ERRAID:
    return "LOG_CTX_ERRAID";
  case LOG_CTX_SCHEDULER:
    return "LOG_CTX_SCHEDULER";
  case LOG_CTX_STDERR:
    return "LOG_CTX_STDERR";
  case LOG_CTX_TADMOR:
    return "LOG_CTX_TADMOR";
  case LOG_CTX_NONE:
    return "LOG_CTX_NONE";
  default:
    return "LOG_CTX_UNKNOWN";
  }
}

void fallback_activated_daemon() {
  if (g_activate_daemon == 1) {
    g_log_to_file = 1;
  }
}
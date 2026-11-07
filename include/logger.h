#ifndef LOGGER_H
#define LOGGER_H

#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Fichiers de log
#define ERRAID_LOG_FILE "erraid.log"
#define SCHEDULER_LOG_FILE "scheduler.log"
#define TADMOR_LOG_FILE "tadmor.log"

// Taille maximale buffer de log
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif

// #define ACTIVATE_DAEMON 1 // 1 = mode démon activé (setsid()), 0 = mode sans (setsid())
// #define LOG_TO_FILE 1     // 1 = écriture dans les fichiers, 0 = affichage sur stdout/stderr
// #define SCHEDULER_LOG_ENABLED 0
// 1 = le scheduler écrit dans scheduler.log ; 0 = le scheduler n'écrit pas dans scheduler.log

#ifndef DEFAULT_ACTIVATE_DAEMON
#define DEFAULT_ACTIVATE_DAEMON 1 // 1 = mode démon activé(setsid()), 0 = mode sans(setsid())
#endif

#ifndef DEFAULT_LOG_TO_FILE
#define DEFAULT_LOG_TO_FILE 1 // 1 = écriture dans les fichiers, 0 = affichage sur stdout/stderr
#endif

#ifndef DEFAULT_SCHEDULER_LOG_ENABLED
#define DEFAULT_SCHEDULER_LOG_ENABLED 1
// 1 = le scheduler écrit dans scheduler.log ; 0 = le scheduler n'écrit pas dans scheduler.log
#endif

// // Failback si démon activé, logging vers fichier automatique
// #if ACTIVATE_DAEMON == 1
// #define LOG_TO_FILE 1
// #endif

extern int g_activate_daemon; // Variables globales
extern int g_log_to_file;
extern int g_scheduler_log_enabled;

void fallback_activated_daemon();

typedef enum {
  LOG_CTX_ERRAID,    // LOG_CTX pour erraid - 0
  LOG_CTX_SCHEDULER, // LOG_CTX pour scheduler - 1
  LOG_CTX_STDERR,    // LOG_CTX pour STDERR_FILENO - 2
  LOG_CTX_TADMOR,    // LOG_CTX pour tadmor - 3
  LOG_CTX_NONE       // LOG_CTX pour aucun log - 4
} log_context_t;

extern int LOG_FD;
extern log_context_t LOG_CURRENT_CTX;

#ifndef LOG_LEVEL
#define LOG_LEVEL 0 // Niveau de log par défaut : 0 = désactivé, 1 = info, 2 = warning, 3 = erreur
#endif

// Déclaration de la fonction de logging
// "fmt" + "..." permet d'utiliser des formats de la forme "Path %s"

void logger_write_ctx(log_context_t log_context_type, const char *log_type, const char *file, int line, const char *fmt,
                      ...);

// Selon LOG_LEVEL, les macros LOG_INFO,LOG_WARN,LOG_ERR appellent logger_write ou ne font rien

#if LOG_LEVEL >= 1
#define LOG_INFO(fmt, ...) logger_write_ctx(LOG_CURRENT_CTX, "[INFO]", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(...) ((void)0)
#endif

#if LOG_LEVEL >= 2
#define LOG_WARN(fmt, ...) logger_write_ctx(LOG_CURRENT_CTX, "[WARNING]", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(...) ((void)0)
#endif

#if LOG_LEVEL >= 3
#define LOG_ERR(fmt, ...) logger_write_ctx(LOG_CURRENT_CTX, "[ERROR]", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_ERR(...) ((void)0)
#endif

const char *log_context_to_string(log_context_t log_context);

int init_logger_fd(char *path);

void init_erraid_log_file(const char *path);
void init_scheduler_log_file(const char *path);
void init_tadmor_log_file(const char *path);

void set_logger_ctx(log_context_t ctx);

int close_fd();

#endif

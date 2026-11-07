CC = gcc
CFLAGS = -Wall -Wextra -Wno-unused -Iinclude

SRC_DIR = src
BUILD_DIR = build

LOG_NONE  = -DLOG_LEVEL=0
LOG_INFO  = -DLOG_LEVEL=1
LOG_WARN  = -DLOG_LEVEL=2
LOG_ERROR = -DLOG_LEVEL=3

# Récupère automatiquement tous les .c
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Convertit src/*.c -> build/*.o
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# --------- Flags --------- #
all: CFLAGS += $(LOG_NONE)
all: clean erraid tadmor

info: CFLAGS += $(LOG_INFO)
info: clean erraid  tadmor

warning: CFLAGS += $(LOG_WARN)
warning: clean erraid  tamdor

error: CFLAGS += $(LOG_ERROR)
error: clean erraid tadmor

# --------- Compilation des .o  ---------

# build/%.o dépend de src/%.c
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# crée build/ si absent
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

erraid: $(filter-out $(BUILD_DIR)/tadmor.o, $(OBJS))
	$(CC) $(CFLAGS) $^ -o erraid

tadmor: $(BUILD_DIR)/tadmor.o $(filter-out $(BUILD_DIR)/main.o, $(OBJS))
	$(CC) $(CFLAGS) $^ -o tadmor


	
# --------- Nettoyage ---------

# Tue les processus en cours
kill:
	@pkill -9 erraid 2>/dev/null || true
	@pkill -9 tadmor 2>/dev/null || true
	@sleep 1

# Nettoyage complet
distclean clean: kill
	rm -rf $(BUILD_DIR) erraid tadmor

# Nettoyage puis récreation des fichiers .o
re: clean all

# --------- Tests ---------

test: all
	@echo "Test: Messages d'erreur "
	@./tadmor 2>&1 | grep -q "Usage" && echo "✓ Usage OK" || echo "✗ Usage FAIL"
	@./tadmor -o abc 2>&1 | grep -q "invalide" && echo "✓ Validation ID OK" || echo "✗ Validation ID FAIL"
	@./tadmor -l 2>&1 | grep -q "daemon" && echo "✓ Message daemon OK" || echo "✗ Message daemon FAIL"
	@echo ""

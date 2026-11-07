Architecture du projet (erraid / tadmor)

1) Vue d’ensemble

Le projet implémente un mini-système de planification de tâches en deux exécutables principaux :
	•	erraid : un daemon qui maintient une liste de tâches, reçoit des requêtes via des pipes (FIFO), persiste les tâches sur disque, et lance un scheduler en sous-processus pour exécuter les tâches au bon moment.
	•	tadmor : un client  qui envoie des requêtes au daemon (création, liste, suppression, récupération stdout/stderr, historique d’exécutions, terminaison).

Le scheduler exécute les commandes via fork/exec, redirige stdout/stderr vers des fichiers, et stocke l’historique (timestamp + exit code).


2) Concepts / Modèle de données

2.1 Tâche

Une tâche est définie par :
	•	id (uint16)
	•	timing :
	•	minutes : bitmask 64 bits (on utilise 0..59)
	•	hours : bitmask 32 bits (0..23)
	•	days : bitmask 8 bits (1..7 dans l’affichage, mais en interne bits 0..6)
	•	cmd : une commande typée

Définition :
	•	task { uint16_t id; timing timing; command *cmd; }
	•	list_of_tasks { task **tasks; uint32_t count; }

2.2 Commandes supportées

Le projet supporte plusieurs “types” de commande :
	•	SI (simple command) : argv[] + argc
	•	SQ (sequence) : tableau de sous-commandes exécutées dans l’ordre, stop si une sous-commande échoue
	•	PL (pipeline) : structure présente et persistée
	•	IF (if/then/else) : exécution implémentée (conditions en séquence, puis branche then/else).

Constantes :
	•	CMD_TYPE_SI = 0x5349
	•	CMD_TYPE_SQ = 0x5351
	•	CMD_TYPE_PL = 0x504C
	•	CMD_TYPE_IF = 0x4946


3) Composants et responsabilités

3.1 erraid (daemon)

Responsabilités
	•	Initialiser l’arborescence par défaut (dans /tmp/$USER/erraid/...).
	•	Charger les tâches persistées au démarrage (read_tree()).
	•	Créer les FIFO de communication (init_erraid_pipes()).
	•	Boucler en attendant des requêtes sur la FIFO request.
	•	Répondre sur la FIFO reply.
	•	Lancer un scheduler (processus séparé) qui exécute les tâches.

Mode démon vs avant plan

Le comportement est piloté par 3 variables globales :
	•	g_activate_daemon : active le setsid() + redirection stdin/out/err vers /dev/null.
	•	g_log_to_file : logs vers fichiers vs stderr
	•	g_scheduler_log_enabled : active/désactive scheduler.log

Options CLI principales côté erraid :
	•	-F : désactive le mode démon (g_activate_daemon = 0)
	•	-L : désactive logs fichiers
	•	-S : désactive logs scheduler
	•	-r <path> : base path (arbre tasks/logs)
	•	-P <path> : base path pipes (répertoire “pipes”)

Fallback : si démon activé, fallback_activated_daemon() force g_log_to_file = 1.

⸻

3.2 Scheduler (create_scheduler + execute_tasks)

Le scheduler est un sous-processus forké par erraid.

Responsabilités
	•	Boucle infinie avec tick “toutes les minutes” :
	•	attend current_sec == 0
	•	évite double exécution sur la même minute (last_min)
	•	Pour chaque tâche :
	•	décide l’exécution via should_execute(task, day, hour, minute) (bitmasks)
	•	fork un task_executor
	•	fork un command_executor
	•	redirige stdout/stderr vers fichiers tasks/<id>/stdout et tasks/<id>/stderr
	•	exécute la commande (exec_command)
	•	attend la fin et écrit un enregistrement (timestamp + exit code) dans times-exitcodes

Artefacts écrits
	•	stdout : dernière sortie standard complète
	•	stderr : dernière sortie d’erreur complète
	•	times-exitcodes : append binaire de couples (timestamp, exitcode)

⸻

3.3 tadmor (client CLI)

Responsabilités
	•	Construire les chemins de pipes (avec ou sans -P)
	•	Envoyer une requête (opcode + payload) vers erraid-request-pipe
	•	Lire la réponse sur erraid-reply-pipe
	•	Afficher une sortie utilisateur (liste, stdout/stderr, historique, succès/erreur)

Actions supportées :
	•	-l : liste tacjes
	•	-c : créer tâche simple (argv après options)
	•	-s : combiner en sequence (SQ)
	•	-p : combiner en pipeline (PL)
	•	-i : combiner en if (IF)
	•	-r <id> : remove
	•	-o <id> : stdout
	•	-e <id> : stderr
	•	-x <id> : times-exitcodes
	•	-q : terminate daemon

Scheduling depuis tadmor :
	•	-m <minutes_bitmask?> -H <hours_bitmask?> -d <days_bitmask?>
	•	-n : no_schedule → force timing à 0 (désactive)


⸻

4) Protocole IPC (pipes) : opcodes / réponses

4.1 Pipes

Deux FIFOs sont créées (ou réutilisées si existent) :
	•	.../erraid-request-pipe
	•	.../erraid-reply-pipe

Par défaut :
	•	/tmp/$USER/erraid/pipes/erraid-request-pipe
	•	/tmp/$USER/erraid/pipes/erraid-reply-pipe

4.2 Opcodes

Définis dans protocol.h :
	•	OPCODE_LIST      = 0x4C53  (‘LS’)
	•	OPCODE_CREATE    = 0x4352  (‘CR’)
	•	OPCODE_COMBINE   = 0x4342  (‘CB’)
	•	OPCODE_REMOVE    = 0x524D  (‘RM’)
	•	OPCODE_TIMES_EXIT= 0x5458  (‘TX’)
	•	OPCODE_STDOUT    = 0x534F  (‘SO’)
	•	OPCODE_STDERR    = 0x5345  (‘SE’)
	•	OPCODE_TERMINATE = 0x4B49  (‘TM’)

4.3 Types de réponse (anstype) + erreurs
	•	ANSTYPE_OK    = 0x4F4B (‘OK’)
	•	ANSTYPE_ERROR = 0x4552 (‘ER’)

Codes d’erreur (quand ANSTYPE_ERROR) :
	•	ERRCODE_ENOEF = 0x4E46 (‘NF’) → tâche non trouvée / fichier absent
	•	ERRCODE_NOTRUN= 0x4E52 (‘NR’) → tâche jamais exécutée (pas de stdout/stderr)

4.4 Encodage / Endianness

Les échanges passent par des conversions big-endian (htobe16, htobe32, htobe64, ntohs, etc.).

5) Flux principaux

5.1 Démarrage
	1.	erraid parse ses options (mode démon/logs/paths)
	2.	crée l’arborescence si non fournie (default /tmp/$USER/erraid)
	3.	alloue la tasks_list
	4.	read_tree() recharge les tâches persistées
	5.	fork le daemon principal (create_main_demon)
	6.	dans le daemon :
	•	init_erraid_pipes()
	•	create_scheduler()
	•	boucle open(req_pipe, O_RDONLY) puis lecture d’opcodes et dispatch handlers

5.2 Création d’une tâche simple (tadmor -c ...)
	1.	tadmor écrit sur request pipe :
	•	OPCODE_CREATE (u16)
	•	timing (u64 minutes, u32 hours, u8 days)
	•	argc (u32)
	•	pour chaque arg : len (u32) + bytes
	2.	erraid :
	•	handle_create_task() désérialise timing + argv
	•	assigne un id (via g_next_task_id)
	•	ajoute à la liste mémoire (add_task_to_list)
	•	persiste sur disque (store_task)
	•	répond OK ou ER/NF

5.3 Listing (tadmor -l)
	1.	tadmor envoie OPCODE_LIST
	2.	erraid répond :
	•	OK
	•	count
	•	puis pour chaque tâche : id + timing + cmd serialisée (via stockage_argument)
	3.	tadmor formate les bitmasks en *, - ou plages a-b,c-d via format_timing_field(), et affiche la commande via read_command().

5.4 Exécution planifiée

À chaque minute (sec==0) :
	•	should_execute() vérifie les bits day/hour/minute
	•	si match :
	•	fork → redirection stdout/stderr → exec_command()
	•	append timestamp+exit dans times-exitcodes

⸻

6) Logging

Fichiers de logs :
	•	erraid.log
	•	scheduler.log
	•	tadmor.log (prévu/partiel)

Le système de logs repose sur :
	•	un LOG_FD global
	•	un LOG_CURRENT_CTX (ERRAiD, SCHEDULER, STDERR, TADMOR, NONE)
	•	macros LOG_INFO, LOG_WARN, LOG_ERR activées par LOG_LEVEL

En mode démon, stdin/out/err sont redirigés vers /dev/null.

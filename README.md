# Projet erraid-tadmor

## Présentation

erraid est un système de planification et d’exécution de tâches, inspiré des mécanismes de type cron, permettant :
• la création de tâches simples
• la combinaison de tâches (séquence, pipeline, conditionnelle)
• l’exécution planifiée selon un calendrier
• la consultation des sorties standard, erreurs et codes de retour

Le projet repose sur une architecture client / démon / scheduler, communiquant via des tubes nommés (FIFO).

## Architecture

Le projet repose sur trois parties : le client (tadmor), le démon (erraid) et son scheduler.
Le détail de l’architecture, du modèle de données (tâches, commandes) et du protocole de communication (pipes, opcodes) est décrit dans [ARCHITECTURE.md](ARCHITECTURE.md).

### Arborescence du projet

```
.
├── ARCHITECTURE.md
├── AUTHORS.md
├── include
│   ├── erraid.h
│   ├── exec.h
│   ├── logger.h
│   ├── protocol.h
│   ├── scheduler.h
│   ├── serialisation.h
│   ├── tadmor.h
│   ├── types.h
│   └── utils.h
├── Makefile
├── README.md
└── src
    ├── erraid.c
    ├── exec.c
    ├── logger.c
    ├── main.c
    ├── scheduler.c
    ├── serialisation.c
    ├── tadmor.c
    ├── types.c
    └── utils.c
```

## Options en ligne de commande

### tadmor (client)

    •	-c : crée une tâche simple
    •	-s : crée une tâche séquentielle à partir d’identifiants de tâches existantes
    •	-p : crée une tâche pipeline à partir d’identifiants de tâches existantes
    •	-i : crée une tâche conditionnelle (if/then/else) à partir d’identifiants de tâches existantes
    •	-l : liste les tâches existantes avec leur identifiant
    •	-o TASKID : affiche la sortie standard de la dernière exécution complète de la tâche
    •	-e TASKID : affiche la sortie d’erreur de la dernière exécution complète de la tâche
    •	-x TASKID : affiche l’historique daté des codes de retour des exécutions de la tâche
    •	-r TASKID : supprime la tâche d’identifiant donné
    •	-q : demande l’arrêt du démon
    •	-m MINUTES : planifie l’exécution aux minutes indiquées (à combiner avec -c/-s/-p/-i)
    •	-H HOURS : planifie l’exécution aux heures indiquées (à combiner avec -c/-s/-p/-i)
    •	-d DAYSOFWEEK : planifie l’exécution aux jours de la semaine indiqués (à combiner avec -c/-s/-p/-i)
    •	-n : crée une tâche sans horaire, destinée à être combinée avec d’autres tâches
    •	-P PIPES_DIR : indique le répertoire contenant les tubes de communication avec le démon
    •	-L : désactive la journalisation dans un fichier

À noter :
• une seule option d’action (-c, -s, -p, -i, -l, -r, -x, -o, -e, -q) doit être utilisée par appel : si plusieurs sont fournies, seule la dernière rencontrée est exécutée, sans message d’erreur
• -m, -H, -d et -n sont de simples modificateurs de planification : ils doivent obligatoirement être combinés avec -c, -s, -p ou -i, sinon la commande échoue avec « Action inconnue »
• -q arrête immédiatement le traitement des options dès qu’elle est rencontrée : elle doit être utilisée seule (et -P doit alors être placée avant elle si un répertoire de pipes spécifique est nécessaire)
• appeler tadmor sans aucune option affiche directement ce rappel des options disponibles

### erraid (démon)

    •	-F : s’exécute en avant-plan, sans démonisation
    •	-R RUN_DIR : indique le répertoire de stockage des tâches (par défaut /tmp/$USER/erraid)
    •	-P PIPES_DIR : indique le répertoire des tubes de communication avec le client
    •	-L : désactive la journalisation dans un fichier (les logs passent sur la sortie d’erreur)
    •	-S : désactive la journalisation de l’exécuteur de tâches (scheduler)

À noter : appeler erraid avec une option invalide affiche directement ce rappel des options disponibles.

## Exemples d’utilisation

### Créer une tâche simple

tadmor -c -m 10 echo "Hello"

### Créer une séquence

tadmor -s 1 2 3

### Créer une commande conditionnelle

tadmor -i 1 2 3

## Compilation

```sh
make
```

Cette commande compile le projet avec les réglages par défaut (équivalent à `make all`).

### Cibles disponibles

| Cible                 | Effet                                                                                            |
| --------------------- | ------------------------------------------------------------------------------------------------ |
| `all`                 | Compile sans logs (niveau `LOG_NONE`) et reconstruit `erraid` et `tadmor`                        |
| `info`                | Compile avec le niveau de log `LOG_INFO` (le plus verbeux)                                       |
| `warning`             | Compile avec le niveau de log `LOG_WARN`                                                         |
| `error`               | Compile avec le niveau de log `LOG_ERROR` (le moins verbeux)                                     |
| `erraid`              | Compile uniquement le binaire `erraid` (démon)                                                   |
| `tadmor`              | Compile uniquement le binaire `tadmor` (client)                                                  |
| `kill`                | Tue les processus `erraid` et `tadmor` en cours d'exécution                                      |
| `clean` / `distclean` | Tue les processus en cours puis supprime les fichiers compilés (`build/`, `erraid`, `tadmor`)    |
| `re`                  | Nettoie puis recompile tout le projet                                                            |
| `test`                | Compile le projet puis exécute une série de tests basiques sur les messages d'erreur de `tadmor` |

Pour utiliser une cible : `make <cible>` (ex. `make info`, `make clean`).

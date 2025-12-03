#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

// Codes de retour des WORKERS
#define WORKER_MSG_UNKNOWN    0
#define WORKER_MSG_PRIME      1  // Le nombre est premier
#define WORKER_MSG_NOT_PRIME -1  // Le nombre n'est pas premier

// Valeur spéciale pour dire aux WORKERS de s'arrêter
// (Puisque les nombres à tester sont >= 2, -1 est sûr)
#define PIPELINE_STOP_ORDER  -1

/**
 * Rapport envoyé d'un WORKER au MASTER (via tube)
 */
typedef struct {
    int number; // Nb testé
    int result; // WORKER_MSG_PRIME ou WORKER_MSG_NOT_PRIME
} WorkerReport;

#endif

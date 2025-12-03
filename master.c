
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

// ajout perso de biblio
#include <sys/ipc.h>
#include <sys/sem.h>

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master a besoin

typedef struct {
    int semId;              // identifiant du groupe de sémaphores
    int fdToWorker;         // fd d'écriture vers le premier worker
    int fdFromWorkers;      // fd de lecture des rapports des workers
    pid_t firstWorkerPid;   // PID du premier worker
    int maxPrime;           // plus grand nombre premier trouvé
    int count;              // nombre de nombres premiers trouvés
    int maxSent;            // plus grand nombre envoyé aux worker
} masterData;

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(masterData *data) {
    bool running = true;

    // boucle infinie :
    while(running) {
        // - ouverture des tubes (cf. rq client.c)
        int fdClientToMaster = open("client_to_master", O_RDONLY);
        myassert(fdClientToMaster != -1, "error: open for 'client_to_master' failed");
        int fdMasterToClient = open("master_to_client", O_WRONLY);
        myassert(fdMasterToClient != -1, "error: open for 'master_to_client' failed");

        // - attente d'un ordre du client (via le tube nommé)
        ClientRequest req;
        int ret = read(fdClientToMaster, &req, sizeof(ClientRequest));
        myassert(ret == sizeof(ClientRequest), "error: read for 'client_to_master' failed");

        if (ret == sizeof(ClientRequest)) {

            MasterResponse res;
            res.status = 0;

            switch (req.order) {
                // - si ORDER_STOP
                case ORDER_STOP:
                printf("MASTER : Reçu demande d'arrêt\n");
                {
                    int stop_order = PIPELINE_STOP_ORDER;
                    write(data->fdToWorker, &stop_order, sizeof(stop_order));
                    waitpid(data->firstWorkerPid, NULL, 0);
                    running = false;
                    res.data = 1; // Accusé de réception
                }
                    break;
                
                // - si ORDER_COMPUTE_PRIME_LOCAL
                case ORDER_COMPUTE_PRIME_LOCAL:
                //       . envoyer ordre de fin au premier worker et attendre sa fin
                //       . envoyer un accusé de réception au client
                    break;
                
                // - si ORDER_COMPUTE_PRIME
                case ORDER_COMPUTE_PRIME:
                printf("MASTER : Reçu demande de calcul pour %d\n", req.number);
                {
                    // On construit la pipeline jusqu'au nombre N-1 (si non encore fait) :
                    for (int n = data->maxSent + 1; n <= req.number; n++) {
                        write(data->fdToWorker, &n, sizeof(n));
                        
                        bool report_for_n_found = false;
                        while (!report_for_n_found) {
                            WorkerReport report;
                            int report_ret = read(data->fdFromWorkers, &report, sizeof(WorkerReport));
                            myassert(report_ret == sizeof(WorkerReport), "error: read from workers failed");

                            // Si un nouveau premier est trouvé, on met à jour les stats
                            if (report.result == WORKER_MSG_PRIME) {
                                data->count++;
                                if (report.number > data->maxPrime) {
                                    data->maxPrime = report.number;
                                }
                            }

                            // Si c'est le rapport pour le nombre 'n' qu'on vient d'envoyer
                            if (report.number == n) {
                                report_for_n_found = true;
                                // Si c'est le nombre final demandé par le client, on stocke la réponse
                                if (n == req.number) {
                                    res.data = (report.result == WORKER_MSG_PRIME) ? 1 : 0;
                                }
                            }
                        }
                    }

                    // Si le nombre demandé était déjà "connu" du pipeline
                    if (req.number <= data->maxSent) {
                        write(data->fdToWorker, &req.number, sizeof(req.number));
                        bool responseFound = false;
                        while (!responseFound) {
                            WorkerReport report;
                            int report_ret = read(data->fdFromWorkers, &report, sizeof(WorkerReport));
                            myassert(report_ret == sizeof(WorkerReport), "error: read from workers for known number failed");
                            if (report.number == req.number) {
                                responseFound = true;
                                res.data = (report.result == WORKER_MSG_PRIME) ? 1 : 0;
                            }
                        }
                    }

                    if (req.number > data->maxSent) {
                        data->maxSent = req.number;
                    }
                    break;
                }

                // - si ORDER_HOW_MANY_PRIME
                case ORDER_HOW_MANY_PRIME:
                    //       . transmettre la réponse au client (le plus simple est que cette
                    //         information soit stockée en local dans le master)
                    res.data = data->count;
                    break;
                
                
                // - si ORDER_HIGHEST_PRIME
                case ORDER_HIGHEST_PRIME:
                    //       . transmettre la réponse au client (le plus simple est que cette
                    //         information soit stockée en local dans le master)
                    res.data = data->maxPrime;
                    break;
                
                default:
                    res.status = -1;
            }
            
            write(fdMasterToClient, &res, sizeof(MasterResponse));
        }
            // - fermer les tubes nommés
            close(fdClientToMaster);
            close(fdMasterToClient);
            // - attendre ordre du client avant de continuer (sémaphore : précédence)
            // - revenir en début de boucle
            // il est important d'ouvrir et fermer les tubes nommés à chaque itération
            // voyez-vous pourquoi ?
            struct sembuf operation = {1, -1, 0};
            semop(data->semId, &operation, 1);
    }
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[]) {
    if (argc != 1) 
        usage(argv[0], NULL);
    
    masterData myData;
    myData.semId = -1;
    myData.maxPrime = 2; // Le premier worker est pour 2, donc c'est le plus grand connu
    myData.count = 1;    // On a déjà trouvé un nombre premier : 2
    myData.maxSent = 2;
    
    key_t key = ftok("master.c", 'S');
    myassert(key != -1, "error: ftok for 'key' failed");

    // - création des sémaphores
    myData.semId = semget(key, 2, 0644 | IPC_CREAT);
    myassert(myData.semId != -1, "error: semget for 'myData.semId' failed");

    // - initialisation des sémaphores
    int ret = semctl(myData.semId, 0, SETVAL, 1);
    myassert(ret != -1, "error: semctl for 'ret' (mutex) failed");
    ret = semctl(myData.semId, 1, SETVAL, 0);
    myassert(ret != -1, "error: semctl for 'sem' (sync) failed");

    mkfifo("client_to_master", 0644);
    mkfifo("master_to_client", 0644);

    // - création des tubes nommés
    
    // - création du tube pour envoyer des nombres aux workers
    int to_workers_pipe[2];
    ret = pipe(to_workers_pipe);
    myassert(ret == 0, "error: pipe for 'to_workers_pipe' failed");

    // - création du tube pour recevoir les rapports des workers
    int from_workers_pipe[2];
    ret = pipe(from_workers_pipe);
    myassert(ret == 0, "error: pipe for 'from_workers_pipe' failed");

    // - création du premier worker
    myData.firstWorkerPid = fork();
    myassert(myData.firstWorkerPid != -1, "error: fork for 'firstWorkerPid' failed");

    if (myData.firstWorkerPid == 0) { // Code de l'enfant (premier worker)
        // Fermeture des extrémités de tubes inutilisées par l'enfant
        close(to_workers_pipe[1]);
        close(from_workers_pipe[0]);

        // Préparation des arguments pour execl
        char strPrime[16];
        char strFdIn[16];
        char strFdToMaster[16];
        sprintf(strPrime, "%d", 2); // Le premier worker gère le nombre premier 2
        sprintf(strFdIn, "%d", to_workers_pipe[0]);
        sprintf(strFdToMaster, "%d", from_workers_pipe[1]);

        // Remplacement du processus enfant par le programme worker
        execl("./worker", "worker", strPrime, strFdIn, strFdToMaster, NULL);
        perror("error: execl for 'worker' failed"); // Ne doit jamais être atteint
        exit(EXIT_FAILURE);
    } else { // Code du parent (master)
        // Fermeture des extrémités de tubes inutilisées par le parent
        close(to_workers_pipe[0]);
        close(from_workers_pipe[1]);

        // Stockage des descripteurs de fichiers utiles
        myData.fdToWorker = to_workers_pipe[1];
        myData.fdFromWorkers = from_workers_pipe[0];
    }

    // boucle infinie
    loop(&myData);

    // destruction des tubes nommés, des sémaphores, ...*
    semctl(myData.semId, 0, IPC_RMID);
    unlink("client_to_master");
    unlink("master_to_client");
    close(myData.fdToWorker);
    close(myData.fdFromWorkers);

    return EXIT_SUCCESS; 
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien

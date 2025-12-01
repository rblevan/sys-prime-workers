
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

// on peut ici définir une structure stockant tout ce dont le master
// a besoin
typedef struct {
    int semId;       // identifiant du sémaphore de précédence
    int maxPrime;      // plus grand nombre premier trouvé
    int count;      // nombre de nombres premiers trouvés
    int maxSent;   // plus grand nombre envoyé aux workers
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

    mkfifo("client_to_master", 0644);
    mkfifo("master_to_client", 0644);

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
                    running = false;
                    res.data = -1;
                    break;
                
                // - si ORDER_COMPUTE_PRIME_LOCAL
                case ORDER_COMPUTE_PRIME_LOCAL:
                //       . envoyer ordre de fin au premier worker et attendre sa fin
                //       . envoyer un accusé de réception au client
                    break;
                
                // - si ORDER_COMPUTE_PRIME
                case ORDER_COMPUTE_PRIME:
                    printf("MASTER : Reçu demande de calcul pour %d\n", req.number);
                    res.data = 0;
                    break;    
                //       . récupérer le nombre N à tester provenant du client
                //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
                //             il faut connaître le plus grand nombre (M) déjà enovoyé aux workers
                //             on leur envoie tous les nombres entre M+1 et N-1
                //             note : chaque envoie déclenche une réponse des workers
                //       . envoyer N dans le pipeline
                //       . récupérer la réponse
                //       . la transmettre au client

                // - si ORDER_HOW_MANY_PRIME
                case ORDER_HOW_MANY_PRIME:
                    //       . transmettre la réponse au client (le plus simple est que cette
                    //         information soit stockée en local dans le master)
                    break;
                
                
                // - si ORDER_HIGHEST_PRIME
                case ORDER_HIGHEST_PRIME:
                    //       . transmettre la réponse au client (le plus simple est que cette
                    //         information soit stockée en local dans le master)
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
    myData.maxPrime = 0;
    myData.count = 0;
    myData.maxSent = 0;

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

    // - création des tubes nommés
    // - création du premier worker

    // boucle infinie
    loop(&myData);

    // destruction des tubes nommés, des sémaphores, ...*
    semctl(myData.semId, 0, IPC_RMID);

    return EXIT_SUCCESS; 
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

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
#include <string.h>

#include "myassert.h"

#include "master_client.h"

// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP      "stop"
#define TK_COMPUTE   "compute"
#define TK_HOW_MANY  "howmany"
#define TK_HIGHEST   "highest"
#define TK_LOCAL     "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <ordre> [<nombre>]\n", exeName);
    fprintf(stderr, "   ordre \"" TK_STOP  "\" : arrêt master\n");
    fprintf(stderr, "   ordre \"" TK_COMPUTE  "\" : calcul de nombre premier\n");
    fprintf(stderr, "                       <nombre> doit être fourni\n");
    fprintf(stderr, "   ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "   ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "   ordre \"" TK_LOCAL  "\" : calcul de nombres premiers en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static int parseArgs(int argc, char * argv[], int *number)
{
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0)
        order = ORDER_STOP;
    else if (strcmp(argv[1], TK_COMPUTE) == 0)
        order = ORDER_COMPUTE_PRIME;
    else if (strcmp(argv[1], TK_HOW_MANY) == 0)
        order = ORDER_HOW_MANY_PRIME;
    else if (strcmp(argv[1], TK_HIGHEST) == 0)
        order = ORDER_HIGHEST_PRIME;
    else if (strcmp(argv[1], TK_LOCAL) == 0)
        order = ORDER_COMPUTE_PRIME_LOCAL;
    
    if (order == ORDER_NONE)
        usage(argv[0], "ordre incorrect");
    if ((order == ORDER_STOP) && (argc != 2))
        usage(argv[0], TK_STOP" : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
        usage(argv[0], TK_COMPUTE " : il faut le second argument");
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
        usage(argv[0], TK_HOW_MANY" : il ne faut pas de second argument");
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
        usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
        usage(argv[0], TK_LOCAL " : il faut le second argument");
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL))
    {
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2)
             usage(argv[0], "le nombre doit être >= 2");
    }       
    
    return order;
}

/************************************************************************
 * Affichage de la réponse du master
 ************************************************************************/

static void displayResponse(int order, int number, const MasterResponse *res)
{
    if (res->status == 0) {
        switch (order) {
            case ORDER_STOP:
                printf("Master en cours d'arrêt.\n");
                break;
            case ORDER_COMPUTE_PRIME:
                printf("Le nombre %d est %s.\n", number, res->data == 1 ? "premier" : "non premier");
                break;
            case ORDER_HOW_MANY_PRIME:
                printf("Nombre de nombres premiers trouvés : %d.\n", res->data);
                break;
            case ORDER_HIGHEST_PRIME:
                printf("Plus grand nombre premier trouvé : %d.\n", res->data);
                break;
            default:
                // Ne devrait pas arriver si parseArgs fait bien son travail
                fprintf(stderr, "Réponse reçue pour une commande inconnue.\n");
        }
    } else {
        fprintf(stderr, "Le master a retourné une erreur.\n");
    }
}

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    int number = 0;
    int order = parseArgs(argc, argv, &number);

    // order peut valoir 5 valeurs (cf. master_client.h) :
    //      - ORDER_COMPUTE_PRIME_LOCAL
    //      - ORDER_STOP
    //      - ORDER_COMPUTE_PRIME
    //      - ORDER_HOW_MANY_PRIME
    //      - ORDER_HIGHEST_PRIME
    //
    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    if(order == ORDER_COMPUTE_PRIME_LOCAL) {
        //    alors c'est un code complètement à part multi-thread
        // TODO Implémenter avec Threads
        printf("Thread (pas encore fait)");
    } else {
        key_t key = ftok("master.c", 'S');
        myassert(key != -1, "error: ftok for 'key' failed");
        int semId = semget(key, 2, 0);
        myassert(semId != -1, "error: semget for 'semId' failed");
        struct sembuf enterMutex = {0, -1, 0};
        semop(semId, &enterMutex, 1);

        int fdClientToMaster = open("client_to_master", O_WRONLY);
        myassert(fdClientToMaster != -1, "error: open for 'client_to_master' failed");
        int fdMasterToClient = open("master_to_client", O_RDONLY);
        myassert(fdMasterToClient != -1, "error: open for 'master_to_client' failed");

        ClientRequest req;
        req.order = order;
        req.number = number;

        int ret = write(fdClientToMaster, &req, sizeof(ClientRequest));
        myassert(ret == sizeof(ClientRequest), "error: write for 'fdClientToMaster' failed");

        MasterResponse res;
        ret = read(fdMasterToClient, &res, sizeof(MasterResponse));
        myassert(ret == sizeof(MasterResponse), "error: read for 'fdMasterToClient' failed");

        // Affichage de la reponse du master
        displayResponse(order, number, &res);

        close(fdClientToMaster);
        close(fdMasterToClient);

        struct sembuf wakeMaster = {1, 1, 0};
        semop(semId, &wakeMaster, 1);

        struct sembuf leaveMutex = {0, 1, 0};
        semop(semId, &leaveMutex, 1);
    
    }


        
        // sinon
        //    - entrer en section critique :
        //           . pour empêcher que 2 clients communiquent simultanément
        //           . le mutex est déjà créé par le master
        //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
        //           . les ouvertures sont bloquantes, il faut s'assurer que
        //             le master ouvre les tubes dans le même ordre
        //    - envoyer l'ordre et les données éventuelles au master
        //    - attendre la réponse sur le second tube
        //    - sortir de la section critique
        //    - libérer les ressources (fermeture des tubes, ...)
        //    - débloquer le master grâce à un second sémaphore (cf. ci-dessous)
        // 
    
    
    // Une fois que le master a envoyé la réponse au client, il se bloque
    // sur un sémaphore ; le dernier point permet donc au master de continuer
    //
    // N'hésitez pas à faire des fonctions annexes ; si la fonction main
    // ne dépassait pas une trentaine de lignes, ce serait bien.
    
    return EXIT_SUCCESS;
}

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// ajout perso de biblio
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "myassert.h"

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

typedef struct {
    int myPrime; // nb premier géré par le WORKER
    int fdIn; // tube par lequel on est entré
    int fdToMaster; // tube par lequel on va au MASTER
    int fdOut; // tube par lequel on va sortir
    bool hasNext; // si il y a un WORKER suivant
    pid_t pidNext; // pid du WORKER suivant
} WorkerProps;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[], WorkerProps *props)
{
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    props->myPrime = atoi(argv[1]);
    props->fdIn = atoi(argv[2]);
    props->fdToMaster = atoi(argv[3]);
    props->fdOut = -1;
    props->hasNext = false;
    props->pidNext = -1;
}

void createNextWorker(WorkerProps *props, int n) {

    int tube[2];
    int ret = pipe(tube);
    myassert(ret == 0, "error: pipe for 'ret' failed");

    props->pidNext = fork();
    myassert(props->pidNext != -1, "error: fork for 'props->pidNext' failed");

    if (props->pidNext == 0) {
        // enfant
        close(tube[1]);

        char strN[16];
        char strFdIn[16];
        char strFdToMaster[16];

        sprintf(strN, "%d", n);
        sprintf(strFdIn, "%d", tube[0]);
        sprintf(strFdToMaster, "%d", props->fdToMaster);

        execl("./worker", "worker", strN, strFdIn, strFdToMaster, NULL);
        perror("error: execl for 'worker' failed");
        exit(EXIT_FAILURE);
    } else {
        // parent
        close(tube[0]);
        props->fdOut = tube[1];
        props->hasNext = true;
    }
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(/* paramètres */)
{
    // boucle infinie :
    //    attendre l'arrivée d'un nombre à tester
    //    si ordre d'arrêt
    //       si il y a un worker suivant, transmettre l'ordre et attendre sa fin
    //       sortir de la boucle
    //    sinon c'est un nombre à tester, 4 possibilités :
    //           - le nombre est premier
    //           - le nombre n'est pas premier
    //           - s'il y a un worker suivant lui transmettre le nombre
    //           - s'il n'y a pas de worker suivant, le créer
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
    parseArgs(argc, argv /*, structure à remplir*/);
    
    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier

    loop(/* paramètres */);

    // libérer les ressources : fermeture des files descriptors par exemple

    return EXIT_SUCCESS;
}

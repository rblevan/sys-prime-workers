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
    int myPrime;    // nb premier géré par le WORKER
    int fdIn;       // tube par lequel on est entré
    int fdToMaster; // tube par lequel on va au MASTER
    int fdOut;      // tube par lequel on va sortir
    bool hasNext;   // si il y a un WORKER suivant
    pid_t pidNext;  // pid du WORKER suivant
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

void loop(WorkerProps *props)
{
    int numberToTest;
    bool running = true;

    // boucle infinie :
    while(running){

        // attendre l'arrivée d'un nombre à tester
        int ret = read(props->fdIn, &numberToTest, sizeof(numberToTest));
        myassert(ret != -1, "error: read for 'props->fdIn' failed");
        
        // si ordre d'arrêt
        if ((ret == 0) || (numberToTest == PIPELINE_STOP_ORDER)) {
            running = false;
        
            // si il y a un worker suivant, transmettre l'ordre et attendre sa fin
            if (props->hasNext) {
                int stop_order = PIPELINE_STOP_ORDER;
                write(props->fdOut, &stop_order, sizeof(stop_order));
                close(props->fdOut); // fermeture du tube pour que le fils sorte de son read
                waitpid(props->pidNext, NULL, 0);
            }

            // sortir de la boucle
            break;
        }

        // sinon c'est un nombre à tester, 4 possibilités :
        // - le nombre n'est pas premier (divisible par myPrime)
        if (numberToTest % props->myPrime == 0) {
            WorkerReport report;
            report.number = numberToTest;
            report.result = WORKER_MSG_NOT_PRIME;
            ret = write(props->fdToMaster, &report, sizeof(WorkerReport));
            myassert(ret == sizeof(WorkerReport), "error: write for 'props->fdToMaster' failed in not primary return");
        }
        else {
            // Le nombre n'est pas divisible par myPrime, on le teste plus loin
            // - s'il y a un worker suivant lui transmettre le nombre
            if (props->hasNext){
                ret = write(props->fdOut, &numberToTest, sizeof(numberToTest));
                myassert(ret == sizeof(numberToTest), "error: write to next worker failed");
            }
            // - s'il n'y a pas de worker suivant, le créer
            else {
                // - le nombre est premier (car il n'a été divisé par aucun premier précédent)
                WorkerReport report;
                report.number = numberToTest;
                report.result = WORKER_MSG_PRIME;
                ret = write(props->fdToMaster, &report, sizeof(WorkerReport));
                myassert(ret == sizeof(WorkerReport), "error: write for 'props->fdToMaster' failed in primary return");

                // On crée le worker suivant pour ce nouveau nombre premier
                createNextWorker(props, numberToTest);
            }
        }
    }
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
    WorkerProps props;
    parseArgs(argc, argv, &props);

    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier
    // Note : C'est le worker parent qui envoie ce message au master juste avant de créer ce worker.

    loop(&props);

    // libérer les ressources : fermeture des files descriptors par exemple
    close(props.fdIn);

    return EXIT_SUCCESS;
}

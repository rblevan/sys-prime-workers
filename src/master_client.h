#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)

// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

// Structure de données

/**
 *  Message envoyé du CLIENT vers le MASTER
 */
typedef struct
{
    int order; // Pour les codes ORDER_
    int number; // Nb à tester (ignoré si COMPUTE)
} ClientRequest;

/**
 *  Message envoyé du MASTER vers le CLIENT
 */
typedef struct {
    int status; // si on a une erreur (on pose 0 = OK et -1 = erreur [optionnel])
    int data; // la réponse du master : 1 (vrai) ou 0 (faux)
              // compte pour HOW_MANY
              // max pour HIGHEST
              // 0 ou 1 pour accusé de STOP
} MasterResponse;

#endif

#ifndef _JOBS_H
#define _JOBS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "csapp.h"
#include "readcmd.h"



//macros pour les jobs
#define MAXJOBS 15
#define UNDEF -1
#define UNUSED 0
#define FG 1 //premier plan
#define BG 2 //arrière plan
#define STOPPED 3 
#define ZOMBIE 4
#define TERMINATED 5
#define Done 6
#define RUNNING 7

/*structure jobs*/
struct job_t{
	int numero_job;
	pid_t pid;
	int status;
	char *cmd;
	
};

/*structure tableau jobs*/
struct tab_jobs_t{
	struct job_t tab[MAXJOBS];
	int nb_jobs;//nb jobs actifs dans le tableau
};



//prototypes des fonctions
/*affiche le tableau des jobs*/
void afficher_tab_jobs(struct tab_jobs_t *tab_jobs);

/*touver l'index d'un job par son pid*/
int trouver_index(struct tab_jobs_t *tab_jobs, pid_t pid);

/*l'initialisation du tableau des jobs*/
void init_tab_jobs(struct tab_jobs_t *tab_jobs);

/*retourne l'indice de la premiere case libre du tableau des jobs*/
int premierecase(struct tab_jobs_t *tab_jobs);

/*retourne l'indice du job en premier plan*/
int job_premier_plan(struct tab_jobs_t *tab_jobs);

/*ajoute le job au tableau des jobs*/
void ajouter_job(struct tab_jobs_t *tab_jobs, int index, pid_t pid, char *cmd, int status);

/*supprime le job du tableau des jobs*/
void supprimer_jobs(struct tab_jobs_t *tab_jobs, int index);

/*nettoie les jobs terminés du tableau des jobs*/
void nettoyer_jobs(struct tab_jobs_t *tab_jobs);

/*affiche les jobs*/
void jobs(struct tab_jobs_t *tab_jobs);

/*met le job en premier plan*/
void fg(struct tab_jobs_t *tab_jobs, int numero);

/*met le job en arrière plan*/
void bg(struct tab_jobs_t *tab_jobs, int numero);


#endif


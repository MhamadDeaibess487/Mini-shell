/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#ifndef __READCMD_H
#define __READCMD_H

/* Read a command line from input stream. Return null when input closed.
Display an error and call exit() in case of memory exhaustion. */
struct cmdline *readcmd(void);

/* Structure returned by readcmd() */
struct cmdline {
	char *err;	/* If not null, it is an error message that should be
			   displayed. The other fields are null. */
	char *in;	/* If not null : name of file for input redirection. */
	char *out;	/* If not null : name of file for output redirection. */
	char ***seq;	/* See comment below */
	int background;//1 background job, 0 foreground job
};

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

/*initialization du tableau des jobs*/
void init_tab_jobs(struct tab_jobs_t *tab_jobs);

/*retourne la premiere case libre du tableau des jobs*/
int premierecase(struct tab_jobs_t *tab_jobs);

/*retourne l'index du premier job en premier plan*/
int job_premier_plan(struct tab_jobs_t *tab_jobs);

/*affiche les jobs*/
void jobs(struct tab_jobs_t *tab_jobs);

/*ajouter un job a l'indice = index du tab jobs*/
int ajouter_job(struct tab_jobs_t *tab_jobs,int index, pid_t pid, char *cmd, int status);

/*supprime un job a l'indice = index du tab jobs*/
void supprimer_jobs(struct tab_jobs_t	 *tab_jobs, int index);

int trouver_index(struct tab_jobs_t *tab_jobs, pid_t pid);
/* Field seq of struct cmdline :
A command line is a sequence of commands whose output is linked to the input
of the next command by a pipe. To describe such a structure :
A command is an array of strings (char **), whose last item is a null pointer.
A sequence is an array of commands (char ***), whose last item is a null
pointer.
When a struct cmdline is returned by readcmd(), seq[0] is never null.
*/
#endif

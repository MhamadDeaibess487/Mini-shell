/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include "csapp.h"
#include <string.h>
#include "jobs.h"
#include <wordexp.h>

struct tab_jobs_t tab_jobs;


void handler_signal(int sig){
	switch (sig) {
		case SIGINT: {
			int idx = job_premier_plan(&tab_jobs);
			if (idx >= 0){
				Kill(-tab_jobs.tab[idx].pid, SIGINT);

			}
			break;
		}
		case SIGCHLD: {
			int status;
			pid_t pid;

			while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
				int idx = trouver_index(&tab_jobs, pid);

				if (idx >= 0) {
					if (WIFEXITED(status) || WIFSIGNALED(status))
						supprimer_jobs(&tab_jobs, idx);
					else if (WIFSTOPPED(status))
						tab_jobs.tab[idx].status = STOPPED;
				}
			}
			break;
		}
		case SIGTSTP: {

			int idx = job_premier_plan(&tab_jobs);
			if (idx >= 0) {
				Kill(-tab_jobs.tab[idx].pid, SIGTSTP);
				tab_jobs.tab[idx].status = STOPPED;
			}
			break;
		}
		default:
			break;
	}
	
	
}



int main()
{
	Signal(SIGCHLD, handler_signal);//pour eviter les processus zombies
	Signal(SIGINT, handler_signal);//pour pouvoir sortir du shell avec le signal SIGINT (ctrl+c)
	Signal(SIGTSTP, handler_signal);//pour pouvoir stopper un processus avec le signal SIGTSTP (ctrl+z)

	init_tab_jobs(&tab_jobs);
	pid_t pid1;
	while (1) {
		struct cmdline *l;
		//int i, j;

		printf("shell> ");
		l = readcmd();
		

		/* If input stream closed, normal termination */
		if (!l) {
			printf("exit\n");
			exit(0);
		}
		if (l->seq == NULL || l->seq[0] == NULL){
    	continue;
		}


		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		
		/* Display each command of the pipe */
	
		
		/*for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
			
			for (j=0; cmd[j]!=0; j++) {
				printf("%s \n", cmd[j]);
				

			}
		}*/
			int prev_fd=-1;
			//on traite la sequence de commande
			for(int i=0;l->seq[i]!=NULL;i++){
				int fd[2];
				if (l->seq[i+1] != NULL) {
					if (pipe(fd) < 0) {
						perror("pipe");
						exit(1);
					}
				}
				//on verifie le quit dans la sequence de commande pour sortir du shell
				if(strcmp(l->seq[i][0], "quit") == 0){
					printf("exit\n");
					exit(0);
				}
				if(strcmp(l->seq[i][0], "jobs") == 0){
					//affichage des jobs
					jobs(&tab_jobs);
					continue;
				}
				if(strcmp(l->seq[i][0], "stop") == 0){
					if (l->seq[i][1] == NULL) {
						printf("stop: argument missing\n");
						continue;
					}

					char *arg = l->seq[i][1];

					if (arg[0] == '%') {
						int numero = atoi(arg + 1);
						int index = numero - 1;

						if (index < 0 || index >= MAXJOBS ||
							tab_jobs.tab[index].numero_job == 0) {
							printf("stop: no such job\n");
							continue;
						}

						Kill(-tab_jobs.tab[index].pid, SIGTSTP);
					}
					else {
						printf("stop: usage stop %%jobnumber\n");
					}
					continue;
				}
				if (strcmp(l->seq[i][0], "fg") == 0) {

					if (l->seq[i][1] == NULL) {
						printf("fg: argument missing\n");
						continue;
					}

					char *arg = l->seq[i][1];

					if (arg[0] == '%') {
						int numero = atoi(arg + 1);
						int index = numero - 1;

						if (index < 0 || index >= MAXJOBS ||
							tab_jobs.tab[index].numero_job == 0) {
							printf("fg: no such job\n");
							continue;
						}

						fg(&tab_jobs, index);
					}
					else {
						printf("fg: usage fg %%jobnumber\n");
					}

					continue;
				}
					
					
				if (strcmp(l->seq[i][0], "bg") == 0) {

					if (l->seq[i][1] == NULL) {
						printf("bg: argument missing\n");
						continue;
					}

					char *arg = l->seq[i][1];

					if (arg[0] == '%') {
						int numero = atoi(arg + 1);
						int index = numero - 1;

						if (index < 0 || index >= MAXJOBS ||
							tab_jobs.tab[index].numero_job == 0) {
							printf("bg: no such job\n");
							continue;
						}

						bg(&tab_jobs, index);
					}
					else {
						printf("bg: usage bg %%jobnumber\n");
					}

					continue;
				}
				if(strcmp(l->seq[i][0], "kill") == 0){
					int numero = atoi(l->seq[i][1]) - 1;
					if (numero ==1){
						printf("kill: job not permitted\n");
					};
				}
				if (strcmp(l->seq[i][0], "cd") == 0) {

					if (l->seq[i][1] == NULL) {
						/* cd sans argument → aller dans HOME */
						char *home = getenv("HOME");
						if (home != NULL)
							chdir(home);
					} else {
						if (chdir(l->seq[i][1]) < 0)
							perror("cd");
					}

					continue;
				}
				
					sigset_t mask, prev;
					Sigemptyset(&mask);
					Sigaddset(&mask, SIGCHLD);
					Sigprocmask(SIG_BLOCK, &mask, &prev);
					//on cree un processus fils pour chaque commande de la sequence
					 pid1 = Fork();
					
					if(pid1 == 0){
						setpgid(0, 0);
						Sigprocmask(SIG_SETMASK, &prev, NULL);
						//on traite les permission de fichier d'entrée
							if(l->in!=NULL){
							int fd0 = open(l->in, O_RDONLY, 0);
							if(fd0 < 0){
								fprintf(stderr, "%s: permission denied\n", l->in);
								exit(1);
								}
								Dup2(fd0, 0);
								Close(fd0);
								
							}
						//si c'est la premier commande de la sequence et qu'il y a une commande suivante
						if(prev_fd == -1 && l->seq[i+1] != NULL){
							
							//on ecrit dans fd[1] du tube pour la prochaine commande
							Dup2(fd[1],1);
							Close(fd[0]);
							Close(fd[1]);
						}
						//si c'est une commande intermédiaire de la sequence
						else if(prev_fd !=-1 && l->seq[i+1]!=NULL){
							//on lit dans prev_fd du tube de la commande précédente
							Dup2(prev_fd, 0);
							Close(prev_fd);
							//on ecrit dans fd[1] du tube pour la prochaine commande
							Dup2(fd[1], 1);
							Close(fd[0]);
							Close(fd[1]);
						}
						//si c'est la dernière commande de la sequence
						else if(l->seq[i+1] == NULL){
							//on lit dans prev_fd du tube de la commande précédente
							if(prev_fd != -1){
								dup2(prev_fd, 0);
								close(prev_fd);
							}
							//on traite les permission de fichier de sortie
							if(l->out != NULL){
								int fd1 = open(l->out, O_WRONLY|O_CREAT|O_TRUNC, 0644);
								if(fd1 < 0){
									perror(l->out);
									exit(1);
								}
								dup2(fd1, 1);
								close(fd1);
							}
						}

						wordexp_t p;
						char cmdline[1024] = "";

						/* reconstruire la commande complète */
						for (int j = 0; l->seq[i][j] != NULL; j++) {
							strcat(cmdline, l->seq[i][j]);
							strcat(cmdline, " ");
						}

						/* expansion */
						if (wordexp(cmdline, &p, WRDE_NOCMD) == 0) {
							execvp(p.we_wordv[0], p.we_wordv);
							wordfree(&p);
						} else {
							fprintf(stderr, "expansion error\n");
							exit(1);
						}

						fprintf(stderr, "%s: command not found\n", l->seq[i][0]);
						exit(1);
				}else{
					
					//dans le pere 
					if (l->seq[i+1] != NULL) {

						if (prev_fd != -1)
							close(prev_fd);

						close(fd[1]);      // on ferme l’écriture côté parent
						prev_fd = fd[0];   // on garde lecture pour prochaine commande
					}


					
					int status;
					if(l->background){
							status = BG;
						}else{
							status = FG;
						}
						
					//on ajoute le job dans le tableau des jobs
					int idx = premierecase(&tab_jobs);
					char buffer[1024] = "";
					for (int j = 0; l->seq[i][j] != NULL; j++) {
						strcat(buffer, l->seq[i][j]);
						strcat(buffer, " ");
					}
					if (idx >= 0) {
						ajouter_job(&tab_jobs, idx, pid1, buffer, status);
					}
					Sigprocmask(SIG_SETMASK, &prev, NULL);
				}
				
				

			}
			//pour bien gere les ordre dans le forground
			if (!l->background) {
				while (job_premier_plan(&tab_jobs) >= 0) {
					sleep(1);
				}
			}
			
	
	}
}

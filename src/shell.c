/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include "csapp.h"
#include <string.h>

struct tab_jobs_t tab_jobs;


void afficher_tab_jobs(struct tab_jobs_t *tab_jobs){
	printf("Tableau des jobs :\n");
	for (int i = 0; i < MAXJOBS; i++) {
		if (tab_jobs->tab[i].numero_job != 0) {
			printf("[%d]: PID=%d",tab_jobs->tab[i].numero_job,tab_jobs->tab[i].pid);
			switch (tab_jobs->tab[i].status) {
				case FG:
					printf(" (Foreground)");
					break;
				case BG:
					printf(" (Background)");
					break;
				case STOPPED:
					printf(" (Stopped)");
					break;
				case ZOMBIE:
					printf(" (Zombie)");
					break;
				case TERMINATED:
					printf(" (Terminated)");
					break;
				case UNUSED:
					printf(" (Unused)");
					break;
				case Done:
					printf(" (Done)");
					break;
				case RUNNING:
					printf(" (Running)");
					break;
				default:
					printf(" (Unknown status)");
					break;
			
				
		}
		printf(": %s\n", tab_jobs->tab[i].cmd);
		
	}
	
}
}

int trouver_index(struct tab_jobs_t *tab_jobs, pid_t pid){
	for(int i=0;i<MAXJOBS;i++){
		if(tab_jobs->tab[i].pid == pid){
			return i;
		}
	}
	return -1;
}

void init_tab_jobs(struct tab_jobs_t *tab_jobs){
	tab_jobs->nb_jobs = 0;
	for(int i=0;i<MAXJOBS;i++){
		tab_jobs->tab[i].pid = 0;
		tab_jobs->tab[i].numero_job = 0;
		tab_jobs->tab[i].status = UNUSED;
		tab_jobs->tab[i].cmd = NULL;	
	}
}


int premierecase(struct tab_jobs_t *tab_jobs){
	for(int i=0;i<MAXJOBS;i++){
		if(tab_jobs->tab[i].numero_job == 0 && tab_jobs->tab[i].status == UNUSED){
			return i;
		}
	}
	return -1;
}


int job_premier_plan(struct tab_jobs_t *tab_jobs){
	for(int i=0;i<MAXJOBS;i++){
		if(tab_jobs->tab[i].status == FG){
			return i;
		}
	}
	return -1;
}




void jobs(struct tab_jobs_t *tab_jobs){
	afficher_tab_jobs(tab_jobs);
}

int ajouter_job(struct tab_jobs_t *tab_jobs,int index, pid_t pid, char *cmd, int status){
		
	tab_jobs->tab[index].pid = pid;
	tab_jobs->tab[index].numero_job = index + 1;
	tab_jobs->tab[index].status = status;
	tab_jobs->tab[index].cmd = malloc(strlen(cmd) + 1);
	strcpy(tab_jobs->tab[index].cmd, cmd);
	tab_jobs->nb_jobs++;
	return index;

}
	
void supprimer_jobs(struct tab_jobs_t *tab_jobs, int index){
	tab_jobs->tab[index].pid = 0;
	tab_jobs->tab[index].numero_job = 0;
	tab_jobs->tab[index].status = UNUSED;
	free(tab_jobs->tab[index].cmd);
	tab_jobs->tab[index].cmd = NULL;
	tab_jobs->nb_jobs--;
}

void fg(struct tab_jobs_t *tab_jobs, int index){
	tab_jobs->tab[index].status = FG;
	kill(tab_jobs->tab[index].pid, SIGCONT);
}

void bg(struct tab_jobs_t *tab_jobs, int index){
	tab_jobs->tab[index].status = BG;
	kill(tab_jobs->tab[index].pid, SIGCONT);
	

}

void handler_signal(int sig){
	switch (sig) {
		case SIGINT: {
			int idx = job_premier_plan(&tab_jobs);
			if (idx >= 0){
				kill(tab_jobs.tab[idx].pid, SIGINT);
				tab_jobs.tab[idx].status = TERMINATED;
			}
			break;
		}
		case SIGCHLD:
			int status;
			pid_t pid;
			while ((pid=waitpid(-1, &status, WNOHANG)) > 0){
				//indice = trouver pid dans le tableau des jobs
				int index_job = trouver_index(&tab_jobs, pid);
				if(index_job >= 0){
				supprimer_jobs(&tab_jobs, index_job);
				}
			}
			break;
		case SIGTSTP: {
			int idx = job_premier_plan(&tab_jobs);
			if (idx >= 0) {
				kill(tab_jobs.tab[idx].pid, SIGTSTP);
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
	int stat=0;
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
				if(strcmp(l->seq[i][0], "fg") == 0){
					//mettre le job en premier plan
					int numero = atoi(l->seq[i][1]) - 1;
					fg(&tab_jobs, numero);
					l->background = 0;
					continue;
				}
				if(strcmp(l->seq[i][0], "bg") == 0){
					//mettre le job en arrière plan
					int numero = atoi(l->seq[i][1]) - 1;
					if(!numero){
						numero = job_premier_plan(&tab_jobs);
					}

					bg(&tab_jobs,numero); 
					l->background = 1; 
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
					//on cree un processus fils pour chaque commande de la sequence
					pid_t pid1 = Fork();
					if(pid1 == 0){
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

						//on execute la commande
						execvp(l->seq[i][0], l->seq[i]);
						if(l->background){
							stat = BG;
						}else{
							stat = FG;
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


					sigset_t mask, prev;
					Sigemptyset(&mask);
					Sigaddset(&mask, SIGCHLD);
					Sigprocmask(SIG_BLOCK, &mask, &prev);

					//on ajoute le job dans le tableau des jobs
					ajouter_job(&tab_jobs, premierecase(&tab_jobs), pid1, l->seq[i][0], stat);

					Sigprocmask(SIG_SETMASK, &prev, NULL);
				}
				
				

			}
			//pour bien gere les ordre dans le forground
			if (!l->background) {
				int status;
				for (int k = 0; l->seq[k] != NULL; k++)
					wait(&status);
			}
			
			
	
	}
}

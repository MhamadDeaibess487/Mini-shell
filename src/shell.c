/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include "csapp.h"
#include <string.h>


void handler_signal(int sig){
	switch (sig) {
		case SIGINT:
			printf("caught sigint, exiting\n");
			exit(0);
			break;
		case SIGCHLD:
			int status;
			while (waitpid(-1, &status, WNOHANG) > 0);
			break;
		case SIGTSTP:
			printf("caught sigstop, exiting\n");
			exit(0);
			break;
		default:
			break;
	}
	
	
}

int main()
{
	Signal(SIGCHLD, handler_signal);//pour eviter les processus zombies
	Signal(SIGINT, handler_signal);//pour pouvoir sortir du shell avec le signal SIGINT (ctrl+c)
	Signal(SIGTSTP, handler_signal);//pour pouvoir sortir du shell avec le signal SIGSTOP (ctrl+z)
	while (1) {
		struct cmdline *l;
		//int i, j;

		//printf("shell> ");
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
					kill(getpid(), SIGINT);
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

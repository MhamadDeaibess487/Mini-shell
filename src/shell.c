/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include "csapp.h"
#include <string.h>


int main()
{
	while (1) {
		struct cmdline *l;
		int i, j;

		printf("shell> ");
		l = readcmd();

		/* If input stream closed, normal termination */
		if (!l) {
			printf("exit\n");
			exit(0);
		}

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		
		/* Display each command of the pipe */
	
		
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
			
			for (j=0; cmd[j]!=0; j++) {
				printf("%s \n", cmd[j]);
				

			}
		}
	
		
		if(strcmp(l->seq[0][0], "quit") == 0){
			printf("caught sigint, exiting\n");
			exit(0);
		}
		else{
			pid_t pid1 = Fork();
			
			if(pid1 == 0){
				
				if(l->in!=NULL){
					int fd0 = open(l->in, O_RDONLY, 0);
					if(fd0 < 0){
						fprintf(stderr, "%s: permission denied\n", l->in);
						exit(0);
					}
					Dup2(fd0, 0);
					Close(fd0);
					
				}
				if(l->out!=NULL){
					int fd1 = open(l->out, O_WRONLY|O_CREAT|O_TRUNC, DEF_MODE);
					if(fd1 < 0){
						fprintf(stderr, "%s: permission denied\n", l->out);
						exit(0);
					}
					Dup2(fd1, 1);
					Close(fd1);
				

				}
				
				
				execvp(l->seq[0][0], l->seq[0]);
				fprintf(stderr, "%s: command not found\n", l->seq[0][0]);
				exit(0);
			}
			else{
				wait(NULL);
			}
			
		}
	

		
	}
}

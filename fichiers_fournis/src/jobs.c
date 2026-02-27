#include "jobs.h"

void afficher_tab_jobs(struct tab_jobs_t *tab_jobs){
	
	for (int i = 0; i < MAXJOBS; i++) {
		if (tab_jobs->tab[i].numero_job != 0) {
			printf("[%d]: %d",tab_jobs->tab[i].numero_job,tab_jobs->tab[i].pid);
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
		printf(" %s\n", tab_jobs->tab[i].cmd);
		
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
	nettoyer_jobs(tab_jobs);
	afficher_tab_jobs(tab_jobs);
}

void ajouter_job(struct tab_jobs_t *tab_jobs,int index, pid_t pid, char *cmd, int status){
		
	tab_jobs->tab[index].pid = pid;
	tab_jobs->tab[index].numero_job = index + 1;
	tab_jobs->tab[index].status = status;
	tab_jobs->tab[index].cmd = malloc(strlen(cmd) + 1);
	tab_jobs->tab[index].cmd = strdup(cmd); 
	tab_jobs->nb_jobs++;
	

}
	
void supprimer_jobs(struct tab_jobs_t *tab_jobs, int index){

    tab_jobs->tab[index].pid = 0;
    tab_jobs->tab[index].numero_job = 0;
    tab_jobs->tab[index].status = UNUSED;
    if (tab_jobs->tab[index].cmd != NULL) {
        free(tab_jobs->tab[index].cmd);
        tab_jobs->tab[index].cmd = NULL;
    }
    tab_jobs->nb_jobs--;
}

void nettoyer_jobs(struct tab_jobs_t *tab_jobs) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int idx = trouver_index(tab_jobs, pid);
        if (idx >= 0)
            supprimer_jobs(tab_jobs, idx);
    }
}

void fg(struct tab_jobs_t *tab_jobs, int index){
	tab_jobs->tab[index].status = FG;
	Kill(-tab_jobs->tab[index].pid, SIGCONT);
}

void bg(struct tab_jobs_t *tab_jobs, int index){
	tab_jobs->tab[index].status = BG;
	Kill(-tab_jobs->tab[index].pid, SIGCONT);
}
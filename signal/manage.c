/*Author: KAI WANG*/
#include"head.h"

void handler(int signo);

seg_t* seg;
int shmid, msqid;


int main(){
	int i, found, tested, exist;	//tested to record the total amount of tested number of terminated computing processes

	struct shmid_ds ds;
	msg_t snd, rcv;
	sigset_t mask;
	struct sigaction action;
	
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGQUIT);
	action.sa_mask= mask;
	action.sa_flags= 0;
	action.sa_handler= handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);

	found= 0;
	tested= 0;
	
	bit[0]=1;
	for(i=1; i<8; i++){
		bit[i]= bit[i-1]<<1;
	}

	printf("**********************************\nmanage process  %d begins:\n", getpid());
	// create and attach shared memory
	if((shmid= shmget(KEY, sizeof(seg_t), IPC_CREAT|0666))==-1){
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	printf("shmid: %d size: %ld\n", shmid, sizeof(seg_t));
	if((seg= (seg_t*)shmat(shmid, 0, 0))==(seg_t*)-1){
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	shmctl(shmid, IPC_STAT, &ds);	
	printf("nattch: %d\n", (int)ds.shm_nattch);
	memset(seg, 0, sizeof(seg_t));
	
	// get message queue, create if not exists
	if((msqid= msgget(KEY, IPC_CREAT|0666))== -1){
		perror("msgget");
		exit(EXIT_FAILURE);
	}
	printf("message queue %d created\n*******************************\n", msqid);

	while(1){		
		if(msgrcv(msqid, &rcv, sizeof(msg_content), MANAGE_PORT, 0)==-1){
			perror("msgrcv");	
			exit(EXIT_FAILURE);
		}

		if(rcv.mtext.type== getent){
			printf("get a msg for entry number\n");
		 
			for(i=0; i<NUM_PROC; i++){
				if(seg->proc[i].pid== 0){		
					seg->proc[i].pid= snd.mtarget= rcv.mtext.sender;
					snd.mtext.type= getent;
					snd.mtext.data.entry= i;
					snd.mtext.sender= getpid();
					printf("%dth entry pid: %d\n", i, seg->proc[i].pid);
					if(msgsnd(msqid, &snd, sizeof(msg_content), 0)==-1){
						perror("msgsnd");
						exit(1);
					}
					
					break;
				}
			}
			if(i== NUM_PROC){
				perror("number of computing processes more than limit");
				exit(EXIT_FAILURE);
			}
			printf("----------------------------------\n");
		}else if(rcv.mtext.type== pfnum){
			//manage update perfect numbers
			exist= 0;
			for(i=0; i< found; i++){
				if(seg->pf[i]==rcv.mtext.data.num){
					exist= 1;
					break;
				}
			}
			if(exist==0){
				seg->pf[found++]= rcv.mtext.data.num;
			}
			printf("get a msg for perfect number: %d\n", rcv.mtext.data.num);
			printf("total found %d\n----------------------------------\n", found);
		}else if(rcv.mtext.type== mng_pid ){
		 	printf("get a msg for manage pid\n");
			snd.mtarget= rcv.mtext.sender;
			snd.mtext.type= mng_pid;
			snd.mtext.data.pid= getpid();
			snd.mtext.sender= getpid();
			if(msgsnd(msqid, &snd, sizeof(msg_content), 0)==-1){
				perror("msgsnd");
				exit(1);
			}
			printf("----------------------------------\n");
		}else if (rcv.mtext.type== end){
			/* replace these three lines with following commented if requiring manage to clear pid
			printf("get a msg for termination from compute process %d that tested %d numbers\n", rcv.mtext.sender, seg->proc[rcv.mtext.data.entry].tested);
			tested+= seg->proc[rcv.mtext.data.entry].tested; 
			memset(&seg->proc[rcv.mtext.data.entry], 0, sizeof(proc_t));*/
			
			printf("get a msg for termination from compute process %d that tested %d numbers\n", rcv.mtext.sender, rcv.mtext.data.num);
			tested+= rcv.mtext.data.num;
			for(i=0; i<NUM_PROC;i++){
				if(rcv.mtext.sender==seg->proc[i].pid){
					seg->proc[i].pid= 0;
					break;
				}
			}
			/**/
			printf("----------------------------------\n");
		}else if(rcv.mtext.type== report_test){
			printf("get a msg for reporting tested number\n");
			snd.mtarget= rcv.mtext.sender;
			snd.mtext.type= report_test;
			snd.mtext.sender= getpid();
			int t=0;
			for(i=0; i<NUM_PROC; i++){
				if(seg->proc[i].pid!=0){
					t+= seg->proc[i].tested;
				}
			}
			snd.mtext.data.num= tested + t;
			printf("tested:%d\n",snd.mtext.data.num);
			if(msgsnd(msqid, &snd, sizeof(msg_content), 0)==-1){
				perror("msgsnd");
				exit(1);
			}
			printf("----------------------------------\n");
		}else{
			fprintf(stderr, "unexpected message type\n");	
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

void handler(int signo){
	int i;
	printf("\nenter manage handler\n");
	print_sig(signo);
	// loop and find active compute processes, no need a queue to keep track
	for(i=0; i< NUM_PROC; i++){
		if(seg->proc[i].pid !=(pid_t)0){
			kill(seg->proc[i].pid, SIGINT);
			printf("\n send signal to compute process: %d\n", seg->proc[i].pid);
		}
	}
	sleep(5);
	shmdt((void*)seg);

	struct shmid_ds ds;
	shmctl(shmid, IPC_STAT, &ds);	
	printf("nattch before calling rm: %ld\n", ds.shm_nattch);

	if(shmctl(shmid, IPC_RMID, NULL)== -1){
		perror("shmctl rm");
		exit(EXIT_FAILURE);
	}
	if(msgctl(msqid, IPC_RMID, NULL)== -1){
		perror("msgctl rm");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}


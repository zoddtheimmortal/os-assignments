#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <sys/msg.h>
#include <limits.h>
#include <pthread.h>

#define sign(x) ((x>=0)?1:-1)
#define min(a,b) ((a<b)?a:b)
#define max(a,b) ((a>b)?a:b)

#define PERMS 0644
#define MAX_N 105
#define MAX_NEW_REQUESTS 30
#define ELEVATOR_MAX_CAP 20
#define PASSENGER_MAX 1000
#define MAX_LEN 100
#define QMAX 3005
#define THRESHOLD 5
#define ASSIGN_MAX 3
#define MIN_AWAY 10
#define FIX_CNT M
#define ELV_THR 10

#define UP 1
#define DOWN -1

int N,K,M,T;
key_t SHM_KEY,MAINQ_KEY;
key_t QKEYS[MAX_N];
int TOTAL_REQS=0;

typedef struct PassengerRequest{
    int requestId;
    int startFloor;
    int requestedFloor;
} PassengerRequest;

typedef struct MainSharedMemory{
    char authStrings[MAX_LEN][ELEVATOR_MAX_CAP+1];
    char elevatorMovementInstructions[MAX_LEN];
    PassengerRequest newPassengerRequests[MAX_NEW_REQUESTS];
    int elevatorFloors[MAX_LEN];
    int droppedPassengers[PASSENGER_MAX];
    int pickedUpPassengers[PASSENGER_MAX][2];
} MainSharedMemory;

typedef struct SolverRequest{
    long mtype;
    int elevatorNumber;
    char authStringGuess[ELEVATOR_MAX_CAP+1];
} SolverRequest;

typedef struct SolverResponse{
    long mtype;
    int guessIsCorrect;
} SolverResponse;

typedef struct TurnChangeResponse{
    long mtype;
    int turnNumber;
    int newPassengerRequestCount;
    int errorOccurred;
    int finished;
} TurnChangeResponse;

typedef struct TurnChangeRequest{
    long mtype;
    int droppedPassengersCount;
    int pickedUpPassengersCount;
} TurnChangeRequest;

typedef struct Elevator{
    int idx;
    int curr_floor;
    int num_passengers;
    int direction;
    int len;
    char prevmove;
    int set;
} Elevator;

typedef struct t_data{
    int idx;
    int si;
    int ei;
    int eno;
    int solver_id;
    char authstr[ELEVATOR_MAX_CAP+1];
} t_data;

typedef struct Request{
    PassengerRequest preq;
    int assigned_to;
    int picked;
    int dropped;
} Request;

Elevator elv[MAX_N];
int solverqid[MAX_N][2];
Request* req;

MainSharedMemory* mainshm;

int req_size=0;
int THREAD_CNT=0;
int gthread=-1,guessed=0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t solver_locks[MAX_N];

void* runner(void* args){
    t_data* data=(t_data*)args;

    SolverRequest sr_send;
    SolverResponse sr_rcv;

    int tg=0,len=elv[data->eno].len;
    int si=data->si,ei=data->ei,i=data->eno;

    int free_solver=data->solver_id;
    solverqid[free_solver][1]=0;

    for(int c=si;c<ei;c++){
        pthread_mutex_lock(&lock);
        if(guessed){
            pthread_mutex_unlock(&lock);
            break;
        }
        pthread_mutex_unlock(&lock);

        char* authstr=(char*)malloc(sizeof(char)*(len+1));
        int p=c;
        for(int j=0;j<len;j++){
            authstr[j]='a'+(p%6);
            p/=6;
        }
        authstr[len]='\0';

        sprintf(data->authstr,"%s",authstr);
        
        sr_send.mtype=2;
        sr_send.elevatorNumber=i;

        pthread_mutex_lock(&solver_locks[free_solver]);
        if(msgsnd(solverqid[free_solver][0],&sr_send,sizeof(sr_send)-sizeof(sr_send.mtype),0)==-1){
            perror("set elvnum: solverqid msgsnd: check runner");

            pthread_mutex_unlock(&solver_locks[free_solver]);
            exit(1);
        }
        
        sr_send.mtype=3;
        sprintf(sr_send.authStringGuess,"%s",authstr);

        if(msgsnd(solverqid[free_solver][0],&sr_send,sizeof(sr_send)-sizeof(sr_send.mtype),0)==-1){
            perror("set authstr: solverqid msgsnd: check runner");

            pthread_mutex_unlock(&solver_locks[free_solver]);
            exit(1);
        }

        if(msgrcv(solverqid[free_solver][0],&sr_rcv,sizeof(sr_rcv)-sizeof(sr_rcv.mtype),4,0)==-1){
            perror("solverqid msgrcv: check runner");

            pthread_mutex_unlock(&solver_locks[free_solver]);
            exit(1);
        }
        pthread_mutex_unlock(&solver_locks[free_solver]);
        
        if(sr_rcv.guessIsCorrect){
            pthread_mutex_lock(&lock);
            guessed=1;
            gthread=data->idx;
            pthread_mutex_unlock(&lock);
            sprintf(data->authstr,"%s",authstr);
            free(authstr);
            break;
        }
        free(authstr);
    }

    solverqid[free_solver][1]=1;
    pthread_exit(NULL);
}


void read_input(){
    FILE* file=fopen("input.txt","r");

    fscanf(file,"%d",&N);
    fscanf(file,"%d",&K);
    fscanf(file,"%d",&M);
    fscanf(file,"%d",&T);
    fscanf(file,"%d",&SHM_KEY);
    fscanf(file,"%d",&MAINQ_KEY);

    for(int i=0;i<M;i++){
        fscanf(file,"%d",&QKEYS[i]);
    }

    fclose(file);
}

int main(int argc, char **argv){
    read_input();

    printf("N: %d K: %d M: %d T: %d\n",N,K,M,T);
    printf("SHM_KEY: %d MAINQ_KEY: %d\n",SHM_KEY,MAINQ_KEY);

    THREAD_CNT=FIX_CNT;
    TOTAL_REQS=30*T+5;

    req=(Request*)malloc(sizeof(Request)*TOTAL_REQS);

    for(int i=0;i<M;i++){
        pthread_mutex_init(&solver_locks[i],NULL);
    }

    int mainqid;
    if((mainqid=msgget(MAINQ_KEY,PERMS|IPC_CREAT))==-1){
        perror("mainqid msgget: check main");
        exit(1);
    }

    int mainshmid;

    if((mainshmid=shmget(SHM_KEY,sizeof(MainSharedMemory),PERMS|IPC_CREAT))==-1){
        perror("mainshmid shmget: check main");
        exit(1);
    }

    if((mainshm=shmat(mainshmid,NULL,0))==(void*)-1){
        perror("mainshm shmat: check main");
        exit(1);
    }

    for(int i=0;i<M;i++){
        if((solverqid[i][0]=msgget(QKEYS[i],PERMS|IPC_CREAT))==-1){
            perror("solverqid msgget: check main");
            exit(1);
        }
        solverqid[i][1]=1;
    }

    TurnChangeResponse tcr_get;

    if(msgrcv(mainqid,&tcr_get,sizeof(tcr_get)-sizeof(tcr_get.mtype),2,0)==-1){
        perror("mainqid msgrcv: check main");
        exit(1);
    }

    for(int i=0;i<N;i++){
        elv[i].idx=i;
        elv[i].curr_floor=0;
        elv[i].num_passengers=0;
        elv[i].direction=1;
        elv[i].len=0;
        elv[i].prevmove='s';
        elv[i].set=0;
    }

    for(int i=0;i<TOTAL_REQS;i++){
        req[i].assigned_to=-1;
        req[i].picked=0;
        req[i].dropped=0;
    }

    int t=0;

    while(!tcr_get.finished&&!tcr_get.errorOccurred){
        PassengerRequest* preqs=mainshm->newPassengerRequests;

        for(int i=0;i<N;i++){
            elv[i].set=0;
            elv[i].curr_floor=mainshm->elevatorFloors[i];
        }

        for(int i=0;i<tcr_get.newPassengerRequestCount;i++){
            req[req_size].preq=preqs[i];
            req_size=(1+req_size)%TOTAL_REQS;
        }

        // assign reqs
        if(N>=ELV_THR){
            for(int p=0;p<req_size;p++){
                if(req[p].preq.requestId==INT_MIN) continue;
                if(req[p].picked) continue;
                if(req[p].dropped) continue;

                if(req[p].assigned_to!=-1){
                    int dist_away=abs(elv[req[p].assigned_to].curr_floor-req[p].preq.startFloor);
                    if(dist_away<=MIN_AWAY) continue;
                }

                int min_idx=-1,min_dist=INT_MAX;
                int dir=sign(req[p].preq.requestedFloor-req[p].preq.startFloor);

                for(int i=0;i<N;i++){
                    if(elv[i].direction!=dir) continue;
                    if(elv[i].num_passengers>=THRESHOLD) continue;
                    int curr_req=0;
                    for(int p=0;p<req_size;p++){
                        if(req[p].preq.requestId==INT_MIN) continue;
                        if(req[p].assigned_to!=i) continue;
                        if(req[p].picked) continue;
                        if(req[p].dropped) continue;

                        curr_req++;
                    }

                    for(int p=0;p<req_size;p++){
                        if(req[p].preq.requestId==INT_MIN) continue;
                        if(req[p].assigned_to!=i) continue;
                        if(!req[p].picked) continue;
                        if(req[p].dropped) continue;

                        curr_req++;
                    }

                    if(curr_req>=THRESHOLD+ASSIGN_MAX) continue;

                    int dist=abs(elv[i].curr_floor-req[p].preq.startFloor);
                    if(dist<min_dist){
                        min_dist=dist; min_idx=i;
                    }
                    if(dist==min_dist){
                        if(elv[i].num_passengers<elv[min_idx].num_passengers) min_idx=i;
                        else if(elv[i].prevmove=='s') min_idx=i;
                    }
                }

                if(min_idx==-1) continue;

                req[p].assigned_to=min_idx;
            }
        }
        else{
            for(int p=0;p<req_size;p++){
                if(req[p].preq.requestId==INT_MIN) continue;
                if(req[p].picked) continue;
                if(req[p].dropped) continue;

                int min_idx=-1,min_dist=INT_MAX;
                int dir=sign(req[p].preq.requestedFloor-req[p].preq.startFloor);

                for(int i=0;i<N;i++){
                    if(elv[i].direction!=dir) continue;
                    if(elv[i].num_passengers>=THRESHOLD) continue;

                    int dist=abs(elv[i].curr_floor-req[p].preq.startFloor);
                    if(dist<min_dist){
                        min_dist=dist; min_idx=i;
                    }
                    if(dist==min_dist){
                        if(elv[i].num_passengers<elv[min_idx].num_passengers) min_idx=i;
                        else if(elv[i].prevmove=='s') min_idx=i;
                    }
                }

                if(min_idx==-1) continue;

                int curr_req=0;
                for(int p=0;p<req_size;p++){
                    if(req[p].preq.requestId==INT_MIN) continue;
                    if(req[p].assigned_to!=min_idx) continue;
                    if(req[p].picked) continue;
                    if(req[p].dropped) continue;

                    curr_req++;
                }

                for(int p=0;p<req_size;p++){
                    if(req[p].preq.requestId==INT_MIN) continue;
                    if(req[p].assigned_to!=min_idx) continue;
                    if(!req[p].picked) continue;
                    if(req[p].dropped) continue;

                    curr_req++;
                }

                if(curr_req>=THRESHOLD+ASSIGN_MAX) continue;

                req[p].assigned_to=min_idx;
            }
        }
        int dropped=0,picked=0;

        //handle drops
        for(int i=0;i<N;i++){ 
            for(int p=0;p<req_size;p++){
                if(req[p].preq.requestId==INT_MIN) continue;
                if(req[p].assigned_to!=i) continue;
                if(req[p].dropped) continue;
                if(!req[p].picked) continue;

                if(elv[i].curr_floor==req[p].preq.requestedFloor){
                    mainshm->droppedPassengers[dropped]=req[p].preq.requestId;
                    elv[i].num_passengers--;
                    req[p].dropped=1;
                    req[p].preq.requestId=INT_MIN;
                    dropped++;
                }
            }
        }

        //handle pickup
        for(int i=0;i<N;i++){
            for(int p=0;p<req_size;p++){
                if(req[p].preq.requestId==INT_MIN) continue;
                if(req[p].assigned_to!=i) continue;
                if(req[p].picked) continue;
                if(req[p].dropped) continue;

                if(elv[i].curr_floor==req[p].preq.startFloor){
                    // if(elv[i].num_passengers>=THRESHOLD) continue;
                    mainshm->pickedUpPassengers[picked][0]=req[p].preq.requestId;
                    mainshm->pickedUpPassengers[picked][1]=i;

                    req[p].picked=1;
                    elv[i].num_passengers++;
                    picked++;
                }
            }
        }

        //drop if above THRESHOLD
        for(int i=0;i<N;i++){
            if(elv[i].num_passengers<=THRESHOLD) continue;

            while(elv[i].num_passengers>THRESHOLD){
                int furthest=-1,max_dist=INT_MIN;
                for(int p=0;p<req_size;p++){
                    if(req[p].preq.requestId==INT_MIN) continue;
                    if(req[p].assigned_to!=i) continue;
                    if(req[p].dropped) continue;
                    if(!req[p].picked) continue;
                    int dist=abs(elv[i].curr_floor-req[p].preq.startFloor);
                    if(dist>max_dist){
                        max_dist=dist;
                        furthest=p;
                    }
                }

                if(furthest==-1) break;

                mainshm->droppedPassengers[dropped]=req[furthest].preq.requestId;
                elv[i].num_passengers--;

                req[furthest].preq.startFloor=elv[i].curr_floor;
                req[furthest].assigned_to=-1;
                req[furthest].dropped=0;
                req[furthest].picked=0;

                dropped++;
            }
        }

        // set directions
        for(int i=0;i<N;i++){
            if(elv[i].curr_floor==0) elv[i].direction=UP;
            else if(elv[i].curr_floor==K-1) elv[i].direction=DOWN;
            else{
                if(elv[i].direction==UP){
                    int max_floor=INT_MIN;
                    for(int p=0;p<req_size;p++){
                        if(req[p].preq.requestId==INT_MIN) continue;
                        if(req[p].assigned_to!=i) continue;
                        if(req[p].picked) continue;
                        max_floor=max(max_floor,req[p].preq.startFloor);
                    }

                    for(int p=0;p<req_size;p++){
                        if(req[p].preq.requestId==INT_MIN) continue;
                        if(req[p].assigned_to!=i) continue;
                        if(!req[p].picked) continue;
                        if(req[p].dropped) continue;

                        max_floor=max(max_floor,req[p].preq.requestedFloor);
                    }

                    if(elv[i].curr_floor<max_floor) continue;
                    else elv[i].direction=DOWN;
                } 
                else{
                    int min_floor=INT_MAX;
                    for(int p=0;p<req_size;p++){
                        if(req[p].preq.requestId==INT_MIN) continue;
                        if(req[p].assigned_to!=i) continue;
                        if(req[p].picked) continue;
                        min_floor=min(min_floor,req[p].preq.startFloor);
                    }

                    for(int p=0;p<req_size;p++){
                        if(req[p].preq.requestId==INT_MIN) continue;
                        if(req[p].assigned_to!=i) continue;
                        if(!req[p].picked) continue;
                        if(req[p].dropped) continue;

                        min_floor=min(min_floor,req[p].preq.requestedFloor);
                    } 

                    if(elv[i].curr_floor>min_floor) continue;
                    else elv[i].direction=UP;
                }
            }
        }


        // set idles
        for(int i=0;i<N;i++){
            int pempty=1,dempty=1;
            for(int p=0;p<req_size;p++){
                if(req[p].preq.requestId==INT_MIN) continue;
                if(req[p].assigned_to!=i) continue;
                if(req[p].picked) continue;
                pempty=0;
                break;
            }

            for(int p=0;p<req_size;p++){
                if(req[p].preq.requestId==INT_MIN) continue;
                if(req[p].assigned_to!=i) continue;
                if(!req[p].picked) continue;
                if(req[p].dropped) continue;
                dempty=0;
                break;
            }

            if(pempty&&dempty){
                elv[i].prevmove=mainshm->elevatorMovementInstructions[i];
                mainshm->elevatorMovementInstructions[i]='s';
                elv[i].set=1;
            }
        } 

        //set action
        for(int i=0;i<N;i++){
            if(elv[i].set) continue;

            elv[i].prevmove=mainshm->elevatorMovementInstructions[i];
            elv[i].set=1;

            if(elv[i].direction==UP) mainshm->elevatorMovementInstructions[i]='u';
            else mainshm->elevatorMovementInstructions[i]='d';
        }

        // guess auth strings
        for(int i=0;i<N;i++){

            if(mainshm->elevatorMovementInstructions[i]=='s') continue;

            if(elv[i].len==0) continue; 

            // add threads here?
            guessed=0;
            gthread=-1;

            int combos=1,len=elv[i].len;
            for(int i=0;i<len;i++) combos*=6;

            pthread_t thread_id[THREAD_CNT];
            t_data t_data[THREAD_CNT];

            int si=0;
            for(int t=0;t<THREAD_CNT;t++){
                int ei=si+(combos/THREAD_CNT);
                if(t<combos%THREAD_CNT) ei++;
                t_data[t].idx=t;
                t_data[t].si=si;
                t_data[t].ei=ei;
                t_data[t].eno=i;
                t_data[t].solver_id=(t%M);
                si=ei;
            }

            for(int t=0;t<THREAD_CNT;t++){
                if(pthread_create(&thread_id[t],NULL,runner,(void*)&t_data[t])==-1){
                    perror("pthread_create: check main");
                    exit(1);
                }
            }

            for(int t=0;t<THREAD_CNT;t++){
                if(pthread_join(thread_id[t],NULL)==-1){
                    perror("pthread_join: check main");
                    exit(1);
                }
            }

            if(!guessed){
                // why tf is this even triggered?
                printf("Turn: %d Elevator: %d No Guess\n",t,i);
                elv[i].prevmove=mainshm->elevatorMovementInstructions[i];
                mainshm->elevatorMovementInstructions[i]='s';
            }
            else{
                sprintf(mainshm->authStrings[i],"%s",t_data[gthread].authstr);
            }
        }



        for(int i=0;i<N;i++) elv[i].len=elv[i].num_passengers;

        printf("Turn: %d Movement: %s\n",t,mainshm->elevatorMovementInstructions);
        printf("Dropped: %d Picked: %d\n\n",dropped,picked);

        TurnChangeRequest tcr_send;
        tcr_send.mtype=1;
        tcr_send.droppedPassengersCount=dropped;
        tcr_send.pickedUpPassengersCount=picked;

        if(msgsnd(mainqid,&tcr_send,sizeof(tcr_send)-sizeof(tcr_send.mtype),0)==-1){
            perror("mainqid msgsnd: check main");
            exit(1);
        }

        if(msgrcv(mainqid,&tcr_get,sizeof(tcr_get)-sizeof(tcr_get.mtype),2,0)==-1){
            perror("mainqid msgrcv: check main");
            exit(1);
        }
        t++;
    }

    free(req);

    if(shmdt(mainshm)==-1){
        perror("mainshm shmdt: check main");
        exit(1);
    }
}
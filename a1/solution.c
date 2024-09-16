#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include<pthread.h>
#include<sys/syscall.h> 

#define PERMS 0644
#define FNAME_SIZE 100
#define READ_END 0
#define WRITE_END 1
#define BUFF_SIZE 200
#define MSG_LEN 100
#define WORD_LEN 1024
#define A 26

int MATRIX_SIZE,MAX_LEN,SHM_KEY,MSG_KEY;
char infile[FNAME_SIZE],wordfile[FNAME_SIZE];

int cnt=0;

typedef struct message{
    long mtype;
    int key;
} message;

typedef struct Node{
    struct Node* children[A];
    int wordcount;
} Node;

typedef struct data{
    char* word;
    int shift;
} data;

Node* root;

Node* get_new_node(){
    Node* node=(Node*)malloc(sizeof(Node));
    node->wordcount=0;

    for(int i=0;i<A;i++){
        node->children[i]=NULL;
    }
    return node;
}

void insert(Node* root,char* word){
    Node* temp=root;
    int len=strlen(word);

    for(int i=0;i<len;i++){
        int charpos=word[i]-'a';
        if(temp->children[charpos]==NULL){
            temp->children[charpos]=get_new_node();
        }
        temp=temp->children[charpos];
    }
    temp->wordcount++;
}

void preprocess_file(Node* root){
    FILE* file=fopen(wordfile,"r");
    if(file==NULL){
        perror("No such file exists: check preprocess_file\n");
        exit(1);
    }

    char* buffer=(char*)malloc(MAX_LEN*sizeof(char));
    while(fscanf(file,"%s",buffer)!=EOF){
        insert(root,buffer);
    }
    free(buffer);

    fclose(file);
}

char* decode_caesar(char* str,int shift){
    int len=strlen(str);
    char* res=(char*)malloc((len+1)*sizeof(char));

    for(int i=0;i<len;i++){
        res[i]=(str[i]-'a'+shift)%26+'a';
    }
    res[len]='\0';
    return res;
}

int get_wordcount(char* word,Node* root) {
    Node* temp=root;
    int len=strlen(word);

    for(int i=0;i<len;i++){
        int charpos=word[i]-'a';
        if(temp->children[charpos]==NULL) return 0;
        temp=temp->children[charpos];
    }

    if(temp!=NULL) return temp->wordcount;

    return 0;
}

void read_keys_from_file(char* filename){
    FILE* file=fopen(filename,"r");
    if(file==NULL){
        perror("No such file exists: check read_keys_from_file\n");
        exit(1);
    }

    fscanf(file,"%d",&MATRIX_SIZE);
    fscanf(file,"%d",&MAX_LEN);
    fscanf(file,"%d",&SHM_KEY);
    fscanf(file,"%d",&MSG_KEY);

    fclose(file);
}

int main(int argc,char** argv){
    int file_idx=atoi(argv[1]);

    sprintf(infile,"input%d.txt",file_idx);
    sprintf(wordfile,"words%d.txt",file_idx);

    read_keys_from_file(infile); 
    
    root=get_new_node();

    preprocess_file(root);

    int shmid;
    char (*shmptr)[MATRIX_SIZE][MAX_LEN];

    if((shmid=shmget(SHM_KEY,sizeof(char[MATRIX_SIZE][MATRIX_SIZE][MAX_LEN]),PERMS))==-1){
        perror("Error in shmget: check main\n");
        exit(1);
    }

    if((shmptr=shmat(shmid,NULL,0))==(void*)-1){
        perror("Error in shmat: check main\n");
        exit(1);
    }

    int shift=0;
    int msqid;

    if((msqid=msgget(MSG_KEY,PERMS|IPC_CREAT))==-1){
        perror("Error in msgget: check main\n");
        exit(1);
    }

    for(int c=0;c<MATRIX_SIZE;c++){
        int i=0,j=c,t=0;
        cnt=0;

        while(i<MATRIX_SIZE&&j>=0){
            char* search_word=decode_caesar(shmptr[i][j],shift);
            cnt+=get_wordcount(search_word,root);
            i++; t++;
            j--;
        }

        message msg;
        msg.mtype=1;
        msg.key=cnt;

        if(msgsnd(msqid,&msg,sizeof(msg)-sizeof(msg.mtype),0)==-1){
            perror("Error in msgsnd: check main\n");
            exit(1);
        }

        if(msgrcv(msqid,&msg,sizeof(msg)-sizeof(msg.mtype),2,0)==-1){
            perror("Error in msgrcv: check main\n");
            exit(1);
        }

        shift=msg.key;
     }

    for(int r=1;r<MATRIX_SIZE;r++){
        int i=r,j=MATRIX_SIZE-1,t=0;
        cnt=0;

        while(i<MATRIX_SIZE&&j>=0){
            char* search_word=decode_caesar(shmptr[i][j],shift);
            cnt+=get_wordcount(search_word,root);
            i++; t++;
            j--;
        }

        message msg;
        msg.mtype=1;
        msg.key=cnt;

        if(msgsnd(msqid,&msg,sizeof(msg)-sizeof(msg.mtype),0)==-1){
            perror("Error in msgsnd: check main\n");
            exit(1);
        }

        if(msgrcv(msqid,&msg,sizeof(msg)-sizeof(msg.mtype),2,0)==-1){
            perror("Error in msgrcv: check main\n");
            exit(1);
        }

        shift=msg.key;
    }

    if((shmdt(shmptr))==-1){
        perror("Error in shmdt: check main\n");
        exit(1);
    }
}
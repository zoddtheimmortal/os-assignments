#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>

#define PERMS 0644
#define FNAME_SIZE 100
#define READ_END 0
#define WRITE_END 1
#define BUFF_SIZE 200

int MATRIX_SIZE,MAX_LEN,SHM_KEY,MSG_KEY;

/*
Areas to add threading:
 - decoding - make a thread to decode each str of the diagonal
 - wordcnter - new thread to cnt for each word
 - wordcnter - new thread to cnt for each letter???
*/

char* decode_ceasar(char* str,int shift){
    int len=strlen(str);
    char* res=(char*)malloc(len*sizeof(char));

    for(int i=0;i<len;i++){
        res[i]=(str[i]-'a'+shift)%26+'a';
    }
    return res;
}

void read_keys_from_file(char* filename){
    FILE* file=fopen(filename,"r");
    if(file==NULL){
        perror("No such file exists\n");
        exit(1);
    }

    fscanf(file,"%d",&MATRIX_SIZE);
    fscanf(file,"%d",&MAX_LEN);
    fscanf(file,"%d",&SHM_KEY);
    fscanf(file,"%d",&MSG_KEY);

    fclose(file);
}

int get_wordcount(char* filename, char* word) {
    int fd[2];
    
    if(pipe(fd)==-1){
        perror("Pipe failed: check get_wordcount fn\n");
        exit(1);
    }

    pid_t pid;
    pid=fork();

    if(pid==-1){
        perror("Fork failed: check get_wordcount fn\n");
        exit(1);
    }

    if (pid==0) {
        close(fd[READ_END]); 
        dup2(fd[WRITE_END], STDOUT_FILENO);
        close(fd[WRITE_END]);

        char command[200];
        snprintf(command, sizeof(command), "grep -o '\\b%s\\b' %s | wc -l", word, filename);
        execlp("sh", "sh", "-c", command, NULL);

        perror("Error in execlp: check get_wordcount\n");
        exit(1);
    } else {
        close(fd[WRITE_END]); 
        wait(NULL); 

        char buffer[BUFF_SIZE];
        int count = 0;
        if (read(fd[READ_END], buffer, BUFF_SIZE*sizeof(char)) != -1) {
            count = atoi(buffer);
        }
        close(fd[READ_END]); 
        return count;
    }
}

int main(int argc,char** argv){
    int file_idx=atoi(argv[1]);
    printf("File index: %d\n",file_idx);

    char infile[FNAME_SIZE],wordfile[FNAME_SIZE];
    sprintf(infile,"input%d.txt",file_idx);
    sprintf(wordfile,"words%d.txt",file_idx);

    read_keys_from_file(infile);
    printf("Matrix size: %d\nMax len: %d\nSHM key: %d\nMSG key: %d\n",MATRIX_SIZE,MAX_LEN,SHM_KEY,MSG_KEY);

    char* word="abc";
    int cnt=get_wordcount(wordfile,word);
    printf("Word count: %d\n",cnt);
}
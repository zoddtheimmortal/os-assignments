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

int main(int argc,char** argv){
    int file_idx=atoi(argv[1]);
    printf("File index: %d\n",file_idx);

    char infile[FNAME_SIZE],wordfile[FNAME_SIZE];
    sprintf(infile,"input%d.txt",file_idx);
    sprintf(wordfile,"words%d.txt",file_idx);

    read_keys_from_file(infile);
    printf("Matrix size: %d\nMax len: %d\nSHM key: %d\nMSG key: %d\n",MATRIX_SIZE,MAX_LEN,SHM_KEY,MSG_KEY);
}
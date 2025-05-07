#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include<math.h>

#define PERMS 0644

int main(int argc,char** argv){
    int len=3,combos=1;
    for(int i=0;i<len;i++){
        combos*=6;
    }

    printf("Combos: %d\n",combos);

    for(int i=combos-1;i>=0;i--){
        char* authstr=(char*)malloc(sizeof(char)*(len+1));
        int p=i;
        for(int j=0;j<len;j++){
            authstr[j]='a'+(p%6);
            p/=6;
        }
        authstr[len]='\0';
        printf("%s\n",authstr);
    }   
}
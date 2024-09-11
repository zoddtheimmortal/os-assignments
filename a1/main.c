#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>

#define PERMS 0644

char* decode_ceasar(char* str,int shift){
    int n=strlen(str);
    char* res=(char*)malloc(n*sizeof(char));

    for(int i=0;i<n;i++){
        res[i]=(str[i]-'a'+shift)%26+'a';
    }
    return res;
}

int main(int argc,char** argv){
      
}
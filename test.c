/*************************************************************************
	> File Name: test.c
	> Author:henuzxy 
	> Mail: 
	> Created Time: 2019年07月22日 星期一 16时20分42秒
 ************************************************************************/

#include<stdio.h>
#include<string.h>
#include<stdbool.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int main(int argc,char *argv[]){
    FILE *fp;
    fp = fopen("input.txt","r");
    if(fp == NULL){
        perror("Error:");
        //fprintf(stderr,"Error:");
        return -1;
    }
    fclose(fp);

    return 0;
}

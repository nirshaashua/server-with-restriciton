#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFFSIZE 1024
#define NICK_SIZE 16
#define PORT 1234
#define OPTLEN 16
#define MAX_HELP_SIZE 1024


struct PACKET {
    char option[OPTLEN]; // instruction
    char alias[NICK_SIZE]; // user's name
    char buff[BUFFSIZE]; // message size
};

struct THREADINFO {
    pthread_t thread_ID; // thread's pointer
    int sockfd; // socket file descriptor
    char alias[NICK_SIZE]; // user's name
};

struct USER {
        int sockfd; // user's socket descriptor
        char alias[NICK_SIZE]; // user's name
};

int isconnected, sockfd;
char option[MAX_HELP_SIZE];
struct USER me;


int connect_with_server(char *ip);
void setnickname(struct USER *me);
void logout(struct USER *me);
void login(struct USER *me,char *ip);
void *receiver(void *param);
void sendtoall(struct USER *me, char *msg);
void sendtouser(struct USER *me, char * target, char *msg);

int main(int argc, char *argv[]) {
    int nicknamesize;

    memset(&me, 0, sizeof(struct USER));
    if (argc != 2)
     {//arguments does'nt meet the request

  	    fprintf(stderr,"did'nt enter IP address on launch or enter too much parameters on launch \n");
  	    exit(EXIT_FAILURE);

     }
    strcpy(me.alias, "Anonymous");
    login(&me,argv[1]);

    printf("hello guys welcome to the best chat\n write help for instructions \n");
    while(fgets(option,MAX_HELP_SIZE-1,stdin)) {


        if(!strncmp(option, "exit", 4)) {
            logout(&me);
            break;
        }
        if(!strncmp(option, "help", 4)) {
            FILE *fin = fopen("help.md", "r");
            if(fin != NULL) {
                while(fgets(option, MAX_HELP_SIZE-1, fin))
                puts(option);
                fclose(fin);
            }
            else {
                fprintf(stderr, "Help file not found...\n");
            }
        }

        else if(!strncmp(option, "changenick", 10)) {
            char *ptr = strtok(option, " ");
            ptr = strtok(0, " ");
            memset(me.alias, 0, sizeof(char) * NICK_SIZE);
            if(ptr != NULL) {
            	nicknamesize =  strlen(ptr);
                if(nicknamesize > NICK_SIZE) ptr[NICK_SIZE] = 0;
                strcpy(me.alias, ptr);
                setnickname(&me);
            }
        }
        else if(!strncmp(option, "whisp", 5)) {
            char *ptr = strtok(option, " ");
            char temp[NICK_SIZE];
            ptr = strtok(0, " ");
            memset(temp, 0, sizeof(char) * NICK_SIZE);
            if(ptr != NULL) {
            	nicknamesize =  strlen(ptr);
                if(nicknamesize > NICK_SIZE) ptr[NICK_SIZE] = 0;
                strcpy(temp, ptr);
                while(*ptr) ptr++; ptr++;
                while(*ptr <= ' ') ptr++;
                sendtouser(&me, temp, ptr);
            }
        }
        else if(!strncmp(option, "send", 4)) {
            sendtoall(&me, &option[5]);
        }
        else if(!strncmp(option, "logout", 6)) {
            logout(&me);
        }
        else fprintf(stderr, "Unknown option...\n");
    }

    return 0;





}

void login(struct USER *me,char *ip) {

    if(isconnected==1) {
        fprintf(stderr, "You are already connected to server at %s:%d\n",ip, PORT);
        exit(EXIT_FAILURE);
    }

    sockfd= connect_with_server(ip);
    struct THREADINFO threadinfo;
    pthread_create(&threadinfo.thread_ID, NULL, receiver, (void *)&threadinfo);
    sleep(1);
        isconnected = 1;
        me->sockfd = sockfd;
        if(strcmp(me->alias, "Anonymous")) setnickname(me);
        printf("Logged in as %s\n", me->alias);
        printf("Receiver started [%d]...\n", sockfd);


    }




int connect_with_server(char * ip) {
    int newfd;
    struct sockaddr_in serv_addr;
    struct hostent *to;
    printf("trying to connect \n");
    /* generate address */
    if((to = gethostbyname(ip))==NULL) {

        fprintf(stderr, "gethostbyname() error...\n");
        exit(EXIT_FAILURE);
    }


    /* open a socket */
    if((newfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

        fprintf(stderr, "socket() error...\n");
        exit(EXIT_FAILURE);
    }

    /* set initial values */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr = *((struct in_addr *)to->h_addr);
    memset(&(serv_addr.sin_zero), 0, 8);

    /* try to connect with server */
    if(connect(newfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {

        fprintf(stderr, "connect() error...\n");
        exit(EXIT_FAILURE);
    }
    else {
        printf("Connected to server at %s:%d\n", ip, PORT);
        isconnected = 1;
        return newfd;
    }
}

void logout(struct USER *me) {

    struct PACKET packet;

    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        return;
    }

    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "exit");
    strcpy(packet.alias, me->alias);

    /* send request to close this connetion */
    send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
    isconnected = 0;
}

void setnickname(struct USER *me) {

    struct PACKET packet;

    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        exit(EXIT_FAILURE);
    }

    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "changenick");
    strcpy(packet.alias, me->alias);

    /* send request to close this connetion */
    send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}
void sendtoall(struct USER *me, char *msg) {

    struct PACKET packet;

    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        exit(EXIT_FAILURE);
    }

    msg[BUFFSIZE] = 0;

    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "send");
    strcpy(packet.alias, me->alias);
    strcpy(packet.buff, msg);

    /* send request to close this connetion */
    send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}
void sendtouser(struct USER *me, char *target, char *msg) {
    int targetlen;
    struct PACKET packet;

    if(target == NULL) {
    	exit(EXIT_FAILURE);
    }

    if(msg == NULL) {
    	exit(EXIT_FAILURE);
    }

    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        exit(EXIT_FAILURE);
    }
    msg[BUFFSIZE] = 0;
    targetlen = strlen(target);

    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "whisp");
    strcpy(packet.alias, me->alias);
    strcpy(packet.buff, target);
    strcpy(&packet.buff[targetlen], " ");
    strcpy(&packet.buff[targetlen+1], msg);

    /* send request to close this connetion */
    send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}
void *receiver(void *param) {
    int recvd;
    struct PACKET packet;


    while(isconnected) {

        recvd = recv(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
        if(!recvd) {
            fprintf(stderr, "Connection lost from server...\n");
            isconnected = 0;
            close(sockfd);
            break;
        }
        if(recvd > 0) {
        	 if(!strcmp(packet.option,"block"))
        	{
        		printf("you have been blocked \n");
        		isconnected = 0;
        		close(sockfd);
        		exit(1);


        	}
        	 else
        	 {
        	packet.alias[10]='\0';

            printf("[%s]: %s\n", packet.alias, packet.buff);
        	 }
        }
        memset(&packet, 0, sizeof(struct PACKET));
    }
    return NULL;
}

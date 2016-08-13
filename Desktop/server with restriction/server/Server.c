#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NICK_SIZE 16
#define PORT 1234
#define BUFFSIZE 1024
#define MAXQUEUE 10
#define MAX_CLIENTS	20
#define OPTLEN 16
#define HOST_SIZE 16

struct PACKET {
	char option[OPTLEN]; // instruction
	char alias[NICK_SIZE]; // user's name
	char buff[BUFFSIZE]; // payload
};

struct THREADINFO {
	pthread_t thread_ID; // thread's pointer
	int sockfd; // socket file descriptor
	char alias[NICK_SIZE]; // client's alias
};

struct LLNODE {
	struct THREADINFO threadinfo;
	struct LLNODE *next;
};

struct LLIST {
	struct LLNODE *head, *tail;
	int size;
};
int compare(struct THREADINFO *a, struct THREADINFO *b) {
	return a->sockfd - b->sockfd;
}
void list_init(struct LLIST *ll) {
	ll->head = ll->tail = NULL;
	ll->size = 0;
}
void list_insert(struct LLIST *ll, struct THREADINFO *thr_info) {
	if (ll->size == MAX_CLIENTS) {
		fprintf(stderr, "Connection full, request rejected...\n");
		exit(EXIT_FAILURE);
	}
	if (ll->head == NULL) {
		ll->head = (struct LLNODE *) calloc(sizeof(struct LLNODE), 1);
		ll->head->threadinfo = *thr_info;
		ll->head->next = NULL;
		ll->tail = ll->head;
	} else {
		ll->tail->next = (struct LLNODE *) calloc(sizeof(struct LLNODE), 1);
		ll->tail->next->threadinfo = *thr_info;
		ll->tail->next->next = NULL;
		ll->tail = ll->tail->next;
	}
	ll->size++;

}
//
void list_delete(struct LLIST *ll, struct THREADINFO *thr_info) {

	struct LLNODE* tmp = ll->head, *prev = NULL;

	while ((compare(thr_info, &tmp->threadinfo) != 0) && (tmp->next != NULL))

	{
		prev = tmp;
		tmp = tmp->next;
	}

	if (compare(thr_info, &tmp->threadinfo) == 0) {
		if (prev) {
			prev->next = tmp->next;

		}
		if (tmp->next == NULL) {
			ll->tail = prev;
		}

		free(tmp);
		ll->size--;
	}

}

void list_show(struct LLIST *ll) {
	struct LLNODE *curr;
	struct THREADINFO *thr_info;
	printf("Connection count: %d\n", ll->size);
	for (curr = ll->head; curr != NULL; curr = curr->next) {
		thr_info = &curr->threadinfo;
		printf("[%d] %s\n", thr_info->sockfd, thr_info->alias);
	}
}
int checkblock(const char *filename, char *host) {
	char temp[HOST_SIZE];
	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		fprintf(stderr, "file doesn't exist");
		exit(EXIT_FAILURE);

	}
	while (fscanf(f, "%s", temp) == 1) {
		if (strcmp(temp, host) == 0) {
			return 0;
		}
	}

	fclose(f);
	return 1;
}
void blocklist_show() {
	char temp[HOST_SIZE];
	FILE *f = fopen("hosts.md", "r");
	if (f == NULL) {
		fprintf(stderr, "file doesn't exist");
		exit(EXIT_FAILURE);

	}
	while (fscanf(f, "%s", temp) == 1) {
		printf("Host/IP: %s \n", temp);

	}

}
void addblock(char *host) {
	FILE *f = fopen("hosts.md", "a");

	fprintf(f, " \n%s", host);
	fclose(f);

}
void removeblock(char*host) {
	char temp[BUFFSIZE];
	FILE *f = fopen("hosts.md", "r");
	FILE *fptr = fopen("ptr.md", "w+");

	if (f != NULL) {

		while (fscanf(f, "%s,", temp) == 1) {
			if (strcmp(temp, host)) {
				fprintf(fptr, "%s\n", temp);
			}

		}
		fclose(f);
		fclose(fptr);
		fptr = fopen("ptr.md", "r");
		f = fopen("hosts.md", "w");
		while (fscanf(fptr, "%s", temp) == 1) {
			fprintf(f, "%s\n", temp);
		}
		fclose(f);
		fclose(fptr);
		remove("ptr.md");

	} else {
		fprintf(stderr, "file doesn't exist");
		exit(EXIT_FAILURE);
	}

}

void *client_handler(void *fd);
void *io_handler(void *param);
int sockfd, new_fd;
pthread_mutex_t clientlist_mutex;
struct LLIST client_list;
int main(int argc, char *argv[]) {

	char host[BUFFSIZE];
	struct sockaddr_in cli_addr, serv_addr;
	socklen_t clilen;
	pthread_t interrupt;
	/* initialize linked list */
	list_init(&client_list);
	/* initiate mutex */
	pthread_mutex_init(&clientlist_mutex, NULL);

	if (argc != 2) { //arguments does'nt meet the request

		fprintf(stderr,
				"did'nt enter IP address on launch or enter too much parameters on launch \n");
		exit(EXIT_FAILURE);

	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

		fprintf(stderr, "socket() failed...\n");
		exit(EXIT_FAILURE);
	}

	/* set initial values */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	memset(&(serv_addr.sin_zero), 0, 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr))
			== -1) {

		fprintf(stderr, "bind() failed...\n");
		exit(EXIT_FAILURE);
	}
	if (listen(sockfd, MAXQUEUE) == -1) {
		fprintf(stderr, "Socket listening failed \n");
		exit(EXIT_FAILURE);
	}
	fprintf(stdout,"server started\n enter list to show connection,listblock to show blocked IP address \n block to add block, rmblock to remove block, exit to close server \n");

	/* initiate interrupt handler for IO controlling */
	if (pthread_create(&interrupt, NULL, io_handler, NULL) != 0) {
		fprintf(stderr, "pthread_create() failed...\n");
		exit(EXIT_FAILURE);
	}
	struct THREADINFO threadinfo;
	threadinfo.sockfd = new_fd;
	strcpy(threadinfo.alias, "Server");
	pthread_mutex_lock(&clientlist_mutex);
	list_insert(&client_list, &threadinfo);
	pthread_mutex_unlock(&clientlist_mutex);

	while (1) {

		clilen = sizeof(cli_addr);
		if ((new_fd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen))
				== -1) {
			fprintf(stderr, "accept() failed...\n");
			exit(EXIT_FAILURE);
		} else {

			if (client_list.size == MAX_CLIENTS) {
				fprintf(stderr, "Connection full, request rejected...\n");
				continue;
			}

			printf("Connection requested received...\n");
			strcpy(host, inet_ntoa(cli_addr.sin_addr));
			if (!checkblock("hosts.md", host)) {
				struct PACKET spacket;
				fprintf(stderr, "IP address blocked\n");
				strcpy(spacket.option, "block");
				send(new_fd, (void *) &spacket,sizeof(struct PACKET), 0);
				continue;
			}
			struct THREADINFO threadinfo;
			threadinfo.sockfd = new_fd;
			strcpy(threadinfo.alias, "Anonymous");
			pthread_mutex_lock(&clientlist_mutex);
			list_insert(&client_list, &threadinfo);
			pthread_mutex_unlock(&clientlist_mutex);
			pthread_create(&threadinfo.thread_ID, NULL, client_handler,
					(void *) &threadinfo);
		}

	}

}
void *io_handler(void *param) {
	char option[OPTLEN];
	char host[OPTLEN];
	while (scanf("%s", option) == 1) {
		if (!strcmp(option, "exit")) {
			/* clean up */
			printf("Terminating server...\n");
			pthread_mutex_destroy(&clientlist_mutex);
			close(sockfd);
			exit(0);
		} else if (!strcmp(option, "list")) {
			pthread_mutex_lock(&clientlist_mutex);
			list_show(&client_list);

			pthread_mutex_unlock(&clientlist_mutex);
		}
		  else if(!strcmp(option,"listblock")) {
				        printf("The black list is: \n");
				        pthread_mutex_lock(&clientlist_mutex);
				        	blocklist_show();
				      	pthread_mutex_unlock(&clientlist_mutex);

				        }
				  else if(!strcmp(option,"block")){
					  scanf("%s",host);
					  addblock(host);
					  printf("host has been blocked:%s \n",host);
				  }
				  else if(!strcmp(option,"rmblock")){
					  scanf("%s",host);

					  removeblock(host);
					  printf("host has been retrieve:%s \n",host);
				  }
				  else {
			fprintf(stderr, "Unknown command: %s...\n", option);
		}
	}
	return NULL;
}
void *client_handler(void *fd) {
	struct THREADINFO threadinfo = *(struct THREADINFO *) fd;
	struct PACKET packet;
	struct LLNODE *curr;
	int bytes;
	while (1) {
		bytes = recv(threadinfo.sockfd, (void *) &packet, sizeof(struct PACKET),
				0);
		if (!bytes) {
			fprintf(stderr, "Connection lost from [%d] %s...\n",
					threadinfo.sockfd, threadinfo.alias);
			pthread_mutex_lock(&clientlist_mutex);
			list_delete(&client_list, &threadinfo);
			pthread_mutex_unlock(&clientlist_mutex);
			break;
		}

		if (!strcmp(packet.option, "changenick")) {
			printf("Set nick to %s\n", packet.alias);
			pthread_mutex_lock(&clientlist_mutex);
			for (curr = client_list.head; curr != NULL; curr = curr->next) {

				if (compare(&curr->threadinfo, &threadinfo) == 0) {
					strcpy(curr->threadinfo.alias, packet.alias);
					strcpy(threadinfo.alias, packet.alias);
					break;
				}
			}
			pthread_mutex_unlock(&clientlist_mutex);
		}

		else if (!strcmp(packet.option, "whisp")) {
			printf("[%d] %s %s %s\n", threadinfo.sockfd, packet.alias,
					packet.option, packet.buff);
			int i;
			char target[NICK_SIZE];
			for (i = 0; packet.buff[i] != ' '; i++)
				;
			packet.buff[i++] = 0;
			strcpy(target, packet.buff);
			pthread_mutex_lock(&clientlist_mutex);
			for (curr = client_list.head; curr != NULL; curr = curr->next) {
				if (strcmp(target, curr->threadinfo.alias) == 0) {
					struct PACKET spacket;
					memset(&spacket, 0, sizeof(struct PACKET));

					if (!compare(&curr->threadinfo, &threadinfo))
						continue;
					strcpy(spacket.option, "msg");
					strcpy(spacket.alias, packet.alias);
					strcpy(spacket.buff, &packet.buff[i]);
					send(curr->threadinfo.sockfd, (void *) &spacket,
							sizeof(struct PACKET), 0);
				}
			}
			pthread_mutex_unlock(&clientlist_mutex);
		} else if (!strcmp(packet.option, "send")) {
			printf("[%d] %s %s %s\n", threadinfo.sockfd, packet.alias,
					packet.option, packet.buff);
			pthread_mutex_lock(&clientlist_mutex);
			for (curr = client_list.head; curr != NULL; curr = curr->next) {
				struct PACKET spacket;
				memset(&spacket, 0, sizeof(struct PACKET));
				if (!compare(&curr->threadinfo, &threadinfo))
					continue;
				strcpy(spacket.option, "msg");
				strcpy(spacket.alias, packet.alias);
				strcpy(spacket.buff, packet.buff);
				send(curr->threadinfo.sockfd, (void *) &spacket,
						sizeof(struct PACKET), 0);
			}
			pthread_mutex_unlock(&clientlist_mutex);
		} else if (!strcmp(packet.option, "exit")) {
			printf("[%d] %s has disconnected...\n", threadinfo.sockfd,
					threadinfo.alias);
			pthread_mutex_lock(&clientlist_mutex);

			list_delete(&client_list, &threadinfo);
			pthread_mutex_unlock(&clientlist_mutex);

		} else {
			fprintf(stderr, "Garbage data from [%d] %s...\n", threadinfo.sockfd,
					threadinfo.alias);
		}
	}

	/* clean up */
	close(threadinfo.sockfd);

	return NULL;
}

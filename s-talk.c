#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include "list.h"

#define NUM_THREADS 4
#define BUFF_SIZE 256

pthread_t thread[NUM_THREADS];
pthread_mutex_t sendMutex;
pthread_mutex_t recvMutex;
pthread_cond_t send_cond;
pthread_cond_t recv_cond;
LIST *sendList;
LIST *recvList;
int message_length;
char buff[BUFF_SIZE];
char recv_buff[BUFF_SIZE];
int socket_num;
struct sockaddr_in host_addr, remote_addr;
struct hostent *remote_host;

void createSocket(int *host_port, char *remote_machine_name, int *remote_port){

	//struct hostent *host;
	socket_num = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_num < 0){
		printf("Error, cannot open socket\n");
		exit(0);
	}

	//Set up a remote socket
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(remote_port);

	//Get remote machine's name and save it in order for remote machine to receive message
	remote_host = gethostbyname(remote_machine_name);
	if (remote_host == NULL){
		printf("Error, cannot find remote machine by given name.\n");
		exit(0);
	}
	memcpy((void *)&remote_addr.sin_addr, remote_host->h_addr_list[0], remote_host->h_length);
	
	//Set up a host (local) sockeyt
	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons(host_port);
	host_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&host_addr.sin_zero, '\0', 8);
	if (bind(socket_num, (struct sockaddr *)&host_addr, sizeof(struct sockaddr_in)) < 0){
		printf("Error, failed to bind host socket.\n");
		exit(0);
	}

}

void recvMessage(void *unused){
	
	socklen_t socket_length = sizeof(struct sockaddr_in);
	
	while(1){

		//Receive message from the socket
		if (recvfrom(socket_num, recv_buff, BUFF_SIZE, 0, (struct sockaddr *)&host_addr, &socket_length) < 0){
			printf("Error, failed to receive from socket.\n");
			exit(0);
		}

		pthread_mutex_lock(&recvMutex);
		ListAppend(recvList, recv_buff);
		pthread_cond_signal(&recv_cond);
		pthread_mutex_unlock(&recvMutex);	
	}

}

void *keyboardInput(void *unused){

	while (1){
		//Reads the keyboard input
		if (fgets(buff, BUFF_SIZE, stdin) == NULL){
			printf("Error, can't take keyboard input. \n");
		}

		printf("[Sent by host] \n");
		pthread_mutex_lock(&sendMutex);
		ListAppend(sendList, buff);
		pthread_cond_signal(&send_cond);
		pthread_mutex_unlock(&sendMutex);
	}

}

void *sendMessage(void *unused){
	
	while (1){

		pthread_mutex_lock(&sendMutex);
		//Set waiting condition if the sendList is empty
		if (ListCount(sendList) == 0){
			pthread_cond_wait(&send_cond, &sendMutex);
		}

		if (ListCount(sendList) > 0){
			//Let sendingMsg equal to the item from the last place of the sendList
			char *sendingMsg = ListLast(sendList);
			if (sendto(socket_num, sendingMsg, strlen(sendingMsg), 0, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr_in)) < 0){
				printf("Error, failed to send to socket.\n");
				exit(0);
			}

			//Exit after sending message ! and a enter key (new line)
			if (sendingMsg[0] == '!' && sendingMsg[1] == '\n'){
				pthread_mutex_unlock(&sendMutex);
				exit(0);
			}
			
			//Remove last item from the sendList (don't need the last item anymore, already sent)
			ListTrim(sendList);
		}

		pthread_mutex_unlock(&sendMutex);
	}

}

void screenOutput(void *unused){

	while (1){

		pthread_mutex_lock(&recvMutex);
		//Set waiting condition if the recvList is empty
		if (ListCount(recvList) == 0){
			pthread_cond_wait(&recv_cond, &recvMutex);
		} 

		if (ListCount(recvList) > 0){
			//Let receivingMsg equal to the item from the last place of the recvList
			char *receivingMsg = ListLast(recvList);
			printf("[Received by remote] %s\n", receivingMsg);

			//Exit after receiving and printing message ! and a enter key (new line)
			if (receivingMsg[0] == '!' && receivingMsg[1] == '\n'){
				pthread_mutex_unlock(&recvMutex);
				exit(0);
			}

			//Remove last item from the recvList (don't need the last item anymore, already printed)
			ListTrim(recvList);
		}

		//clear previous recv_buff 
		memset(recv_buff, 0, BUFF_SIZE);
		pthread_mutex_unlock(&recvMutex);
	}

}


int main(int argc, char *argv[]){

	if (argc != 4){
		printf ("Enter 4 arguments: s-talk, my port-number, remote machine name, remote port-number\n");
		exit(1);
	}

	printf("Successfully Joined s-talk! You can chat now.\n");
	printf("Type ! if want to exit.\n");

	int *host_port = atoi(argv[1]); 
	char *remote_machine = argv[2];
	int *remote_port = atoi(argv[3]);


	//Set numbers for each threads
	int send_num = 0;
	int recv_num = 1;
	int keyboard_num = 2;
	int screen_num = 3;

	//Create sending and receiving lists
	sendList = ListCreate();
	recvList = ListCreate();

	//Create socket
	createSocket(host_port, remote_machine, remote_port);

	//Initialize mutex and condition variables
	pthread_mutex_init(&sendMutex, NULL);
	pthread_mutex_init(&recvMutex, NULL);
	pthread_cond_init(&send_cond, NULL);
	pthread_cond_init(&recv_cond, NULL);

	//create pthreads for each of 4 threads
	int iret1 = pthread_create(&thread[keyboard_num], NULL, keyboardInput, (void *)NULL);
	int iret2 = pthread_create(&thread[send_num], NULL, sendMessage, (void *)NULL);
	int iret3 = pthread_create(&thread[recv_num], NULL, recvMessage, (void *)NULL);
	int iret4 = pthread_create(&thread[screen_num], NULL, screenOutput, (void *)NULL);

	//Join threads, else won't be able to chat
	pthread_join(thread[keyboard_num], NULL);	
	pthread_join(thread[send_num], NULL);
	pthread_join(thread[recv_num], NULL);	
	pthread_join(thread[screen_num], NULL);	

	//Cancell all threads 
	pthread_cancel(thread[keyboard_num]);
	pthread_cancel(thread[send_num]);
	pthread_cancel(thread[recv_num]);
	pthread_cancel(thread[screen_num]);

	//destrory all conditions and mutexes
	pthread_cond_destroy(&send_cond);
	pthread_cond_destroy(&recv_cond);
	pthread_mutex_destroy(&sendMutex);
	pthread_mutex_destroy(&recvMutex);

	//close the socket
	close(socket_num);

	exit(0);
} 

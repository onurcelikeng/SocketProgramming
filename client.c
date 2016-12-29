#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#define BUFFER_SIZE 1024
#define SERVER "127.0.0.1"
#define TOKEN_SIZE 30

char outgoing[BUFFER_SIZE];
char incoming[BUFFER_SIZE];
char *Token; //secret key
bool isOnline = false;

void *ReceiveMessage(void *socket);
bool loginControl(char value[]);
char* XOR_Function(char *token, char *message, int messageSize);
unsigned char swapNibble(unsigned char value);
char* encrypt(char *message, int messageSize);
char* decrypt(char *message, int messageSize);


int main()
{
	int senderSocket, *newSocked;
	struct sockaddr_in server;
	struct hostent *host;
	pthread_t thread;

	host = gethostbyname(SERVER);

	if ((senderSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
		perror("Socket error"); exit(1);
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(5000);
	server.sin_addr = *((struct in_addr *)host->h_addr);

	if (connect(senderSocket, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
	{
		perror("Connect"); exit(1);
	}

	else
	{
		puts("Connected to server");
	}

    newSocked = malloc(1);
    *newSocked = senderSocket;

    if(pthread_create(&thread, NULL, ReceiveMessage, (void*) newSocked) < 0)
    {
        return 1;
    }

	while(true)
	{
		gets(outgoing);

		if(strcmp(outgoing, "q") == 0)
		{
			send(senderSocket, outgoing, strlen(outgoing), 0);
			break;
		}

		else
		{
			if(Token == NULL)
			{
				send(senderSocket, outgoing, strlen(outgoing), 0);
			}

			else 
			{
				if(loginControl(outgoing) == true)
				{
					puts("server's reply: already logged in");
				}

				else
				{
					strcpy(outgoing, encrypt(outgoing, strlen(outgoing)));
					send(senderSocket, outgoing, strlen(outgoing), 0);
				}
			}
		}
	}

    close(senderSocket);
	return 0;
}


void *ReceiveMessage(void *socket)
{
	int receiverSocket = *((int*)socket); 

	while(true)
	{
		int length = recv(receiverSocket, incoming, BUFFER_SIZE, 0);
		if(length < 1) break;

		incoming[length] = '\0';		

		if(isOnline == false)
		{
			isOnline = true;
			Token = (char *)calloc(TOKEN_SIZE * sizeof(unsigned char), 1);	
			strcpy(Token, incoming);
			puts("server's reply: login succesful");
		}

		else
		{
			printf("server's reply: ");

			int i;
			for(i = 0; i < length; i++)
			{
				printf("%02X", incoming[i]);
			}

			puts("");
			printf("server’s reply decoded as: ");
			puts(decrypt(incoming, length));
		}
	}

    free(socket);
	return 0;
}

bool loginControl(char value[])
{
	if(strstr(value, "login ") != NULL)
	{
		return true;
	}

	else
	{
		return false;
	}
}

char* XOR_Function(char *token, char *message, int messageSize)
{
	char *encryptedMessage = (char *)calloc(messageSize, 1);
	int index = 0;

	int i;
	for(i = 0; i < messageSize; i++)
	{
		encryptedMessage[i] = token[index++] ^ message[i]; //XOR operation
		if(strlen(token) == index) index = 0;
	}

	return encryptedMessage;
}

unsigned char swapNibble(unsigned char value)
{
	return ((value << 4) | (value >> 4));
}

char* encrypt(char *message, int messageSize)
{
	char *token = (char *)calloc(TOKEN_SIZE, 1);
	strcpy(token, Token);
	
	char *swapped = (char *)calloc(2 * messageSize, 1);
	unsigned char *temp = (char *)calloc(2 * sizeof(unsigned char), 1);

	int i;
	for(i = 0; i < messageSize; i++)
	{
        sprintf(temp, "%c", swapNibble(message[i]));
        strcat(swapped, temp);
    }

    strcpy(swapped, XOR_Function(token, swapped, messageSize));

    return swapped;
}

char* decrypt(char *message, int messageSize)
{
	char *token = (char *)calloc(TOKEN_SIZE, 1);
	strcpy(token, Token);

	char *swapped = (char *)calloc(2 * messageSize, 1);
	char *XORed = (char *)calloc(2 * messageSize, 1);

    strcpy(XORed, XOR_Function(token, message, messageSize));

	unsigned char *temp;
	int i;
	for(i = 0; i < messageSize; i++)
	{
        sprintf(temp, "%c", swapNibble(XORed[i]));
        strcat(swapped, temp);
    }

    return swapped;
}
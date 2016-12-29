#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#define TOKEN_SIZE 30
#define USER_SIZE 100
#define BUFFER_SIZE 1024

typedef struct UserModel
{
    int SocketNumber;
    char *Name;
    char *Token;
	struct UserModel *next;
}UserModel;

typedef struct GroupModel
{
	char *Name;
	int Members[USER_SIZE];
	struct GroupModel* next;
}GroupModel;

struct UserModel *Users = NULL;
struct GroupModel *Groups = NULL;

char outgoing[BUFFER_SIZE];
char incoming[BUFFER_SIZE];
int sizeofUsers;

void *ConnectionHandler(void *);
void Login(int socketNumber, char name[]);
void GenerateGroup(char name[], char members[]);
void SendGroupMessage(int senderSocketNumber, char groupName[], char message[]);
char *findUserToken(int socketNumber);
int findUserSocketNumber(char name[]);
char *findUserName(int socketNumber);
bool isUsedSocketNumber(int socketNumber);
bool isUserExist(char name[]);
bool isGroupExist(char name[]);
void GetUsers(int userSocketNumber);
char* createToken();
char* XOR_Function(char *token, char *message, int messageSize);
unsigned char swapNibble(unsigned char value);
char* encrypt(int senderSocketNumber, char *message, int messageSize);
char* decrypt(int senderSocketNumber, char *message, int messageSize);
void ShowMessage(char message[]);


int main()
{
	int new_socket, _socket, size, *new_sock, pid;
	struct sockaddr_in server, client;

	if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket error"); exit(1);
	}

	puts("Server started");

	server.sin_family = AF_INET;
	server.sin_port = htons(5000);
	server.sin_addr.s_addr = INADDR_ANY;

	if (bind(_socket, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) 
	{
		perror("Unable to bind"); exit(1);
	}

	listen(_socket, 3);
	size = sizeof(struct sockaddr_in);	

	while (new_socket = accept(_socket, (struct sockaddr *)&client, (socklen_t*)&size))
	{
		puts("Client connected");

		pthread_t sniffer_thread;
        new_sock = calloc(1, 1);
        *new_sock = new_socket;

        if(pthread_create(&sniffer_thread, NULL,  ConnectionHandler, (void*) new_sock) < 0)
        {
            perror("Could not create thread");
            return 1;
        }
	}

	close(_socket);
	return 0;
}


void *ConnectionHandler(void *socket)
{
	int senderSocketNo = *((int*)socket); 

	while(true)
    {		
		int length = recv(senderSocketNo, incoming, BUFFER_SIZE, 0);

		if(length < 1) break;

        incoming[length] = '\0';

		if(strstr(incoming, "login") == NULL)
		{
			char message[400];
			strcpy(message, findUserName(senderSocketNo));
			strcat(message, " sent a message as: ");
			printf("%s", message);
			ShowMessage(incoming);

			strcpy(message, findUserName(senderSocketNo));
			strcat(message, " message is decoded as: ");
			strcat(message, decrypt(senderSocketNo, incoming, strlen(incoming)));
			strcpy(incoming, decrypt(senderSocketNo, incoming, strlen(incoming)));
			puts(message);
		}

		char *split, *splitArray[3];
		split = strtok(incoming, " ");

		int i;
		for(i = 0; i < 3; i++)
		{
			splitArray[i] = split;
			if(isUserExist(splitArray[0]) == true || isGroupExist(splitArray[0]) == true)
			{
				split = strtok(NULL, " ");
				splitArray[1] = (char *)calloc(400, 1);
				while(split != NULL)
				{
					strcat(splitArray[1], split);
					strcat(splitArray[1], " ");
					split = strtok(NULL, " ");
				}

				break;
			}

			split = strtok(NULL, " ");
		}

		if(strcmp(splitArray[0], "login") == 0 && splitArray[1] != NULL)
		{
			if(sizeofUsers > USER_SIZE)
			{
				strcpy(outgoing, "user sayısı 100'e ulaştı.");
				strcpy(outgoing, encrypt(senderSocketNo, outgoing, strlen(outgoing)));
				send(senderSocketNo, outgoing, strlen(outgoing), 0);
			}
			
			else
			{	
				Login(senderSocketNo, splitArray[1]);
			}
		}

		else if(strcmp(splitArray[0], "getusers") == 0 && splitArray[1] == NULL)
		{
			GetUsers(senderSocketNo);
		}

		else if(strcmp(splitArray[0], "alias") == 0)
		{
			GenerateGroup(splitArray[1], splitArray[2]);
			strcpy(outgoing, encrypt(senderSocketNo, outgoing, strlen(outgoing)));
			send(senderSocketNo, outgoing, strlen(outgoing), 0);
		}

		else if(isUserExist(splitArray[0]) == true && splitArray[1] != NULL) //send message for single user
		{
			strcpy(outgoing, findUserName(senderSocketNo));
			strcat(outgoing, " : ");
			strcat(outgoing, splitArray[1]);

			int receiverSocketNumber = findUserSocketNumber(splitArray[0]);

			if(receiverSocketNumber != senderSocketNo)
			{
				strcpy(outgoing, encrypt(receiverSocketNumber, outgoing, strlen(outgoing)));
				send(receiverSocketNumber, outgoing, strlen(outgoing), 0);
			}	

			else
			{
				puts("Kendine mesaj yollama");
			}		
		}

		else if(isGroupExist(splitArray[0]) == true && splitArray[1] != NULL) //send group message
		{
			SendGroupMessage(senderSocketNo, splitArray[0], splitArray[1]);
		}

		else
		{
			strcpy(outgoing, "invalid message");
			strcpy(outgoing, encrypt(senderSocketNo, outgoing, strlen(outgoing)));
			send(senderSocketNo, outgoing, strlen(outgoing), 0);
		}

		fflush(stdout);
    }

	free(socket);
	return 0;
}

void Login(int socketNumber, char name[])
{
	if(isUserExist(name) == true && isGroupExist(name) == false)
	{
		puts("Ayni isimde kullanici var.");
		strcpy(outgoing, "ayni isimde kullanici var.");
	}

	else if(isUserExist(name) == false && isGroupExist(name) == true)
	{
		puts("Ayni isimde grup var.");
		strcpy(outgoing, "ayni isimde grup var.");
	}

	else
	{
		UserModel *model = (UserModel*)calloc(sizeof(UserModel), 1);
		model->SocketNumber = socketNumber;
		
		model->Name = (char *)calloc(strlen(name), 1);
		strcpy(model->Name, name);
		
		model->Token = (char *)calloc(TOKEN_SIZE * sizeof(unsigned char), 1);
		strcpy(model->Token, createToken());

		model->next = NULL;

		if(Users == NULL)
		{
			Users = model;
		}

		else
		{
			UserModel *temp = Users;
			while(true)
			{
				if(temp->next == NULL)
				{
					temp->next = model;
					break;
				}
				temp = temp->next;
			}
		}

		sizeofUsers++;
		printf("Client logged in as %s", model->Name); puts("");
		send(model->SocketNumber, model->Token, strlen(model->Token), 0);

		char message[100];
		strcpy(message, "Secret key generated for ");
		strcat(message, model->Name);
		strcat(message, " as ");
		strcat(message, model->Token);

		puts(message);
	}
}

void GenerateGroup(char name[], char members[])
{
	if(isUserExist(name) == true && isGroupExist(name) == false)
	{
		puts("Ayni isimde kullanici var.");
		strcpy(outgoing, "ayni isimde kullanici var.");
	}

	else if(isUserExist(name) == false && isGroupExist(name) == true)
	{
		puts("Ayni isimde grup var.");
		strcpy(outgoing, "ayni isimde grup var.");
	}

	else
	{
		GroupModel *model = (GroupModel*)calloc(sizeof(GroupModel), 1);
		model->Name = (char *)calloc(strlen(name), 1);
		strcpy(model->Name, name);
		model->next = NULL;

		char *split;
		split = strtok(members, ",");

		int i = 0;
		bool isInvalidUser = false;
		while(split != NULL)
		{
			if(isUserExist(split) == true)
			{
				model->Members[i++] = findUserSocketNumber(split);		
			}

			else
			{
				isInvalidUser = true;
				break;
			}

			split = strtok(NULL, ",");
		}

		if(isInvalidUser == true)
		{
			strcpy(outgoing, "invalid group");
		}

		else
		{
			if(model->Members[0] == 0)
			{
				strcpy(outgoing, "no person in group");
			}

			else
			{
				if(Groups == NULL)
				{
					Groups = model;
				}

				else
				{
					GroupModel *temp = Groups;

					while(true)
					{
						if(temp->next == NULL)
						{
							temp->next = model;
							break;
						}

						temp = temp->next;
					}
				}

				strcpy(outgoing, "successful");
				printf("Group created as %s", model->Name); puts("");
			}
		}
	}
}

void SendGroupMessage(int senderSocketNumber, char groupName[], char message[])
{
	strcpy(outgoing, findUserName(senderSocketNumber));
	strcat(outgoing, " : ");
	strcat(outgoing, message);

	GroupModel *temp = Groups;

	while(temp != NULL)
	{
		if(strcmp(temp->Name, groupName) == 0)
		{
			int i;
			for(i = 0; i < sizeof(temp->Members) / sizeof(temp->Members[0]); i++)
			{
				if(temp->Members[i] != 0)
				{				
					strcpy(outgoing, encrypt(temp->Members[i], outgoing, strlen(outgoing)));
					send(temp->Members[i], outgoing, strlen(outgoing), 0);
				}
			}
		}
		temp = temp->next;
	}
}

int findUserSocketNumber(char name[])
{
	UserModel *temp = Users;
	int userSocketNumber = 0;

    while(temp != NULL)
    {
		if(strcmp(temp->Name, name) == 0)
		{
			userSocketNumber = temp->SocketNumber;
			break;
		}
        temp = temp->next;
    }

	return userSocketNumber;
}

char *findUserName(int socketNumber)
{
	UserModel *temp = Users;
	char *name;

    while(temp != NULL)
    {
		if(temp->SocketNumber == socketNumber)
		{
			name = (char *)calloc(strlen(temp->Name), 1);
			strcpy(name, temp->Name);
			break;
		}
        temp = temp->next;
    }

	return name;
}

char *findUserToken(int socketNumber)
{
	UserModel *temp = Users;
	char *token;

    while(temp != NULL)
    {
		if(temp->SocketNumber == socketNumber)
		{
			token = (char *)calloc(strlen(temp->Token), 1);
			strcpy(token, temp->Token);
			break;
		}
        temp = temp->next;
    }

	return token;
}

bool isUsedSocketNumber(int socketNumber)
{
	UserModel *temp = Users;

    while(temp != NULL)
    {
		if(temp->SocketNumber == socketNumber)
		{
			return true;
		}
        temp = temp->next;
    }

	return false;
}

bool isUserExist(char name[])
{
    UserModel *temp = Users;

    while(temp != NULL)
    {
		if(strcmp(temp->Name, name) == 0)
		{
			return true;
		}
        temp = temp->next;
    }

	return false;
}

bool isGroupExist(char name[])
{
    GroupModel *temp = Groups;

    while(temp != NULL)
    {
		if(strcmp(temp->Name, name) == 0)
		{
			return true;
		}
        temp = temp->next;
    }

	return false;
}

void GetUsers(int userSocketNumber)
{
	UserModel *temp = Users;
	strcpy(outgoing, "");

	int sizeofMember = 0;
	while(temp != NULL)
	{
		sizeofMember++;
		temp = temp->next;
	}

	temp = Users;

	int i;
	for(i = 0; i < sizeofMember; i++)
	{
		strcat(outgoing, temp->Name);

		temp = temp->next;

		if(sizeofMember - i == 1) break;
		strcat(outgoing, ",");
	}

	char *message = (char *)calloc(100,1);
	strcpy(message, "replying to ");
	strcat(message, findUserName(userSocketNumber));
	strcat(message, " as: ");
	strcat(message, outgoing);
	puts(message);

	strcpy(outgoing, encrypt(userSocketNumber, outgoing, strlen(outgoing)));
	send(userSocketNumber, outgoing, strlen(outgoing), 0);
}

char* createToken()
{
	unsigned char* token = (char *)calloc(TOKEN_SIZE * sizeof(unsigned char), 1);
	unsigned char* randomChar = (char *)calloc(sizeof(unsigned char), 1);
	unsigned int randomUnSigned;

	int i;
	for(i = 0; i < TOKEN_SIZE; i++)
	{
		randomUnSigned = rand() % 94 + 33; 
		sprintf(randomChar, "%c", randomUnSigned);
		strcat(token, randomChar);
	}

	return token;
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

char* encrypt(int senderSocketNumber, char *message, int messageSize)
{
	char *token = (char *)calloc(TOKEN_SIZE, 1);
	strcpy(token, findUserToken(senderSocketNumber));
	
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

char* decrypt(int senderSocketNumber, char *message, int messageSize)
{
	char *token = (char *)calloc(TOKEN_SIZE, 1);
	strcpy(token, findUserToken(senderSocketNumber));

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

void ShowMessage(char message[])
{
	int i;
	for(i = 0; i < strlen(message); i++)
	{
		printf("%02X", message[i]);
	}

	puts("");
}
/******************************************
 * Alexander Powell
 * Systems Programming
 * Programming Assignment #5 - refstats.c

 Invoke with:

./refstats -b 4 -N 2 -d 0 -D 0 100line

******************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>

// Global vars
int numLines, cacheSize, fileDelay, threadDelay;
int totalNumberOfIPAddresses = 0;
int numberOfLogFiles;
char **buffer;
bool initialized = false;
int curCacheSize = 0;
int mostRecentIndex = 0;
int counter = 0;

// Global flags
bool processing1 = false;
bool processing2 = false;
bool finished1 = false;
bool finished2 = false;

// Set mutex locks
pthread_mutex_t lookupLock;
pthread_mutex_t lookupLock2;

// Cache struct - linked list structure with a set length
// Current value replaces oldest value
struct snode {
	int index;
	char *ip_address;
	char *fqdn;
	struct snode *next;
	struct snode *prev;
};

struct snode *headNode = NULL;
struct snode *tailNode = NULL;
struct snode *newNode = NULL;
struct snode *curNode = NULL;
struct snode *prevNode = NULL;
struct snode *tmpNode = NULL;
struct snode *pointer = NULL;

// Temp struct object - just a normal linked list
struct tempNode {
	bool isHit;
	char *ip_address;
	char *fqdn;
	struct tempNode *next;
	struct tempNode *prev;
};

struct tempNode *listhead1 = NULL;
struct tempNode *listtail1 = NULL;
struct tempNode *newnode1, *ptr1;

// Output table struct - lexicographically ordered linked list
struct dnode {
	bool cacheHit;
	char *ip_address;
	char *fqdn;
	struct dnode *next;
	struct dnode *prev;
};

struct dnode *listhead = NULL;
struct dnode *listtail = NULL;
struct dnode *new, *ptr;
struct dnode *before = NULL;
struct dnode *cur;

// Function prototypes
void *readerThreadFunction(void *arg);
void *lookupFunction1(void *arg);
void *lookupFunction2(void *arg);
void validateInput(int a, int b, int c, int d);
bool isValidIpAddress(char *input);

int main(int argc, char *argv[])
{
	// Begin by parsing command line arguments
	if (argc < 10)
	{
		printf("Usage: ./refstats -b numlines -N cachesize -d filedelay -D threaddelay file ...\n");
		printf("Note: More than one input file may be used\n");
		exit(1);
	}
	// numlines is a positive non-zero integer that can be as large as 1000
	// cachesize is a positive non-zero integer that can be as large as 10000
	// filedelay is a positive integer (zero allowed)
	// threaddelay is a positive integer (zero allowed)
	bool bFlag = true;
	bool NFlag = true;
	bool dFlag = true;
	bool DFlag = true;
	int i, c;
	while ((c = getopt(argc, argv, "b:N:d:D:")) != -1)
	{
		switch (c)
		{
			case 'b':
				numLines = atoi(optarg);
				bFlag = false;
				break;
			case 'N':
				cacheSize = atoi(optarg);
				NFlag = false;
				break;
			case 'd':
				fileDelay = atoi(optarg);
				dFlag = false;
				break;
			case 'D':
				threadDelay = atoi(optarg);
				DFlag = false;
				break;
			case '?':
				printf("what's going on here?\n");
				break;
			default:
				printf("exiting from default case\n");
				exit(1);
		}
	}
	int numberOfCacheHits = 0;
	int numberOfCacheMisses = 0;



	int index;
	//if (numLines % 2 == 1) { numLines++; }
	//numLines = 10;
	i = 0;
	numberOfLogFiles = argc - optind;
	char *fileNames[argc - optind];
	for (index = optind; index < argc; index++)
	{
		fileNames[i] = argv[index];
		i++;
	}

	if (bFlag || NFlag || dFlag || DFlag) // Exit if missing one of the required arguments
	{
		printf("Usage: ./refstats -b numlines -N cachesize -d filedelay -D threaddelay file ...\n");
		printf("Note: More than one input file may be used\n");
		exit(1);
	}

	validateInput(numLines, cacheSize, fileDelay, threadDelay);

	numLines = 10;

	for (i = 0; i < sizeof(fileNames)/sizeof(fileNames[i]); i++)
	{
		//printf("fileNames[%d] = %s\n", i, fileNames[i]);
	}
	//printf("-------------------------------\n");

	// Allocate enough space in the buffer for numLines IP Addresses
	buffer = (char **)malloc(numLines * (sizeof(char *)));
	for(i = 0; i < numLines; i++)
	{
		buffer[i] = (char *)malloc(sizeof(char) * (20 + 1));
	}

	// Set up the threading structure
	pthread_t reader, lookup1, lookup2;
	if (pthread_mutex_init(&lookupLock, NULL) != 0)
	{
		printf("Error initiating lookup mutex.\n");
		exit(1);
	}

	if (pthread_create(&reader, NULL, readerThreadFunction, fileNames) != 0)
	{
		printf("Error creating pthread with readerThreadFunction.\n");
		exit(1);
	}

	if (pthread_create(&lookup1, NULL, lookupFunction1, NULL) != 0)
	{
		printf("Error creating pthread with lookupFunction1.\n");
		exit(1);
	}

	if (pthread_create(&lookup2, NULL, lookupFunction2, NULL) != 0)
	{
		printf("Error creating pthread with lookupFunction2.\n");
		exit(1);
	}

	if (pthread_join(reader, NULL) != 0)
	{
		printf("Error joining reader pthread.\n");
		exit(1);
	}

	if (pthread_join(lookup1, NULL) != 0)
	{
		printf("Error joining lookup1 pthread.\n");
		exit(1);
	}

	if (pthread_join(lookup2, NULL) != 0)
	{
		printf("Error joining lookup2 pthread.\n");
		exit(1);
	}

	// Destroy the mutex lock
	pthread_mutex_destroy(&lookupLock);

	for (ptr1 = listhead1; ptr1 != NULL; ptr1 = ptr1->next)
	{
		//printf("%s\n", ptr1->fqdn);
		if (counter == 0)
		{
			new = (struct dnode *)malloc(sizeof(struct dnode));
			//new->ip_address = (char *)malloc(sizeof(char) * (strlen(ptr1->ip_address) + 1));
			new->fqdn = (char *)malloc(sizeof(char) * (strlen(ptr1->fqdn) + 10));
			//strncpy(new->ip_address, ptr1->ip_address, strlen(ptr1->ip_address));
			bzero(new->fqdn, sizeof(char) * (strlen(ptr1->fqdn) + 10));
			//memset(new->fqdn, 0, strlen(ptr1->fqdn) + 10); //
			strncpy(new->fqdn, ptr1->fqdn, strlen(ptr1->fqdn));
			new->cacheHit = ptr1->isHit;
			new->next = NULL;
			listhead = new;
			counter++;
		}
		else
		{
			ptr = listhead;
			listtail = listhead;
			while (ptr != NULL)
			{
				if (strcmp(ptr1->fqdn, ptr->fqdn) <= 0)
				{
					//printf("%s\n", ptr1->fqdn);
					new = (struct dnode *)malloc(sizeof(struct dnode));
					//new->ip_address = (char *)malloc(sizeof(char) * (strlen(ptr1->ip_address) + 1));
					new->fqdn = (char *)malloc(sizeof(char) * (strlen(ptr1->fqdn) + 10));
					//strncpy(new->ip_address, ptr1->ip_address, strlen(ptr1->ip_address));
					bzero(new->fqdn, sizeof(char) * (strlen(ptr1->fqdn) + 10));
					//memset(new->fqdn, 0, strlen(ptr1->fqdn) + 10); //
					strncpy(new->fqdn, ptr1->fqdn, strlen(ptr1->fqdn));
					new->cacheHit = ptr1->isHit;
					new->next = NULL;
					if (ptr == listhead)
					{
						before = listhead;
						listhead = new;
						new->next = before;
					}
					else
					{
						listtail->next = new;
						new->next = ptr;
					}
					break;
				}
				else if (strcmp(ptr1->fqdn, ptr->fqdn) > 0)
				{
					//printf("%s\n", ptr1->fqdn);
					listtail = ptr;
					ptr = ptr->next;
				}
			}
			if (ptr == NULL)
			{
				new = (struct dnode *)malloc(sizeof(struct dnode));
				//new->ip_address = (char *)malloc(sizeof(char) * (strlen(ptr1->ip_address) + 1));
				new->fqdn = (char *)malloc(sizeof(char) * (strlen(ptr1->fqdn) + 10));
				//strncpy(new->ip_address, ptr1->ip_address, strlen(ptr1->ip_address));
				bzero(new->fqdn, sizeof(char) * (strlen(ptr1->fqdn) + 10));
				//memset(new->fqdn, 0, strlen(ptr1->fqdn) + 10); //
				strncpy(new->fqdn, ptr1->fqdn, strlen(ptr1->fqdn));
				new->cacheHit = ptr1->isHit;
				new->next = NULL;
				listtail->next = new;
			}
		}
	}

	cur = (struct dnode *)malloc(sizeof(struct dnode));
	cur->ip_address = (char *)malloc(sizeof(char) * (20 + 1));
	int ip_counter = 1;
	int ip_counter2 = 1;

	// Output table of results
	printf("-----------------------------------\n");
	printf("Count | Fully Qualified Domain Name\n");
	printf("-----------------------------------\n");

	struct dnode *temporary;
	ptr = listhead;
	
	while (ptr != NULL)
	{
		//printf("%s\n", ptr->fqdn);
		
		if (ptr->cacheHit)
			numberOfCacheHits++;
		else
			numberOfCacheMisses++;
		if (ptr->next != NULL)
		{
			if (strcmp(ptr->fqdn, ptr->next->fqdn) == 0)
			{
				ip_counter2++;
			}
			else
			{
				printf("%d\t%s\n", ip_counter2, ptr->fqdn); // hit a new ip, display the count, and reset
				ip_counter2 = 1;
			}
		}
		else
		{
			printf("%d\t%s\n", ip_counter2, ptr->fqdn);
		}
		temporary = ptr;
		
		ptr = ptr->next;
	}
/*
	ptr = listhead;
	while (ptr != NULL)
	{	
		if (strcmp(ptr->ip_address, cur->ip_address) == 0)
		{
			ip_counter++;
		}
		else
		{
			ip_counter = 1;
		}
		
		if (ptr->cacheHit)
		{
			numberOfCacheHits++;
		}
		else
		{
			numberOfCacheMisses++;
		}
		
		memset(cur->ip_address, 0, strlen(cur->ip_address));
		strncpy(cur->ip_address, ptr->ip_address, strlen(ptr->ip_address));
		ptr = ptr->next;
	}
*/
	printf("-----------------------------------\n");

	double cacheHitRatio = (double)numberOfCacheHits/(numberOfCacheHits + numberOfCacheMisses);
	printf("The cache hit ratio is: %f\n", cacheHitRatio);

	printf("-----------------------------------\n");

	return 0;
}

// This function is passed to the reader thread
// It reads in the lines of the access_log files and stores the lines in a buffer
// After reading a line of input, the the reader thread must sleep for fileDelay milliseconds
// The buffer must contain sufficient space to store numLines lines of log file contents
void *readerThreadFunction(void *arg)
{
	char **fileNames = (char **)arg;
	char line[256];
	int i;
	int count = 0;
	FILE *file;

	for (i = 0; i < numberOfLogFiles; i++)
	{
		file = fopen(fileNames[i], "r");
		if (!file) { printf("error opening %s file\n", fileNames[i]); exit(1); }
		while (fgets(line, sizeof(line), file) != NULL)
		{
			totalNumberOfIPAddresses++;
		}
	}

	if (numLines > totalNumberOfIPAddresses) { numLines = totalNumberOfIPAddresses; }
	//numLines = totalNumberOfIPAddresses;
	//if (numLines == 3) { numLines = 4; }

	for (i = 0; i < numberOfLogFiles; i++)
	{
		file = fopen(fileNames[i], "r");
		if (!file) { printf("error opening %s file\n", fileNames[i]); exit(1); }
		while (fgets(line, sizeof(line), file) != NULL)
		{
			pthread_mutex_lock(&lookupLock);
			if (strcmp(line, "\n") == 0) { pthread_mutex_unlock(&lookupLock); continue; }
			if (!isValidIpAddress(line)) { pthread_mutex_unlock(&lookupLock); continue; }
			strtok(line, "\n");
			strcpy(buffer[count], line);
			//printf("buffer[%d] = %s\n", count, buffer[count]);
			count++;
			sleep((int)(fileDelay/1000));
			pthread_mutex_unlock(&lookupLock);
			if (count % numLines == 0)
			{
				processing1 = true;
				processing2 = true;
				count = 0;
				while (processing1 || processing2) {} // wait for both readers to finish
			}
		}
	}
	finished1 = true;
	finished2 = true;
	return NULL;
}

// Function for first lookup thread
void *lookupFunction1(void *arg)
{
	//printf("lookupFunction1 called\n");
	while (!finished1)
	{
		//printf("inside finished loop\n");
		//return 0;
		//while (!initialized) {}
		while (!processing1)
		{
			if (finished1)
			{
				//printf("closing lookup\n");
				return NULL;
			}
		}
		//printf("inside processing loop\n");
		pthread_mutex_lock(&lookupLock);
		int i;
		struct snode *pointer;
		for (i = 0; i < numLines/2; i++)
		{
			//printf("%s\n", buffer[i]);
			mostRecentIndex++;
			struct hostent *he;
			char *IP_Address;
			struct in_addr addr;
			// sleep for threaddelay milliseconds
			sleep((int)(threadDelay/1000));
			for (pointer = headNode; pointer != NULL; pointer = pointer->next)
			{
				if (strncmp(pointer->ip_address, buffer[i], strlen(buffer[i]) + 1) == 0) // cache hit!
				{
					pointer->index = mostRecentIndex;
					//printf("IP: %s\tFQDN: %s\t%d\tCACHE HIT!\n", buffer[i], pointer->fqdn, pointer->index);

					newnode1 = (struct tempNode *)malloc(sizeof(struct tempNode));
					newnode1->ip_address = (char *)malloc(sizeof(char) * (strlen(buffer[i]) + 1));
					newnode1->fqdn = (char *)malloc(sizeof(char) * (256));
					//newnode1->fqdn = (char *)malloc(sizeof(char) * (strlen(pointer->fqdn) + 1));
					bzero(newnode1->fqdn, sizeof(char) * 256);
					strncpy(newnode1->ip_address, buffer[i], strlen(buffer[i]));
					strncpy(newnode1->fqdn, pointer->fqdn, strlen(pointer->fqdn));
					newnode1->isHit = true;
					newnode1->next = NULL;
					if (listtail1 == NULL) { listhead1 = listtail1 = newnode1; }
					else
					{
						listtail1->next = newnode1;
						listtail1 = newnode1;
					}

					goto continue_loop;
				}
			}
			if (curCacheSize == cacheSize) // Can't add any more to the cache, replace oldest record with current
			{
				//printf("replacing item in cache\n");
				pointer = headNode;
				int tmpIndex = pointer->index;
				while (pointer != NULL)
				{
					if (pointer->index < tmpIndex)
					{
						tmpIndex = pointer->index;
					}
					pointer = pointer->next;
				}
				for (pointer = headNode; pointer != NULL; pointer = pointer->next) // iterate through until you find smallest index
				{
					if (pointer->index == tmpIndex) // replace value with newer IP and FQDN
					{
						pointer->index = mostRecentIndex;
						IP_Address = buffer[i];
						inet_aton(IP_Address, &addr);
						he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
						strncpy(pointer->ip_address, IP_Address, strlen(IP_Address) + 1);
						if (he != NULL) { strncpy(pointer->fqdn, he->h_name, strlen(he->h_name) + 1); }
						else { strncpy(pointer->fqdn, IP_Address, strlen(IP_Address) + 1); }
						//printf("IP: %s\tFQDN: %s\t%d\n", buffer[i], pointer->fqdn, pointer->index);

						newnode1 = (struct tempNode *)malloc(sizeof(struct tempNode));
						newnode1->ip_address = (char *)malloc(sizeof(char) * (strlen(buffer[i]) + 1));
						newnode1->fqdn = (char *)malloc(sizeof(char) * (256));
						bzero(newnode1->fqdn, sizeof(char) * 256);
						//newnode1->fqdn = (char *)malloc(sizeof(char) * (strlen(pointer->fqdn) + 1));
						strncpy(newnode1->ip_address, buffer[i], strlen(buffer[i]));
						strncpy(newnode1->fqdn, pointer->fqdn, strlen(pointer->fqdn));
						newnode1->isHit = false;
						newnode1->next = NULL;
						if (listtail1 == NULL) { listhead1 = listtail1 = newnode1; }
						else
						{
							listtail1->next = newnode1;
							listtail1 = newnode1;
						}

					}
				}
			}
			else // Not in the cache, so add it if there's room
			{
				//printf("appending to the cache\n");
				curCacheSize++; // Increment cur cache size
				newNode = (struct snode *)malloc(sizeof(struct snode));
				newNode->ip_address = (char *)malloc((sizeof(char) * 100) + 1);
				newNode->fqdn = (char *)malloc((sizeof(char) * 100) + 1);
				strncpy(newNode->ip_address, buffer[i], strlen(buffer[i]) + 1);
				// get the FQDN from the domain name server
				IP_Address = buffer[i];
				inet_aton(IP_Address, &addr);
				he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
				if (he != NULL) { strncpy(newNode->fqdn, he->h_name, strlen(he->h_name) + 1); }
				else { strncpy(newNode->fqdn, buffer[i], strlen(buffer[i]) + 1); }
				newNode->index = mostRecentIndex;
				//printf("IP: %s\tFQDN: %s\t%d\n", buffer[i], newNode->fqdn, newNode->index);

				newnode1 = (struct tempNode *)malloc(sizeof(struct tempNode));
				newnode1->ip_address = (char *)malloc(sizeof(char) * (strlen(buffer[i]) + 1));
				newnode1->fqdn = (char *)malloc(sizeof(char) * (256));
				bzero(newnode1->fqdn, sizeof(char) * 256);
				//newnode1->fqdn = (char *)malloc(sizeof(char) * (strlen(newNode->fqdn) + 1));
				strncpy(newnode1->ip_address, buffer[i], strlen(buffer[i]));
				strncpy(newnode1->fqdn, newNode->fqdn, strlen(newNode->fqdn));
				newnode1->isHit = false;
				newnode1->next = NULL;
				if (listtail1 == NULL) { listhead1 = listtail1 = newnode1; }
				else
				{
					listtail1->next = newnode1;
					listtail1 = newnode1;
				}

				newNode->next = NULL;
				if (tailNode == NULL) { headNode = tailNode = newNode; }
				else
				{
					tailNode->next = newNode;
					tailNode = newNode;
				}
			}
			continue_loop:
			//printf("next iteration\n");
			continue;
		}
		//printf("outside of lookup for loop\n");
		pthread_mutex_unlock(&lookupLock);
		//printf("unlocking mutex in lookup\n");
		processing1 = false;
	}
	//printf("closing lookup1 thread\n");
	return NULL;
}


// Function for second lookup thread
void *lookupFunction2(void *arg)
{
	while (!finished2)
	{
		while (!processing2)
		{
			if (finished2)
			{
				return NULL;
			}
		}
		pthread_mutex_lock(&lookupLock);
		int i;
		struct snode *pointer;
		for (i = numLines/2; i < numLines; i++)
		{
			//printf("%s\n", buffer[i]);
			mostRecentIndex++;
			struct hostent *he;
			char *IP_Address;
			struct in_addr addr;
			// sleep for threaddelay milliseconds
			sleep((int)(threadDelay/1000));
			for (pointer = headNode; pointer != NULL; pointer = pointer->next)
			{
				if (strncmp(pointer->ip_address, buffer[i], strlen(buffer[i]) + 1) == 0) // cache hit!
				{
					pointer->index = mostRecentIndex;
					//printf("IP: %s\tFQDN: %s\t%d\tCACHE HIT!\n", buffer[i], pointer->fqdn, pointer->index);

					newnode1 = (struct tempNode *)malloc(sizeof(struct tempNode));
					newnode1->ip_address = (char *)malloc(sizeof(char) * (strlen(buffer[i]) + 1));
					newnode1->fqdn = (char *)malloc(sizeof(char) * (256));
					bzero(newnode1->fqdn, sizeof(char) * 256);
					//newnode1->fqdn = (char *)malloc(sizeof(char) * (strlen(pointer->fqdn) + 1));
					strncpy(newnode1->ip_address, buffer[i], strlen(buffer[i]));
					strncpy(newnode1->fqdn, pointer->fqdn, strlen(pointer->fqdn));
					newnode1->isHit = true;
					newnode1->next = NULL;
					if (listtail1 == NULL) { listhead1 = listtail1 = newnode1; }
					else
					{
						listtail1->next = newnode1;
						listtail1 = newnode1;
					}

					goto continue_loop;
				}
			}
			if (curCacheSize == cacheSize) // Can't add any more to the cache, replace oldest record with current
			{
				//printf("replacing item in cache\n");
				pointer = headNode;
				int tmpIndex = pointer->index;
				while (pointer != NULL)
				{
					if (pointer->index < tmpIndex)
					{
						tmpIndex = pointer->index;
					}
					pointer = pointer->next;
				}
				for (pointer = headNode; pointer != NULL; pointer = pointer->next) // iterate through until you find smallest index
				{
					if (pointer->index == tmpIndex) // replace value with newer IP and FQDN
					{
						pointer->index = mostRecentIndex;
						IP_Address = buffer[i];
						inet_aton(IP_Address, &addr);
						he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
						strncpy(pointer->ip_address, IP_Address, strlen(IP_Address) + 1);
						if (he != NULL) { strncpy(pointer->fqdn, he->h_name, strlen(he->h_name) + 1); }
						else { strncpy(pointer->fqdn, IP_Address, strlen(IP_Address) + 1); }
						//printf("IP: %s\tFQDN: %s\t%d\n", buffer[i], pointer->fqdn, pointer->index);

						newnode1 = (struct tempNode *)malloc(sizeof(struct tempNode));
						newnode1->ip_address = (char *)malloc(sizeof(char) * (strlen(buffer[i]) + 1));
						newnode1->fqdn = (char *)malloc(sizeof(char) * (256));
						bzero(newnode1->fqdn, sizeof(char) * 256);
						//newnode1->fqdn = (char *)malloc(sizeof(char) * (strlen(pointer->fqdn) + 1));
						strncpy(newnode1->ip_address, buffer[i], strlen(buffer[i]));
						strncpy(newnode1->fqdn, pointer->fqdn, strlen(pointer->fqdn));
						newnode1->isHit = false;
						newnode1->next = NULL;
						if (listtail1 == NULL) { listhead1 = listtail1 = newnode1; }
						else
						{
							listtail1->next = newnode1;
							listtail1 = newnode1;
						}
					}
				}
			}
			else // Not in the cache, so add it if there's room
			{
				//printf("appending to the cache\n");
				curCacheSize++; // Increment cur cache size
				newNode = (struct snode *)malloc(sizeof(struct snode));
				newNode->ip_address = (char *)malloc((sizeof(char) * 100) + 1);
				newNode->fqdn = (char *)malloc((sizeof(char) * 100) + 1);
				strncpy(newNode->ip_address, buffer[i], strlen(buffer[i]) + 1);
				// get the FQDN from the domain name server
				IP_Address = buffer[i];
				inet_aton(IP_Address, &addr);
				he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
				if (he != NULL) { strncpy(newNode->fqdn, he->h_name, strlen(he->h_name) + 1); }
				else { strncpy(newNode->fqdn, buffer[i], strlen(buffer[i]) + 1); }
				newNode->index = mostRecentIndex;
				//printf("IP: %s\tFQDN: %s\t%d\n", buffer[i], newNode->fqdn, newNode->index);

				newnode1 = (struct tempNode *)malloc(sizeof(struct tempNode));
				newnode1->ip_address = (char *)malloc(sizeof(char) * (strlen(buffer[i]) + 1));
				newnode1->fqdn = (char *)malloc(sizeof(char) * (256));
				bzero(newnode1->fqdn, sizeof(char) * 256);
				//newnode1->fqdn = (char *)malloc(sizeof(char) * (strlen(newNode->fqdn) + 1));
				strncpy(newnode1->ip_address, buffer[i], strlen(buffer[i]));
				strncpy(newnode1->fqdn, newNode->fqdn, strlen(newNode->fqdn));
				newnode1->isHit = false;
				newnode1->next = NULL;
				if (listtail1 == NULL) { listhead1 = listtail1 = newnode1; }
				else
				{
					listtail1->next = newnode1;
					listtail1 = newnode1;
				}

				newNode->next = NULL;
				if (tailNode == NULL) { headNode = tailNode = newNode; }
				else
				{
					tailNode->next = newNode;
					tailNode = newNode;
				}
			}
			continue_loop:
			//printf("next iteration\n");
			continue;
		}
		//printf("outside of lookup for loop\n");
		pthread_mutex_unlock(&lookupLock);
		//printf("unlocking mutex in lookup\n");
		processing2 = false;
	}
	//printf("closing lookup1 thread\n");
	return NULL;
}

// Exits from program if input parameters are invalid
void validateInput(int a, int b, int c, int d)
{
	bool invalidInput = false;
	if (a <= 0 || a > 1000)
	{
		printf("numLines must be greater than 0 and no more than 1000.\n");
		invalidInput = true;
	}
	if (b <= 0 || b > 10000)
	{
		printf("cacheSize must be greater than 0 and no more than 10000.\n");
		invalidInput = true;
	}
	if (c < 0)
	{
		printf("fileDelay must be greater than or equal to 0.\n");
		invalidInput = true;
	}
	if (d < 0)
	{
		printf("threadDelay must be greater than or equal to 0.\n");
		invalidInput = true;
	}
	if (invalidInput) { exit(1); }
}

// Returns a boolean expression to determine whether 
// the IP address in the file should be ignored or not
bool isValidIpAddress(char *input)
{
	if (strlen(input) > 20)
	{
		return false;
	}
	int i;
	int count = 0;
	for (i = 0; i < strlen(input); i++)
	{
		if (input[i] == '.')
		{
			count++;
		}
	}
	if (count == 3)
	{
		return true;
	}
	else
	{
		return false;
	}
}


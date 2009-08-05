#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <sys/wait.h>
#include <signal.h>

#define MYPORT 3490	// the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int gtemp;
pthread_mutex_t mutex;

int open_port(void)
{
	struct termios options;
	int fd; /* File descriptor for the port */
	fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1)
		perror("open_port: Unable to open /dev/ttyS0 - ");
	else
		fcntl(fd, F_SETFL, 0);

	tcgetattr(fd, &options);
	cfsetispeed(&options, B1200);
	cfsetospeed(&options, B1200);

	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tcsetattr(fd, TCSANOW, &options);
	return (fd);
}


int close_port(int fd)
{
	close(fd);
}

int readtemperature(int fd, int *t)
{
        char buffer[255];  /* Input buffer */
        char *bufptr;      /* Current char in buffer */
        int  nbytes = 0;       /* Number of bytes read */

	buffer[0] = '\0';
        bufptr = &buffer[0];
        while (nbytes < 254)
        {
                nbytes += read(fd,bufptr, 1);
                if (*bufptr == '\n')
                        break;
                bufptr++;
        }
       /* nul terminate the string and see if we got an OK response */
        *bufptr = '\0';
        if (sscanf(buffer, " Temperatur:  %i", t))
		return 0;
	else
		return -1;
}
void *readtempthread(void *arg)
{
	int intemp = 0;
	int f;
        f = open_port();
	while (1)
	{
	        if (!readtemperature(f,&intemp))
		{
			pthread_mutex_lock (&mutex);
			gtemp = intemp;
			pthread_mutex_unlock (&mutex);
		}
	        else
	                perror("parse error\n");
	}	
	        close_port (f);
	
	return NULL;
}

void *printtempthread(void *arg)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_in6 my_addr;	// my address information
	struct sockaddr_in6 their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;

	if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) 
	{
		perror("socket");
		exit(1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
	{
		perror("setsockopt");
		exit(1);
	}
	
	my_addr.sin6_family = AF_INET6;		 // host byte order
	my_addr.sin6_port = htons(MYPORT);	 // short, network byte order
	my_addr.sin6_addr = in6addr_any; // automatically fill with my IP
	// memset(my_addr.sin6_zero, '\0', sizeof my_addr.sin6_zero);

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) 
	{
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) 
	{
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) 
	{
		perror("sigaction");
		exit(1);
	}

	while(1) 
	{  // main accept() loop
		sin_size = sizeof their_addr;
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) 
		{
			perror("accept");
			continue;
		}

		if (!fork()) 
		{ // this is the child process
			char cbuffer[100];
			close(sockfd); // child doesn't need the listener
			pthread_mutex_lock (&mutex);
			sprintf(cbuffer,"%i",gtemp);
			pthread_mutex_unlock (&mutex);

			if (send(new_fd, &cbuffer, 14, 0) == -1)
				perror("send");
			close(new_fd);
			exit(0);
		}

		close(new_fd);  // parent doesn't need this

	}
}
int main (void)
{
	int i;
	int n = 2;
	pthread_t *threads;
	pthread_attr_t pthread_custom_attr;

	threads=(pthread_t *)malloc(n*sizeof(*threads));
	pthread_attr_init(&pthread_custom_attr);
	pthread_mutex_init (&mutex, NULL);

	pthread_create(&threads[0], &pthread_custom_attr, readtempthread, (void *)0);

	pthread_create(&threads[1], &pthread_custom_attr, printtempthread, (void *)0);

	pthread_join(threads[0],NULL);
	pthread_join(threads[1],NULL);
}

#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include "zlib.h"

#define PORT 'p'
#define COMPRESS 'c'
#define fd0 0
#define fd1 1
#define fd2 2
#define BUFFER 2000
#define lf '\n'
#define cr '\r'
#define cr_lf "\r\n"
#define KILL_SIG 3
#define EOF_SIG 4

int n, m, i;
unsigned char buf[BUFFER];
int cpid;
int status;

int to_child_pipe[2];
int from_child_pipe[2];

void handler(void);

void systemCallErr(char syscall[]);
void configChild();
int to_shell(int fd, int sfd);
int compressed_from_client(int fd, int sfd);
int from_shell(int fd, int sfd);
int compressed_to_client(int fd, int sfd);
void processComplete(int status);

int inf(unsigned char out_buf[], unsigned char in_buf[], int b);
int def(unsigned char out_buf[], unsigned char in_buf[], int b);

int main(int argc, char* argv[])
{
  //Socket Varibles
  int socketFD, newSocketFD, portno;
  struct sockaddr_in serv_addr;

  //Process options
  static struct option long_options[] = {
    {"port", required_argument, 0, PORT},
    {"compress", optional_argument, 0, COMPRESS},
    {0,0,0,0},
  };

  int opt = 0;
  int compressFlag = 0;
  portno = -1;
    
  while ((opt = getopt_long(argc, argv, "", long_options, 0)) != -1)
    {
      switch (opt)
	{
	case PORT:
	  portno = atoi(optarg);
	  break;
	case COMPRESS:
	  compressFlag = 1;
	  break;
	default:
	  fprintf(stderr, "usage: ./lab1b-server --port=VAL [--compress]");
	  exit(1);
	  break;
	}
    }

  if (portno == -1)
    {
      fprintf(stderr, "Positive port number required!");
      exit(1);
    }

  //Create socket  
  socketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFD == -1)
    systemCallErr("socket");
      
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = portno;

  if (bind(socketFD, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1)
    systemCallErr("bind");

  if (listen(socketFD, 1) == -1)
    systemCallErr("listen");

  newSocketFD = accept(socketFD, (struct sockaddr*) NULL, NULL);
  if (newSocketFD == -1)
    systemCallErr("accept");


  //Create pipes
  if (pipe(to_child_pipe) == -1 || pipe(from_child_pipe) == -1)
    systemCallErr("pipe");

  //Create child process
  cpid = fork();
  if (cpid == -1)
    systemCallErr("fork");
  else if (cpid == 0)
    configChild();
  else {
    //Polling
    if (close(to_child_pipe[0]) == -1 || close(from_child_pipe[1]) == -1)
      systemCallErr("close");

    //Poll for input in parent
    struct pollfd poll_list[2];

    //poll_list[0] - received from client
    poll_list[0].fd = newSocketFD;
    poll_list[0].events = POLLIN | POLLHUP | POLLERR;
    //  poll_list[0].revents = 0;

    //poll_list[1] - received from child/shell
    poll_list[1].fd = from_child_pipe[0];
    poll_list[1].events = POLLIN | POLLHUP | POLLERR;
    //    poll_list[1].revents = 0;

    int sig = 0;
    while (1)
      {
	if (poll(poll_list, 2, 0) == -1)
	  systemCallErr("poll");

	//check if received input from client
	if (poll_list[0].revents & POLLIN)
	  {
	    if (!compressFlag)
	      sig = to_shell(to_child_pipe[1], newSocketFD);
	    else
	      sig = compressed_from_client(to_child_pipe[1], newSocketFD);
	    //check for KILL SIGNAL
	    if (sig == KILL_SIG)
	      {
		if (kill(cpid, SIGINT) == -1)
		  systemCallErr("kill");
	      }
	    //check for EOF SIGNAL
	    if (sig == EOF_SIG)
	      {
		if (close(to_child_pipe[1]) == -1)
		  systemCallErr("close");
		signal(SIGPIPE, SIG_IGN);
		break;
	      }
	  }
	if (poll_list[1].revents & POLLIN)
	  {
	    if (!compressFlag)
	      sig = from_shell(from_child_pipe[0], newSocketFD);
	    else
	      sig = compressed_to_client(from_child_pipe[0], newSocketFD);
	    if (sig == EOF_SIG)
	      break;
	    if (sig == -1)
	      close(from_child_pipe[0]);
	  }
	if ((poll_list[1].revents & POLLERR) ||
	    (poll_list[1].revents & POLLHUP) ||
	    (poll_list[0].revents & POLLERR) ||
	    (poll_list[0].revents & POLLHUP))
	  break;
      }
    if (waitpid(cpid, &status, 0) == -1)
      systemCallErr("waitpid");
    processComplete(status);
  }
  exit(0);
}

int compressed_to_client(int fd, int sfd)
{
  unsigned char frShell[BUFFER];
  n = read(fd, frShell, BUFFER);
  if (n == -1)
    systemCallErr("read");
  if (n == 0)
    return -1;
  if (n != 0)
    {
      unsigned char cBuf[BUFFER];
      m = def(cBuf, frShell, n);
      if (write(sfd, cBuf, m) != m)
	systemCallErr("write");

      fprintf(stderr, "sent to client");
      for (i = 0; i < n ; i++)
	{
	  if (frShell[i] == EOF_SIG)
	    return EOF_SIG;
	}
    }
  return 0;
}

int def(unsigned char out_buf[], unsigned char in_buf[], int b)
{
  z_stream strm;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
    exit(1);

  strm.avail_in = b;
  strm.next_in = in_buf;
  strm.avail_out = BUFFER;
  strm.next_out = out_buf;
  do {
    deflate(&strm, Z_SYNC_FLUSH);
  } while (strm.avail_in > 0);

  deflateEnd(&strm);
  return (BUFFER-strm.avail_out);
}

int compressed_from_client(int fd, int sfd)
{
  unsigned char frClient[BUFFER];
  n = read(sfd, frClient, BUFFER);
  if (n == -1)
    systemCallErr("read");
  if (n != 0)
    {
      unsigned char toShell[BUFFER];
      m = inf(toShell, frClient, n);
      for (i = 0; i < m; i++)
	{
	  if (toShell[i] == cr || toShell[i] == lf)
	    {
	      if (write(fd, "\n", 1) == -1)
		systemCallErr("write");
	    }
	  else if (toShell[i] == KILL_SIG) //detect ^C
	    return KILL_SIG;
	  else if (toShell[i] == EOF_SIG) //detect ^D
	    return EOF_SIG;
	  else
	    {
	      if (write(fd, &toShell[i], 1) == -1)
		systemCallErr("write");
	    }
	}
      
    }
  return 0;
}

int inf(unsigned char out_buf[], unsigned char in_buf[], int b)
{
  z_stream strm;
  
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  if (inflateInit(&strm) != Z_OK)
    exit(1);

  strm.avail_in = b;
  strm.next_in = in_buf;
  strm.avail_out = BUFFER;
  strm.next_out = out_buf;

  do
    {
      inflate(&strm, Z_SYNC_FLUSH);
    } while (strm.avail_in > 0);

  inflateEnd(&strm);
  return (BUFFER - strm.avail_out);
}

int to_shell(int fd, int sfd)
{
  n = read(sfd, buf, BUFFER);
  if (n == -1)
    systemCallErr("read");
  for (i = 0; i < n; i++)
    {
      if (buf[i] == cr || buf[i] == lf)
	{
	  if (write(fd, "\n", 1) == -1)
	    systemCallErr("write");
	}
      else if (buf[i] == KILL_SIG) //detect ^C
	return KILL_SIG;
      else if (buf[i] == EOF_SIG) //detect ^D
	return EOF_SIG;
      else
	{
	  if (write(fd, &buf[i], 1) == -1)
	    systemCallErr("write");
	}
    }
  return 0;
}

int from_shell(int fd, int sfd)
{
  n = read(fd, buf, BUFFER);
  if (n == -1)
    systemCallErr("read");
  if (n == 0)
    return -1;
  
  for (i = 0; i < n; i++)
    {
      if (buf[i] == lf || buf[i] == cr)
	{
	  if (write(sfd, cr_lf, 2) == -1)
	    systemCallErr("write");
	}
      else if (buf[i] == EOF_SIG)
	      return EOF_SIG;
      else
	{
	  if (write(sfd, &buf[i], 1) == -1)
	    systemCallErr("write");
	}
    }
  return 0;
}
void configChild(void)
{
  //child process reads data from parent process                               
  if (close(to_child_pipe[1]) == -1)
    systemCallErr("close");
  if (dup2(to_child_pipe[0], fd0) == -1)
    systemCallErr("dup2");
  if (close(to_child_pipe[0]) == -1)
    systemCallErr("close");

  //child process sends data to parent process
  if (close(from_child_pipe[0]) == -1)
    systemCallErr("close");
  if (dup2(from_child_pipe[1], fd1) == -1)
    systemCallErr("dup2");
  if (dup2(from_child_pipe[1], fd2) == -1)
    systemCallErr("dup2");
  if (close(from_child_pipe[1]) == -1)
    systemCallErr("close");

  char* args[2];
  args[0] = "/bin/bash";
  args[1] = NULL;

  if (execvp(args[0], args) == -1)
    systemCallErr("execvp");
}

void processComplete(int status)
{
  int signalNum = WTERMSIG(status);
  int statusNum = WEXITSTATUS(status);
  fprintf(stderr, "SHELL EXIT SIGNAL=%i STATUS=%i", signalNum, statusNum);
}

void systemCallErr(char syscall[])
{
  fprintf(stderr, "Error with %s system call\n", syscall);
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
}

void handler(void)
{
  if (waitpid(cpid, &status, 0) == -1)
      systemCallErr("waitpid");
  processComplete(status);
  exit(0);
}

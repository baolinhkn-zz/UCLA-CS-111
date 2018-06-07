#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#include "zlib.h"

#define PORT 'p'
#define LOG 'l'
#define COMPRESS 'c'
#define lf '\n'
#define cr '\r'
#define cr_lf "\r\n"
#define terminalFD 0
#define fd0 0
#define fd1 1
#define BUFFER 2000

int n, m, i;
unsigned char buf[BUFFER];
struct termios oldtio, newtio, temp;

void setTermAttr(void);
void reset(void);
void systemCallErr(char syscall[]);
void to_server(int sfd, int lfd);
void compressed_to_server(int sfd, int lfd);
void from_server(int sfd, int lfd);
void compressed_from_server(int sfd, int lfd);
void logFile(int lfd, unsigned char buf[], int n, int flag);

int def(unsigned char out_buf[], unsigned char in_buf[], int b);
int inf(unsigned char out_buf[], unsigned char in_buf[], int b);

int main(int argc, char* argv[])
{
  //Socket Variables
  int socketFD, portno;
  struct sockaddr_in serv_addr;
  
  //Process Options
  static struct option long_options[] = {
    {"port", required_argument, 0, PORT},
    {"log", required_argument, 0, LOG},
    {"compress", no_argument, 0, COMPRESS},
    {0, 0, 0, 0},
  };

  int opt;
  int logFD = -1;
  int compressFlag = 0;
  portno = -1;

  while ((opt = getopt_long(argc, argv, "", long_options, 0)) != -1)
    {
      switch(opt)
	{
	case PORT:
	  portno = atoi(optarg);
	  break;
	case LOG:
	  logFD = open(optarg, O_WRONLY | O_CREAT| O_TRUNC, 0666);
	  if (logFD == -1)
	    systemCallErr("open");
	  break;
	case COMPRESS:
	  compressFlag = 1;
	  break;
	default:
	  fprintf(stderr, "usage: client --port=VAL [--log=FILE --compress]");
	  exit(1);
	  break;
	}
    }

  if (portno == -1)
    {
      fprintf(stderr, "Positive port number required!");
      exit(1);
    }
  //set terminal settings
  atexit(reset);
  setTermAttr();
  
  //create socket
  socketFD = socket(AF_INET, SOCK_STREAM, 0);

  if (socketFD == -1)
    systemCallErr("socket");

  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = portno;

  if (connect(socketFD, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    systemCallErr("connect1");
    
  //poll for IO from keyboard and shell
  struct pollfd poll_list[2];

  
  //poll_list[0] - keyboard input
  poll_list[0].fd = fd0;
  poll_list[0].events = POLLIN | POLLHUP | POLLERR;
  poll_list[0].revents = 0;

  //poll_list[1] - socket output
  poll_list[1].fd = socketFD;
  poll_list[1].events = POLLIN | POLLHUP | POLLERR;
  poll_list[1].revents = 0;

  while (1)
    {
      if (poll(poll_list, 2, 0) == -1)
	systemCallErr("poll");

      //received from keyboard
      if (poll_list[0].revents & POLLIN)
	{
	  //send to server
	  if (!compressFlag)
	    to_server(socketFD, logFD);
	  else
	    compressed_to_server(socketFD, logFD);
	}
      
      //received from socket
      if (poll_list[1].revents & POLLIN)
	{
	  //sent from server
	  if (!compressFlag)
	    from_server(socketFD, logFD);
	  else
	    compressed_from_server(socketFD, logFD);
	}

      //error from socket
      if (poll_list[1].revents & (POLLHUP + POLLERR))
	break;
    }
  exit(0);
}

void compressed_from_server(int sfd, int lfd)
{
  unsigned char frServer[BUFFER];
  n = read(sfd, frServer, BUFFER);
  if (n == 0)
    exit(0);
  if (lfd != -1)
    logFile(lfd, frServer, n, 0);
  if (n == -1)
    systemCallErr("read");
  if (n != 0)
    {
      unsigned char toKey[BUFFER];
      m = inf(toKey, frServer, n);
      for (i = 0; i < m; i++)
	{
	  if (toKey[i] == lf || toKey[i] == cr)
	    {
	      if (write(fd1, cr_lf, 2) == -1)
		systemCallErr("write");
	    }
	  else
	    {
	      if (write(fd1, &toKey[i], 1) == -1)
		systemCallErr("write");
	    }
	}
    }
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

void compressed_to_server(int sfd, int lfd)
{
  unsigned char toServ[BUFFER];
  n = read(fd0, toServ, BUFFER);
  if (n == -1)
    systemCallErr("read");
  else
    {
      for (i = 0; i < n; i++)
	{
	  if (toServ[i] == lf || toServ[i] == cr)
	    {
	      if (write(fd1, cr_lf, 2)== -1)
		systemCallErr("write");
	    }
	  else
	    {
	      if (write(fd1, &toServ[i], 1) == -1)
		systemCallErr("write");
	    }
	}
      unsigned char cBuf[BUFFER];
      m = def(cBuf, toServ, n);

      if (write(sfd, cBuf, m) != m)
	systemCallErr("write");
      if (lfd != -1)
	logFile(lfd, cBuf, m, 1);
    }
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

void logFile(int lfd, unsigned char buf[], int n, int flag)
{
  if (flag)
    {
      if (write(lfd, "SENT ", 5) == -1)
	systemCallErr("write");
    }
  else
    {
      if (write(lfd, "RECEIVED ", 9) == -1)
	systemCallErr("write");
    }
  char intBuf[BUFFER];
  int len = sprintf(intBuf, "%d", n);
  if (write(lfd, intBuf, len) != len)
    systemCallErr("write");
  if (write(lfd, " bytes: ", 8) == -1)
    systemCallErr("write");
  if (write(lfd, buf, n) == -1)
    systemCallErr("write");
  if (write( lfd, "\n", 1) == -1)
    systemCallErr("write");
}

void to_server(int sfd, int lfd)
{
  n = read(fd0, buf, BUFFER);
  if (n == -1)
    systemCallErr("read");
  else
    {
      for (i = 0; i < n; i++)
	{
	  if (buf[i] == lf || buf[i] == cr)
	    {
	      if (write(fd1, cr_lf, 2)== -1)
		systemCallErr("write");
	    }
	  else
	    {
	      if (write(fd1, &buf[i], 1) == -1)
		systemCallErr("write");
	    }
	  if (write(sfd, &buf[i], 1) == -1)
	    systemCallErr("write");
	}
    }
  if (n != 0 && lfd != -1)
    logFile(lfd, buf, n, 1);

}

void from_server(int sfd, int lfd)
{
  n = read(sfd, buf, BUFFER);
  if (n == 0)
    exit(0);
  if (n == -1)
    systemCallErr("read");
  else
    {
      for (i = 0; i < n; i++)
	{
	  if (write(fd1, &buf[i], 1) == -1)
	    systemCallErr("write");
	}
      if (lfd != -1)
	logFile(lfd, buf, n, 0);
    }
}



void setTermAttr(void)
{
  if (tcgetattr(terminalFD, &oldtio) < 0)
    systemCallErr("tcgetattr");

  if (tcgetattr(terminalFD, &newtio) < 0)
    systemCallErr("tcgetattr");

  newtio.c_iflag = ISTRIP;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;

  //SETTING NEW ATTRIBUTES
  if (tcsetattr(terminalFD, TCSANOW, &newtio) < 0)
    systemCallErr("tcsetattr");

  //CHECKING IF ATTRIBUTES HAVE BEEN SET
  if (tcgetattr(terminalFD, &temp) < 0)
    systemCallErr("tcgetattr");
  if (temp.c_iflag != ISTRIP || temp.c_oflag != 0 || temp.c_lflag != 0)
    systemCallErr("tcgetattr");
}

void reset(void)
{
  //RESETTING ATTRIBUTES

  if (tcsetattr(terminalFD, TCSANOW, &oldtio) < 0)
    systemCallErr("tcsetattr");

  //CHECKING IF ATTRIBUTES HAVE BEEN RESET
  if (tcgetattr(terminalFD, &temp) < 0)
    systemCallErr("tcgetattr");

  if (temp.c_iflag != oldtio.c_iflag || temp.c_oflag != oldtio.c_oflag || temp.c_lflag != oldtio.c_lflag)
    {
      fprintf(stderr, "Error setting new terminal attributes");
    }
}

void systemCallErr(char syscall[])
{
  fprintf(stderr, "Error with %s system call\n", syscall);
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
}

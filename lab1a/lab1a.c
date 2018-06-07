#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <getopt.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#define terminalFD 0
#define BUFFER 2000
#define cr_lf "\r\n"
#define lf '\n'
#define cr '\r'
#define SHELL 's'
#define CTRL_D "^D"
#define CTRL_C "^C"
#define fd0 0
#define fd1 1
#define fd2 2
#define KILL_SIG 3
#define EOF_SIG 4

struct termios oldtio, newtio, temp;
int n, m, i;
char buf[BUFFER];
int cpid;
int flag;
int status;

int to_child_pipe[2];
int from_child_pipe[2];
void reset(void);
void readKeyboardInput(void);
void IOErrorCheck(int m);
void systemCallErr(char syscall[]);
void handler(int signum);
void setTermAttr(void);
void processComplete(int status);
void configChild(void);

int  to_shell(int fd);
int from_shell(int fd);

int main(int argc, char* const argv[])
{
  flag = 1;

  atexit(reset);
  setTermAttr();

  //DETECTING --shell FLAG
  static struct option long_options[] = {
    {"shell", no_argument, 0, SHELL},
    {0, 0, 0, 0}
  };
  
  int opt;
  int shellFlag = 0;

  while ((opt = getopt_long(argc, argv, "", long_options, 0)) != -1)
    {
      switch (opt)
	{
	case SHELL:
	  shellFlag = 1;
	  break;
	default:
	  fprintf(stderr, "usage: lab1 [--shell]");
	  exit(1);
	  break;
	}
    }
 
  if (shellFlag)
    {

      if (signal(SIGINT, handler) == SIG_ERR ||  signal(SIGPIPE, handler) == SIG_ERR)
	  systemCallErr("signal");
            
      if (pipe(to_child_pipe) == -1 || pipe(from_child_pipe) == -1)
	  systemCallErr("pipe");

      cpid = fork();
      if (cpid == -1)
	  systemCallErr("fork");
      else if (cpid == 0)  //child process
	  configChild();
      else if (cpid > 0)  //parent process
	{
	  if (close(to_child_pipe[0]) == -1)	  // parent process sends data to child process
	    systemCallErr("close");
	  if (close(from_child_pipe[1]) == -1) 	  //parent process reads data from child process
	    systemCallErr("close");

    	  //POLL FOR INPUT IN PARENT
	  struct pollfd poll_list[2];
	  
	  //poll_list[0] - keyboard input
	  poll_list[0].fd = fd0;
	  poll_list[0].events = POLLIN | POLLHUP | POLLERR;
	  poll_list[0].revents = 0;

	  //poll_list[1] - shell output
	  poll_list[1].fd = from_child_pipe[0];
	  poll_list[1].events = POLLIN | POLLHUP | POLLERR;
	  poll_list[1].revents = 0;

	  int sig = 0;
	  while (1)
	    {
	      if (poll(poll_list, 2, 0) == -1)
		  systemCallErr("poll");
	      //check if received keyboard input
	      if (poll_list[0].revents & POLLIN)
		{
		  //write out to shell
		  sig = to_shell(to_child_pipe[1]);
		  if (sig == KILL_SIG)
		    {
		      if (kill(cpid, SIGINT) == -1)
			  systemCallErr("kill");
		    }
		  if (sig == EOF_SIG) //EOF received, close pipe
		    {		   
		      if (close(to_child_pipe[1]) == -1)
			systemCallErr("close");
		    }
		}
	      //check if received output from shell
	      if (poll_list[1].revents & POLLIN)
		{
		  //receive data from shell
		  sig = from_shell(from_child_pipe[0]);
		  if (sig == EOF_SIG)
		    {
		      if (close(to_child_pipe[1]) == -1)
			systemCallErr("close");
		      break;
		    }
		}
	      //polling error from the shell received
	      if (poll_list[1].revents & (POLLHUP + POLLERR))
		  break;
	    }
	  if (waitpid(cpid, &status, 0) == -1)
	    systemCallErr("waitpid");
	  processComplete(status);      
 	}
    }
  else
      readKeyboardInput();
  exit(0);
}

int to_shell(int fd)
{
  n = read(fd0, buf, BUFFER);
  if (n == -1)
    {
      systemCallErr("read");
    }
  if (n > 0)
    {
      for (i = 0; i < n; i++)
	{
	  if (buf[i] == cr || buf[i] == lf)
	    {
	      m = write(1, cr_lf, 2);
	      if (m != 2)
		  systemCallErr("write");
	      m = write(fd, "\n", 1);
	      if (m != 1)
		  systemCallErr("write");
	    }
	  else if (buf[i] == KILL_SIG)//detect ^c, KILL
	    {
	      m = write(1, CTRL_C, 2);
	      if (m != 2)
		systemCallErr("write");
	      return KILL_SIG;
	      //send signal to shell
	    }
	  else if (buf[i] == EOF_SIG) //detect ^D, EOF
	    {
	      m = write(1, CTRL_D , 2);
	      if (m != 2)
		  systemCallErr("write");
	      return EOF_SIG;
	    }
	  else
	    {
	      m = write(1, &buf[i], 1);
	      if (m != 1)
		  systemCallErr("write");
	      m = write(fd, &buf[i], 1);
	      if (m != 1)
		  systemCallErr("write");
	    }      
	}
    }
  return 0;
}

int from_shell(int fd)
{
  n = read(fd, buf, BUFFER);
  if (n == -1)
      systemCallErr("read");
  for (i = 0; i < n; i++)
    {
      if (buf[i] == lf)
	{
	  m = write(fd1, cr_lf, 2);
	  if (m != 2)
	      systemCallErr("write");
	}
      else if (buf[i] == EOF_SIG) //detect ^D, EOF
	{
	  m = write(fd1, CTRL_D , 2);
	  if (m != 2)
	      systemCallErr("write");
	  return EOF_SIG;
	}
      else
	{
	  m = write(fd1, &buf[i], 1);
	  if (m != 1)
	      systemCallErr("write");
	}
    }
  return 0;
}

void readKeyboardInput(void)
{
  while ((n = read(0, buf, BUFFER)) > 0)
    {
      for (i = 0; i < n; i++)
	{
	  if (buf[i] == cr || buf[i] == lf)
	    {
	      m = write(1, cr_lf, 2);
	      if (m != 2)
		  systemCallErr("write");
	    }
	  else if (buf[i] == 0x004) //detect ^D, EOF
	    {
	      m = write(1, CTRL_D , 2);
	      if (m != 2)
		  systemCallErr("write");
	      exit(0);
	    }
	  else
	    {
	      m = write(1, &buf[i], 1);
	      if (m != 1)
		  systemCallErr("write");
	    }
	}
    }
  if (n != 0)
    {
      systemCallErr("write");
    }
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

void reset(void)
{
  waitpid(cpid, &status, WNOHANG);
  if (flag)
    {
      //do nothing if attributes were never set
      return;
    }
  //RESETTING ATTRIBUTES
  if (tcsetattr(terminalFD, TCSANOW, &oldtio) < 0)
      systemCallErr("tcsetattr");

  //CHECKING IF ATTRIBUTES HAVE BEEN RESET
  if (tcgetattr(terminalFD, &temp) < 0)
      systemCallErr("tcgetattr");

  if (temp.c_iflag != oldtio.c_iflag || temp.c_oflag != oldtio.c_oflag || temp.c_lflag != oldtio.c_lflag)
    {
      fprintf(stderr, "%d, %d, %d", temp.c_iflag, temp.c_oflag, oldtio.c_lflag);
      fprintf(stderr, "Error setting new terminal attributes");
    }
}

void systemCallErr(char syscall[])
{
  fprintf(stderr, "Error with %s system call\n", syscall);
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
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
  flag = 0;
}

void processComplete(int status)
{
  int signalNum = WTERMSIG(status);
  int statusNum = WEXITSTATUS(status);
  fprintf(stderr, "SHELL EXIT SIGNAL=%i STATUS=%i", signalNum, statusNum);
}

void handler(int signum)
{
  if (signum == SIGINT)
    {
      if (kill(cpid, SIGINT) == -1)
	{
	  systemCallErr("kill");
	}
    }
  if (signum == SIGPIPE)
    {
      close(to_child_pipe[1]);
    }
}

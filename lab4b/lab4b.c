#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <poll.h>
#include <math.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>
#include <signal.h>
#include <errno.h>

#define PERIOD 'p'
#define SCALE 's'
#define LOG 'l'
#define STDOUT 1
#define STDIN 0
#define BUFFER 2000

int period;
int scaleFlag;
char scale;
int shutdownFlag;
int logFD;
int logFlag;

const int B = 4275;
const int R0 = 100000;

sig_atomic_t volatile run_flag = 1;

void getParameters(int argc, char* const argv[]);
void write_log(int fd, float temp);
void systemCallErr(char syscall[]);
int parseCommand(char command[]);

void do_when_button_pressed()
{
  shutdownFlag=1;
}

int main(int argc, char* const argv[])
{
  scaleFlag = 0;
  period = 1;
  scale = 'F';
  logFD = -1;
  logFlag = 0;
  
  getParameters(argc, argv);
  
  int val;
  int parseRes;

  mraa_aio_context temperatureSensor;
  mraa_gpio_context button;

  button = mraa_gpio_init(73);
  temperatureSensor = mraa_aio_init(1);

  mraa_gpio_dir(button, MRAA_GPIO_IN);
  mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &do_when_button_pressed, NULL);
 
  shutdownFlag = 0;

  struct pollfd poll_list[1];
  poll_list[0].fd = STDIN;
  poll_list[0].events = POLLIN | POLLHUP | POLLERR;
  poll_list[0].revents = 0;

  while (run_flag)
    {

      if (poll(poll_list, 1, 0) == -1)
	  systemCallErr("poll");

      if (poll_list[0].revents & POLLIN)
	{
	  char command[BUFFER];
	  fgets(command, BUFFER, stdin);
	  parseRes = parseCommand(command);
	  if (parseRes == 0)
	    {
	      if (logFD != -1)
		dprintf(logFD, "%s" ,command);
	    }
	}

      val = mraa_aio_read(temperatureSensor);
      float T = 1023.0/val-1.0;
      T = R0*T;

      float temperature = 1.0/(log(T/R0)/B+1/298.15)-273.15;

      if (scale == 'F')
	{
	  //convert to fahrenheit
	  temperature = temperature * 9/5 + 32;
	}
      if (logFlag == 0)
	{
	  write_log(STDOUT, temperature);      
	  if (logFD != -1)
	    write_log(logFD, temperature);
	}
      if (shutdownFlag)
	{
	  if (logFlag == 1)
	    {
	      write_log(STDOUT, temperature);
	      if (logFD != -1)
		write_log(logFD, temperature);
	    }
	
	  break;
	}

      usleep(period*1000000);
    }

  mraa_gpio_close(button);
  mraa_aio_close(temperatureSensor);

  return 0;
}

int parseCommand(char command[])
{
  char buf[BUFFER] = {0};
  unsigned int len;
  if (strcmp(command, "SCALE=F\n") == 0)
    scale = 'F';
  else if (strcmp(command, "SCALE=C\n") == 0)	   
    scale = 'C';
  else if (command[0] == 'P')
    {
      unsigned int i = 0;
      len = strlen(command);
      if (len < 9)
	{
	  fprintf(stderr, "PERIOD=[value]\n is the correct usage");
	  exit(1);
	}
      for (i = 0; i < len; i++)
	{
	  if (command[i] == '\n')
	    break;
	}      
      memcpy(buf, command, 7);
      //      buf[7]=0;
      if (strcmp(buf, "PERIOD=") != 0)
	{
	  fprintf(stderr, "PERIOD=[value]\n is the correct usage");
	  exit(1);
	}
      memcpy(buf, &command[7], i-7);
      buf[i-7]=0;
      period  = atoi(buf);
      if (period < -1)
	{
	  fprintf(stderr, "Period must be non-negative!");
	  exit(1);
	}
    }
  else if (strcmp(command, "STOP\n") == 0)
    {
      if (logFlag)
	fprintf(stdout, "Stop received!\n");
      logFlag = 1;
    }
  else if (strcmp(command, "START\n") == 0)
    {
      if (!logFlag)
	fprintf(stdout, "Start received!\n");
      logFlag = 0;
    }
  else if (strcmp(command, "OFF\n") == 0)
    {
      shutdownFlag = 1;
    }
  else if (command[0] == 'L')
    {
      len = strlen(command);
      if (len < 3)
	{
	  fprintf(stderr, "LOG [some message] is the correct usage!");
	  exit(1);
	}
      memcpy(buf, &command[4], len-4); 
      buf[len-4]=0;
      fprintf(stdout, "received message: ");
      fprintf(stdout, "%s", buf);
    }
  else
    {
      fprintf(stderr, "incorrect arguments!");
      exit(1);
    }
  return 0;
}

void write_log(int fd, float temp)
{
  time_t rawtime;
  struct tm *info;

  time(&rawtime);
  
  info = localtime(&rawtime);

  int sec = info->tm_sec;
  int min = info->tm_min;
  int hour = info->tm_hour;
  
  if (hour < 10)
    {
      dprintf(fd, "0");
    }
  dprintf(fd, "%i", hour);
  dprintf(fd, ":");
  if (min < 10)
    {
      dprintf(fd, "0");
    }
  dprintf(fd, "%i", min);
  dprintf(fd, ":");
  if (sec < 10)
    {
      dprintf(fd, "0");
    }
  dprintf(fd, "%i", sec);
  dprintf(fd, " ");
  if (shutdownFlag == 0)
    dprintf(fd, "%.1f\n", temp);
  else if (shutdownFlag == 1)
    dprintf(fd, "SHUTDOWN");
}

void getParameters(int argc, char* const argv[])
{
  static struct option long_options[] = {
    {"period", required_argument, 0, PERIOD},
    {"scale", required_argument, 0, SCALE},
    {"log", required_argument, 0, LOG},
    {0, 0, 0, 0}
  };

  int opt;

  while ((opt=getopt_long(argc, argv, "", long_options, 0)) != -1)
    {
      switch(opt)
	{
	case PERIOD:
	  period=atoi(optarg);
	  if (period < 0)
	    {
	      fprintf(stderr, "Period must be a nonnegative number!");
	      exit(1);
	    }
	  break;
	case SCALE:
	  scale=*optarg;
	  if (scale != 'F' && scale != 'C')
	    {
	      fprintf(stderr, "Correct Usage: --scale=[fc]");
	      exit(1);
	    }
	  break;
	case LOG:
	  logFD = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	  if (logFD == -1)
	    exit(1);
	  break;
	default:
	  fprintf(stderr, "Correct Usage: ./lab4b [--scale=[fc] --period=[value] --log=[fileName]]");
	  exit(1);
	  break;
	}
    }
}

void systemCallErr(char syscall[])
{
   fprintf(stderr, "Error with %s system call\n", syscall);
   fprintf(stderr, "%s\n", strerror(errno));
   exit(1);
}

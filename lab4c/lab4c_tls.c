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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>


#define PERIOD 'p'
#define SCALE 's'
#define LOG 'l'
#define ID 'i'
#define HOST 'h'
#define STDOUT 1
#define STDIN 0
#define BUFFER 2000

int period;
int scaleFlag;
char scale;
int shutdownFlag;
int logFD;
int logFlag;
int id;
char* host = NULL;
char* temp_id = NULL;
int socketFD;
int portno;
SSL_CTX* ctx;
SSL* ssl_object;
char buffer[BUFFER];

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

  shutdownFlag = 0;
  getParameters(argc, argv);

  if (logFD == -1)
    exit(1);

  //Socket Variables
  struct sockaddr_in serv_addr;
  
  socketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFD == -1)
    systemCallErr("socket");


  struct hostent* server;

  server = gethostbyname(host);

  if (server == NULL)
    systemCallErr("gethostbyname");

  memset((char*) &serv_addr, 0, sizeof(serv_addr));
  memcpy((char*) &serv_addr.sin_addr.s_addr, (char*) server->h_addr, server->h_length);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(portno);

  if (connect(socketFD, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    systemCallErr("connect");


  //initialize open ssl
  if (SSL_library_init() == 0)
    {
      fprintf(stderr, "SSL_library_init() error\n");
      exit(2);
    }
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ctx = SSL_CTX_new(SSLv23_client_method());
  if (ctx == NULL)
    {
      fprintf(stderr, "SSL_CTX_new error\n");
      exit(2);
    }
  ssl_object = SSL_new(ctx);
  if (ssl_object == NULL)
    {
      fprintf(stderr, "SSL_new error\n");
      exit(2);
    }
  if (SSL_set_fd(ssl_object, socketFD) == 0)
    {
      fprintf(stderr, "SSL_set_fd error\n");
      exit(2);
    }
  if (SSL_connect(ssl_object) != 1)
    {
      fprintf(stderr, "Error with SSL_connect\n");
     exit(2);
    }

  char id_buf[14];
  id_buf[0] = 'I';
  id_buf[1] = 'D';
  id_buf[2] = '=';
  strcpy(id_buf+3, temp_id);
  id_buf[12] = '\n';
  id_buf[13] = '\0';
  //  snprintf(buffer, BUFFER, "ID=%i\n", id);
  //  SSL_write(ssl_object, buffer, 13);
  SSL_write(ssl_object, id_buf, 13);
  dprintf(logFD, "ID=%i\n", id);

  int val;
  int parseRes;

  mraa_aio_context temperatureSensor;
  temperatureSensor = mraa_aio_init(1);
 
  shutdownFlag = 0;

  struct pollfd poll_list[1];
  poll_list[0].fd = socketFD; //STDIN;
  poll_list[0].events = POLLIN | POLLHUP | POLLERR;
  poll_list[0].revents = 0;


  while (run_flag)
    {
      val = mraa_aio_read(temperatureSensor);
      float T = 1023.0/val-1.0;
      T = R0*T;

      float temperature = 1.0/(log(T/R0)/B+1/298.15)-273.15;

      if (poll(poll_list, 1, 0) == -1)
	  systemCallErr("poll");

      if (poll_list[0].revents & POLLIN)
	{
	  memset((char*) &buffer, 0, BUFFER);
	  int bytes_read = SSL_read(ssl_object, buffer, BUFFER);
	  if (bytes_read < 0)
	    systemCallErr("read");

	  char* comm = strtok(buffer, "\n");
	  while (comm != NULL && bytes_read > 0)
	    {
	      //	      fprintf(stderr, "%s\n", comm);
	      char command[BUFFER];
	      memset((char*) &command, 0, BUFFER);
	      int ret = snprintf(command, BUFFER, "%s\n", comm);
	      dprintf(logFD, "%s", command);
	      if (!(ret >= 0) || !(ret < BUFFER))
		{
		  fprintf(stderr, "error");
		  exit(2);
		}
	      parseRes = parseCommand(command);

	      if (parseRes == -1)
		{
		  shutdownFlag = 1;
		  write_log(logFD, temperature);
		  SSL_shutdown(ssl_object);
		  SSL_free(ssl_object);
		  exit(0);
		}
	      comm = strtok(NULL, "\n");
	    }
	}


      if (scale == 'F')
	{
	  //convert to fahrenheit
	  temperature = temperature * 9/5 + 32;
	}
      if (logFlag == 0)
	{
	    write_log(logFD, temperature);
	}
      if (shutdownFlag ==1)
	{
	  if (logFlag == 1)
	    {
		write_log(logFD, temperature);
	    }
	  break;
	}
      usleep(period*1000000);
    }

  //  mraa_gpio_close(button);
  mraa_aio_close(temperatureSensor);

  exit(1);
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
      logFlag = 1;
    }
  else if (strcmp(command, "START\n") == 0)
    {
      logFlag = 0;
    }
  else if (strcmp(command, "OFF\n") == 0)
    {
      shutdownFlag = 1;
      return -1;
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

  char logBuf[BUFFER];

  if (hour < 10)
    snprintf(logBuf,BUFFER, "0%i", hour);
  else
    snprintf(logBuf,BUFFER, "%i", hour);

  snprintf(logBuf+2,BUFFER, ":");

  if (min < 10)
    snprintf(logBuf+3,BUFFER, "0%i", min);
  else
    snprintf(logBuf+3, BUFFER,"%i", min);

  snprintf(logBuf+5, BUFFER, ":");

  if (sec < 10)
    snprintf(logBuf+6, BUFFER, "0%i", sec);
  else
    snprintf(logBuf+6, BUFFER, "%i", sec);

  snprintf(logBuf+8,BUFFER,  " ");

  if (shutdownFlag == 0 && logFlag == 0)
    {
      snprintf(logBuf+9, BUFFER, "%.1f\n", temp);
      logBuf[14] = '\0';
      SSL_write(ssl_object, logBuf, 14);
    }
  if (shutdownFlag == 1)
    {
      snprintf(logBuf+9,BUFFER, "SHUTDOWN\n");
      logBuf[18] = '\0';
      SSL_write(ssl_object, logBuf, 18);
    }
  dprintf(fd, "%s", logBuf);
}

void getParameters(int argc, char* const argv[])
{
  static struct option long_options[] = {
    {"period", required_argument, 0, PERIOD},
    {"scale", required_argument, 0, SCALE},
    {"log", required_argument, 0, LOG},
    {"id", required_argument, 0, ID},
    {"host", required_argument, 0, HOST},
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
	case ID:
	  temp_id = optarg;
	  id = atoi(optarg);
	  if (ID < 0)
	    exit(1);
	  break;
	case HOST:
	  host = optarg;
	  break;
	default:
	  fprintf(stderr, "Correct Usage: ./lab4b [--scale=[fc] --period=[value] --log=[fileName]]");
	  exit(1);
	  break;
	}
    }
  int index;
  for (index = optind; index < argc; index++)
    portno = atoi(argv[index]);
}

void systemCallErr(char syscall[])
{
   fprintf(stderr, "Error with %s system call\n", syscall);
   fprintf(stderr, "%s\n", strerror(errno));
   exit(1);
}

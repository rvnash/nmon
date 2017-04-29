#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h> 


bool isOpen(int fd)
{
  return (fd > 0);
}

void closePort(int fd)
{
  if (!isOpen(fd)) return;// If we close an already closed port, that's OK.
  close(fd);
}

int openTCPPort(char *ipAddress, int port)
{
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0) 
      printf("ERROR opening socket\n");
  server = gethostbyname(ipAddress);
  if (server == NULL) {
      fprintf(stderr,"ERROR, no such host\n");
      exit(0);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
       (char *)&serv_addr.sin_addr.s_addr,
       server->h_length);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
    printf("ERROR connecting\n");
    exit(0);
  }
  int optval = 1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
    printf("setsockopt()\r\n");
    exit(0);
  }
  return sockfd;
}

int openPort(char *path)
{
    int fd;
    fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (!isOpen(fd)) return 0; // Could not open the port
    flock(fd, LOCK_UN ); // Allow others access to the serial port
    struct termios tty;
    tcgetattr ( fd, &tty );
    tty.c_cflag |= CLOCAL;     // Ignore ctrl lines
    tcsetattr ( fd, TCSANOW, &tty );
    return fd;
}

void setTerminalMode(int fd)
{
    struct termios newTermios;
    tcgetattr(fd, &newTermios);
    newTermios.c_lflag &= ~(ECHO |  ECHONL | ICANON | IEXTEN);
    tcsetattr(fd, TCSANOW, &newTermios);
}

// Returns 1 if a byte was read
// Returns 0 if no byte was available
// Returns negative number on error
int readByte(int fd, char *b)
{
  int status;
  fd_set set;

  FD_ZERO(&set); /* clear the set */
  FD_SET(fd, &set); /* add our file descriptor to the set */
  struct timeval tv = { 0L, 100L }; // Timeout after .1 ms
  status = select(fd + 1, &set, NULL, NULL, &tv);
  if (status == 0) return 0; // Timeout
  if(status < 0) {
    return -1; // Runtime error
  } else {
    int bytesRead = read( fd, b, 1 ); /* there was data to read */
    if (bytesRead != 1) {
      return -2; // Error reading a byte supposedly ready
    } else {
      return 1; // One byte found
    }
  }
}

// Returns 1 if a byte was read
// Returns 0 if no byte was available
// Returns negative number on error
int writeByte(int fd, char *b)
{
  fd_set set;
  int status;

  FD_ZERO(&set); /* clear the set */
  FD_SET(fd, &set); /* add our file descriptor to the set */
  struct timeval tv = { 0L, 100L }; // Timeout after .1 ms
  status = select(fd + 1, NULL, &set, NULL, &tv);
  if(status == -1) {
    return -1; // Runtime error
  } else if(status == 0) {
    return 0; /* a timeout occured */
  } else {
    int result = write( fd, b, 1 ); /* there was data to read */
    if (result != 1) {
      return result; // Error reading a byte supposedly ready
    } else return 1; // One byte found
  }
}

void usage(char*progname)
{
    printf("Usage:\n\t%s serialDevice\nor\n\t%s -i ipaddress [port]\n", progname, progname);
    exit(1);
}

int main( int argc, char **argv )
{
  int fdSerial;
  bool tcpMode;
  char *openName;
  int port;

  if (argc < 2) {
    usage(argv[0]);
  }
  if (!strcmp(argv[1],"-i")) {
    tcpMode = true;
    if (argc == 3) {
      port = 23; // Standard telnet port (which I probably shouldn't be defaulting to because I'm not doing telnet protocol)
    } else if (argc == 4) {
      port = atoi(argv[3]);
    } else {
      usage(argv[0]);
    }
    openName = argv[2];
  } else {
    tcpMode = false;
    if (argc != 2) usage(argv[0]);
    openName = argv[1];
  }
  // Put terminal into raw mode
  setTerminalMode(0);
  if (tcpMode) {
    fdSerial = openTCPPort(openName, port);
  } else {
    fdSerial = openPort(openName);
  }
  if (!isOpen(fdSerial)) {
    printf("Could not open %s, waiting ...\r\n", openName);
  } else {
    printf("Connected to %s\r\n", openName);
  }

  char localb, remoteb;
  int localNumRead, remoteNumRead;
  while (1) {
    localNumRead = readByte(0, &localb);
    if (isOpen(fdSerial)) {
      if (localNumRead == 1) {
        writeByte(fdSerial,&localb);
      }
      remoteNumRead = readByte(fdSerial, &remoteb);
      if (remoteNumRead == 1) {
        write(0,&remoteb,1);
        fsync(0);
      } else if (remoteNumRead < 0) { // Error detected close the port
        printf("...connection lost to %s ...\r\n", openName);
        closePort(fdSerial);
        fdSerial = 0;
      }
    } else {
      if (localNumRead == 1) {
        write(0,&localb,1); // Echo locally if not connected
        fsync(0);
      }
      if (tcpMode) {
        fdSerial = openTCPPort(openName, port);
      } else {
        fdSerial = openPort(openName);
      }
      if (isOpen(fdSerial)) {
        printf("...Reconnected to %s ...\r\n", openName);
      }
    }
  }
}

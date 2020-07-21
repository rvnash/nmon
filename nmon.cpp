#include <sys/file.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <dirent.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

char connectedTo[2048];

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
  if (sockfd < 0) {
      printf("ERROR opening socket\n"); // Should never happen
      exit(0);
  }
  server = gethostbyname(ipAddress);
  if (server == NULL) {
      fprintf(stderr,"ERROR, no such host\n"); // Can't resolve address, won't clear up so exit
      close(sockfd);
      exit(0);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
       (char *)&serv_addr.sin_addr.s_addr,
       server->h_length);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
    close(sockfd);
    return 0;
  }
  int optval = 1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
    close(sockfd);
    return 0;
  }
  return sockfd;
}

int openFilePort(char *path)
{
  int fd;
  fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
  if (!isOpen(fd)) return 0; // Could not open the port
  struct termios tty;
  int result = tcgetattr ( fd, &tty );
  if (result < 0) {
    printf("Error in tcgetattr\n");
    return 0;
  }
  cfmakeraw(&tty);
  result = tcsetattr ( fd, TCSANOW, &tty );
  if (result < 0) {
    printf("Error in tcsetattr\n");
    return 0;
  }
  flock(fd, LOCK_UN ); // Allow others access to the serial port
  strcpy(connectedTo,path);
  printf("Opened %s\n",path);
  return fd;
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

int openPort(char *path)
{
  if (strcmp(path,"usb")==0) {
    DIR *d;
    struct dirent *dir;
    d = opendir("/dev");
    if (d) {
      while ((dir = readdir(d)) != NULL) {
        if (startsWith("tty.usb",dir->d_name) || startsWith("ttyACM",dir->d_name)) {
          char fullPath[2000];
          sprintf(fullPath, "/dev/%s", dir->d_name);
          int fd = openFilePort(fullPath);
          if (isOpen(fd)) {
            closedir(d);
            return fd;
          }
        }
      }
      closedir(d);
    }
    return 0;
  } else {
    return openFilePort(path);
  }
}

void setTerminalMode(int fd)
{
  struct termios newTermios;
  tcgetattr(fd, &newTermios);
  newTermios.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
  tcsetattr(fd, TCSANOW, &newTermios);
}

// Returns N>0 if bytes were read
// Returns 0 if no byte was available
// Returns negative number on error
int readBytes(int fdRemote, int fdLocal, char *b, int max, int &whichFD)
{
  int status;
  fd_set set;

  FD_ZERO(&set); /* clear the set */
  FD_SET(fdRemote, &set); /* add our file descriptor to the set */
  FD_SET(fdLocal, &set); /* add our file descriptor to the set */
  struct timeval tv = { 10L, 0L }; // Timeout after 10seconds
  int maxFD = (fdRemote > fdLocal) ? fdRemote : fdLocal;
  status = select(maxFD + 1, &set, NULL, NULL, &tv);
  if (status == 0) return 0; // Timeout
  if(status < 0) {
    return -1; // Runtime error
  } else {
    if (FD_ISSET(fdRemote, &set)) {
      whichFD = fdRemote;
    } else {
      whichFD = fdLocal;
    }
    int bytesRead = read( whichFD, b, max ); /* there was data to read */
    if (bytesRead <= 0) {
      return -2; // Error reading a byte supposedly ready
    } else {
      return bytesRead; // One bytes found
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
    printf("Usage:\n\t%s serialDevice\nor\n\t%s -i ipaddress [port]\nor\n\t%s usb\n", progname, progname, progname);
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
  setTerminalMode(0);
  fsync(0);
  if (tcpMode) {
    fdSerial = openTCPPort(openName, port);
  } else {
    fdSerial = openPort(openName);
  }
  if (!isOpen(fdSerial)) {
    printf("Could not open %s, waiting ...\r\n", openName);
  } else {
    printf("Connected to %s\r\n", connectedTo);
  }
  // Put terminal into raw mode
  while (1) {
    if (isOpen(fdSerial)) {
      char b[1000];
      int numRead, whichFD;
      numRead = readBytes(fdSerial, 0, b, sizeof(b), whichFD);
      if (numRead > 0) {
        if (whichFD == 0) {
          // Read from the keyboard, write to the serial port
          write(fdSerial,b,numRead);
        } else {
          // Read from the serial port write to the monitor
          write(0,b,numRead);
        }
      } else if (numRead < 0) { // Error detected close the port
        printf("...connection lost to %s ...\r\n", openName);
        closePort(fdSerial);
        fdSerial = 0;
      }
    } else {
      if (tcpMode) {
        fdSerial = openTCPPort(openName, port);
      } else {
        fdSerial = openPort(openName);
      }
      if (isOpen(fdSerial)) {
        printf("...Reconnected to %s ...\r\n", connectedTo);
      } else {
        usleep(10000);
      }
    }
  }
}

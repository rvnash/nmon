#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int fd;
char *serialPath;

void closePort()
{
  if (fd == 0) return; // If we close an already closed port, that's OK.
  close(fd);
  fd = 0;
}

void openPort(char *path)
{
  if (fd != 0) closePort();

  fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    // Could not open the port.
    return;
  }

  // Allow others access to the serial port
  flock(fd, LOCK_UN );

  // Set no flow control
  fcntl(fd, F_SETFL, 0);
}

bool isOpen()
{
  return (fd > 0);
}

int readByte(char *b)
{
  fd_set set;
  int rv;

  FD_ZERO(&set); /* clear the set */
  FD_SET(fd, &set); /* add our file descriptor to the set */
  rv = select(fd + 1, &set, NULL, NULL, NULL);
  if(rv == -1) {
    return -1; // Runtime error
  } else if(rv == 0) {
    return 0; /* a timeout occured */
  } else {
    int bytesRead = read( fd, b, 1 ); /* there was data to read */
    if (bytesRead != 1) {
      return -2; // Error reading a byte supposedly ready
    } else return 1; // One byte found
  }
}

// Should modify this to echo data the other way too.
int main( int argc, char **argv )
{
  if (argc != 2) {
    printf("Usage: %s serialDevice\n", argv[0]);
    exit(1);
  }
  serialPath = argv[1];
  fd = 0;
  openPort(serialPath);
  if (!isOpen()) {
    printf("Could not open %s, waiting ...\n", serialPath);
  }
  while (1) {
    if (!isOpen()) {
      openPort(serialPath);
      if (!isOpen()) {
	usleep(250 * 1000); // Sleep for 0.25 second and try again
      } else {
	printf("...Reconnected to %s ...\n", serialPath);
      }
    } else {
      char b;
      int read = readByte(&b);
      if (read < 0) {
	printf("...connection lost to %s ...\n", serialPath);
	closePort();
      } else if (read == 1) {
	printf("%c",b);
      } else {
      }
    }
  }
}

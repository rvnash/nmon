#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <curses.h>

int fdSerial=0;

void closePort()
{
  if (fdSerial == 0) return; // If we close an already closed port, that's OK.
  close(fdSerial);
  fdSerial = 0;
}

void openPort(char *path)
{
  if (fdSerial != 0) closePort();

  fdSerial = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fdSerial == -1) {
    // Could not open the port.
    return;
  }

  // Allow others access to the serial port
  flock(fdSerial, LOCK_UN );

  // Set no flow control
  fcntl(fdSerial, F_SETFL, 0);

}

bool isOpen()
{
  return (fdSerial > 0);
}

int readByte(char *b)
{
  fd_set set;
  int rv;

  FD_ZERO(&set); /* clear the set */
  FD_SET(fdSerial, &set); /* add our file descriptor to the set */
  rv = select(fdSerial + 1, &set, NULL, NULL, NULL);
  if(rv == -1) {
    return -1; // Runtime error
  } else if(rv == 0) {
    return 0; /* a timeout occured */
  } else {
    int bytesRead = read( fdSerial, b, 1 ); /* there was data to read */
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

  // Initialize curses
  initscr();
  refresh();
  timeout(0);
  noecho();

  openPort(argv[1]);
  if (!isOpen()) {
    printw("Could not open %s, waiting ...\n\r", argv[1]);
    refresh();
  }

  while (1) {
    if (isOpen()) {
      char b;

      // Read by from port and put it on the screen
      int read = readByte(&b);
      if (read == 1) {
       addch(b);
      } else if (read < 0) { // Error detected close the port
        printw("...connection lost to %s ...\n\r", argv[1]);
        refresh();
        closePort();
      }

      // Read port from the keyboard and send it to the port
      b = getch();
      if (b > 0) {
        write( fdSerial, &b, 1);
      }
    } else {
      openPort(argv[1]);
      if (!isOpen()) {
        usleep(1000); // Sleep for 0.001 second and try again
      } else {
        printw("...Reconnected to %s ...\n\r", argv[1]);
        refresh();
      }
    }
  }
}

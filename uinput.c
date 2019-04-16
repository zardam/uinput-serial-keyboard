#include <linux/uinput.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>

#define NUM_KEYS 53
#define NUM_MODES 2

typedef struct {
  char *name;
  int code[NUM_MODES];
} key;

key keymap[NUM_KEYS] = {
  {"left",      {KEY_KP4,         KEY_KP4}},
  {"up",        {KEY_KP8,         KEY_KP8}},
  {"down",      {KEY_KP2,         KEY_KP2}},
  {"right",     {KEY_KP6,         KEY_KP6}},
  {"ok",        {BTN_LEFT,        BTN_LEFT}},
  {"back",      {BTN_RIGHT,       BTN_RIGHT}},
  {"home"},     // not handled here
  {"power"},    // toggle mouse mode
  {NULL},
  {NULL},
  {NULL},
  {NULL},
  {"shift",     {KEY_LEFTSHIFT,   KEY_LEFTSHIFT}},
  {"alpha",     {KEY_CAPSLOCK,    KEY_CAPSLOCK}},
  {"xnt"},      // Switch to first keymap
  {"var"},      // Switch to second keymap
  {"toolbox",   {KEY_RIGHTALT,    KEY_RIGHTALT}},
  {"backspace", {KEY_BACKSPACE,   KEY_ESC}},
  {"A",         {KEY_A,           KEY_F1}},
  {"B",         {KEY_B,           KEY_F2}},
  {"C",         {KEY_C,           KEY_F3}},
  {"D",         {KEY_D,           KEY_F4}},
  {"E ,",       {KEY_E,           KEY_F5}},
  {"F",         {KEY_F,           KEY_F6}},
  {"G",         {KEY_G,           KEY_F7}},
  {"H",         {KEY_H,           KEY_F8}},
  {"I",         {KEY_I,           KEY_F9}},
  {"J",         {KEY_J,           KEY_F10}},
  {"K",         {KEY_K,           KEY_F11}},
  {"L",         {KEY_L,           KEY_F12}},
  {"M 7",       {KEY_M,   KEY_7}},
  {"N 8",       {KEY_N,           KEY_8}},
  {"O 9",       {KEY_O,           KEY_9}},
  {"P (",       {KEY_P,           KEY_5}},
  {"Q )",       {KEY_Q,           KEY_MINUS}},
  {NULL},
  {"R 4",       {KEY_R,           KEY_4}},
  {"S 5",       {KEY_S,           KEY_5}},
  {"T 6",       {KEY_T,           KEY_6}},
  {"U *",       {KEY_U,           KEY_KPASTERISK}},
  {"V /",       {KEY_V,           KEY_KPSLASH}},
  {NULL},
  {"W 1",       {KEY_W,           KEY_1}},
  {"X 2",       {KEY_X,           KEY_2}},
  {"Y 3",       {KEY_Y,           KEY_3}},
  {"Z +",       {KEY_Z,           KEY_KPPLUS}},
  {"space -",   {KEY_SPACE,       KEY_KPMINUS}},
  {NULL},
  {"? 0",       {KEY_QUESTION,           KEY_0}},
  {"! .",       {KEY_COMMA,       KEY_SEMICOLON}},
  {"x10^x",     {KEY_LEFTCTRL,    KEY_LEFTCTRL}},
  {"ans",       {KEY_LEFTALT,     KEY_LEFTALT}},
  {"exe",       {KEY_ENTER,       KEY_EQUAL}},
};

void emit(int fd, int type, int code, int val)
{
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   /* timestamp values below are ignored */
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

   write(fd, &ie, sizeof(ie));
}

int fd = -1;

void sig_handler(int signo)
{
  if (signo == SIGINT)
    printf("received SIGINT\n");
    if(fd != -1) {
      ioctl(fd, UI_DEV_DESTROY);
      close(fd);
    }
    exit(0);
}

void process(u_int64_t scan) {
  static u_int64_t old_scan = 0;
  static int mode = 0;
  int index = 0;
  u_int64_t changed = old_scan ^ scan, mask = 1 << index;

  // Switch mouse
  if(changed & (1<<7)) {
    emit(fd, EV_KEY, KEY_LEFTSHIFT, (scan & (1<<7)) ? 1 : 0);
    emit(fd, EV_KEY, KEY_NUMLOCK, (scan & (1<<7)) ? 1 : 0);
  }

  // Switch mode
  if(scan & (1<<14)) {
    mode = 0;
    input_setup(); // Workaround for retropie
  } else if (scan & (1<<15)) {
    mode = 1;
    input_setup(); // Workaround for retropie
  }

  // Normal codes
  while(index < NUM_KEYS) {
    if(keymap[index].code[mode] != 0 && changed & mask)
      emit(fd, EV_KEY, keymap[index].code[mode], (scan & mask) ? 1 : 0);
    mask <<= 1;
    index++;
  }
  emit(fd, EV_SYN, SYN_REPORT, 0);
  old_scan = scan;
}

void input_setup(void)
{
   int i, j;
   struct uinput_setup usetup;
   long long scan;

   if (signal(SIGINT, sig_handler) == SIG_ERR) {
     printf("can't catch SIGINT\n");
     exit(1);
   }
   fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
   if(fd == -1) {
     exit(1);
   }

   // Setup used keycodes
   ioctl(fd, UI_SET_EVBIT, EV_KEY);
   ioctl(fd, UI_SET_KEYBIT, KEY_NUMLOCK);
   for(i=0; i<NUM_KEYS; i++) {
     for(j=0; j< NUM_MODES; j++) {
       if(keymap[i].code[j] != 0) {
          ioctl(fd, UI_SET_KEYBIT, keymap[i].code[j]);
       }
     }
   }
   // Setup Mouse
   ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
   ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
   ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
   ioctl(fd, UI_SET_EVBIT, EV_REL);
   ioctl(fd, UI_SET_RELBIT, REL_X);
   ioctl(fd, UI_SET_RELBIT, REL_Y);

   memset(&usetup, 0, sizeof(usetup));
   usetup.id.bustype = BUS_USB;
   usetup.id.vendor = 0x1234;
   usetup.id.product = 0x5678;
   strcpy(usetup.name, "NW Keyboard");

   ioctl(fd, UI_DEV_SETUP, &usetup);
   ioctl(fd, UI_DEV_CREATE);
}

void serial_loop() {
  struct termios tty;
  struct termios tty_old;

  int tty_fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY);

  if (tty_fd < 0) {
    printf("Error %d opening tty: %s\n", errno, strerror(errno));
    exit(1);
  }

  memset(&tty, 0, sizeof(tty));

  if(tcgetattr(tty_fd, &tty) != 0) {
    printf("Error %d from tcgetattr: %s\n", errno, strerror(errno));
    exit(1);
  }

  /* Save old tty parameters */
  tty_old = tty;

  /* Set Baud Rate */
  cfsetospeed (&tty, (speed_t)B115200);
  cfsetispeed (&tty, (speed_t)B115200);

  /* Setting other Port Stuff */
  tty.c_cflag     &=  ~PARENB;            // Make 8n1
  tty.c_cflag     &=  ~CSTOPB;
  tty.c_cflag     &=  ~CSIZE;
  tty.c_cflag     |=  CS8;

  tty.c_cflag     &=  ~CRTSCTS;           // no flow control
  tty.c_cc[VMIN]   =  1;                  // read doesn't block
  tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
  tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

  /* Make raw */
  cfmakeraw(&tty);

  /* Flush Port, then applies attributes */
  tcflush(tty_fd, TCIFLUSH);

  if(tcsetattr(tty_fd, TCSANOW, &tty) != 0) {
    printf("Error %d from tcsetattr:", errno, strerror(errno));
    exit(1);
  }

  while(1) {
    int n = 0, spot = 0;
    char buf = '\0';
    char response[1024];
    u_int64_t scan;

    do {
        n = read(tty_fd, &buf, 1);
        sprintf(&response[spot], "%c", buf);
        spot += n;
    } while(buf != '\n' && n > 0 && spot < 1023);

    if (n <= 0) {
        printf("Read error\n");
        exit(1);
    } else {
        if(EOF != sscanf(response, ":%16llx\r\n", &scan)) {
          printf("Response: %s (%llx)\n", response, scan);
          process(scan);
        } else {
          printf("Undecodable response: %s\n", response);
        }
    }
  }
}

int main(void)
{
   input_setup();
   serial_loop();

   return(0);
}

// An example of a user-space program to manipulate the
// fake_webcam interface.

#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#define WIDTH 1280
#define HEIGHT 720
#define DEPTH 3
#define RADIUS 20

int draw_ball(FILE* output, int x, int y);

int main() {
  FILE* output = fopen("/dev/fake_webcam", "wb");
  if (!output) {
    perror("fopen");
    return 1;
  }

  int x = 50;
  int y = 60;
  int x_vel = 15;
  int y_vel = 15;
  while (1) {
    x += x_vel;
    y += y_vel;
    if (x + RADIUS > WIDTH) {
      x = WIDTH - RADIUS;
      x_vel *= -1;
    } else if (x - RADIUS < 0) {
      x = RADIUS;
      x_vel *= -1;
    }
    if (y + RADIUS > HEIGHT) {
      y = HEIGHT - RADIUS;
      y_vel *= -1;
    } else if (y - RADIUS < 0) {
      y = RADIUS;
      y_vel *= -1;
    }
    draw_ball(output, x, y);
    usleep(1000000 / 60);
  }
}

int draw_ball(FILE* output, int x, int y) {
  unsigned char buffer[WIDTH * HEIGHT * 3];
  bzero(buffer, sizeof(buffer));
  for (int i = y - RADIUS; i < y + RADIUS; i++) {
    if (i < 0 || i >= HEIGHT) {
      continue;
    }
    for (int j = x - RADIUS; j < x + RADIUS; ++j) {
      if (j < 0 || j >= WIDTH) {
        continue;
      }
      int pix_idx = (j + (i * WIDTH)) * DEPTH;
      buffer[pix_idx] = 0xff;
    }
  }
  fwrite(buffer, 1, sizeof(buffer), output);
  fflush(output);
}
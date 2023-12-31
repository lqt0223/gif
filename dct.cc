#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979324
#endif

void Compute8x8Dct(const double in[8][8], double out[8][8])
{
  int i, j, u, v; // i, j: coord in freq domain; u, v: coord in time domain
  double s;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
    {
      s = 0;

      for (u = 0; u < 8; u++)
        for (v = 0; v < 8; v++)
          s += in[u][v] * cos((2 * u + 1) * i * M_PI / 16) *
                          cos((2 * v + 1) * j * M_PI / 16) *
               ((i == 0) ? 1 / sqrt(2) : 1) *
               ((j == 0) ? 1 / sqrt(2) : 1);

      out[i][j] = s / 4;
    }
}

void Compute8x8Idct(const double in[8][8], double out[8][8])
{
  int i, j, u, v; // i, j: coord in time domain; u, v: coord in freq domain
  double s;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
    {
      s = 0;

      for (u = 0; u < 8; u++)
        for (v = 0; v < 8; v++)
          s += in[u][v] * cos((2 * i + 1) * u * M_PI / 16) *
                          cos((2 * j + 1) * v * M_PI / 16) *
               ((u == 0) ? 1 / sqrt(2) : 1.) *
               ((v == 0) ? 1 / sqrt(2) : 1.);

      out[i][j] = s / 4;
    }
}

void Print8x8(const char* title, const double in[8][8])
{
  int i, j;

  printf("%s\n", title);

  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 8; j++)
      printf("%8.3f ", in[i][j]);
    printf("\n");
  }
}

int main(void)
{
  double time1[8][8], freq[8][8], time2[8][8];
  int i, j;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
#if 00
      pic1[i][j] = 255;
#else
      time1[i][j] = i + j;
#endif
  Print8x8("pic1:", time1);
  Compute8x8Dct(time1, freq);
  Print8x8("dct:", freq);
  Compute8x8Idct(freq, time2);
  Print8x8("pic2:", time2);

  return 0;
}

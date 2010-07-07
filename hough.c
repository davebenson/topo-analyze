#include <stdio.h>
#include <glib.h>
#include <math.h>

#define RTOD                    (180.0 / M_PI)

#define MIN_ANGLE_RADIANS	(10.0 * M_PI / 180.0)
#define MAX_ANGLE_RADIANS	(30.0 * M_PI / 180.0)
#define N_ANGULAR_BINS		2000
#define CENTER_MIN		50
#define CENTER_MAX		500
#define N_CENTER_BINS           (CENTER_MAX-CENTER_MIN+1)

#define ANGULAR_BIN_SIZE        ((MAX_ANGLE_RADIANS-MIN_ANGLE_RADIANS)/N_ANGULAR_BINS)

static FILE *ppm_fp;
static unsigned ppm_width, ppm_height;
static guint8 *ppm_row;
static unsigned ppm_row_index;

static gboolean next_row (void);

static unsigned counts[N_CENTER_BINS][N_ANGULAR_BINS];

static const char *ppm_command = "tifftopnm $HOME/Downloads/o36118d3.tif";

static void
start_ppm (void)
{
  unsigned nums[3];
  unsigned n_nums = 0;
  char buf[1024];
  g_assert (ppm_fp == NULL);
  ppm_fp = popen (ppm_command, "r");
  if (ppm_fp == NULL)
    g_error ("error running '%s'", ppm_command);

  if (fgets (buf, sizeof (buf), ppm_fp) == NULL)
    g_error ("error reading ppm header");
  if (buf[0] != 'P' || buf[1] != '6')
    g_error ("open binary, RGB ppms supported");
  
  while (n_nums < 3)
    {
      char *end;
      char *at;
      if (!fgets (buf, sizeof (buf), ppm_fp))
        g_error ("error reading ppm header");
      if (buf[0] == '#')
        continue;
      at = buf;
      while (n_nums < 3 && *at != 0)
        {
          if (g_ascii_isspace (*at))
            {
              at++;
              continue;
            }
          nums[n_nums] = strtoul (at, &end, 10);
          if (at == end)
            g_error ("error parsing pnm header (at '%s')", at);
          n_nums++;
          at = end;
        }
    }
  ppm_width = nums[0];
  ppm_height = nums[1];
  if (ppm_row == NULL)
    ppm_row = g_malloc (ppm_width * 3);
  ppm_row_index = 0;
  next_row ();
  ppm_row_index = 0;
}

static gboolean
next_row (void)
{
  if (++ppm_row_index == ppm_height)
    return FALSE;
  if (fread (ppm_row, 3 * ppm_width, 1, ppm_fp) != 1)
    g_error ("error reading ppm row");
  return TRUE;
}

static void
finish_ppm (void)
{
  pclose (ppm_fp);
  ppm_fp = NULL;
}

int main(int argc, char **argv)
{
  unsigned x;
  unsigned r;
  unsigned y;
  /* Pass 1:  bin data to find the top row */
  start_ppm ();
  do
    {
      guint8 *at = ppm_row;
      for (x = 0; x < ppm_width; x++)
        {
	  if (at[0] == 0 && at[1] == 0 && at[2] == 0)
	    {
	      for (r = 0; r < N_ANGULAR_BINS; r++)
	        {
                  double x_offset = (double)x - 0.5 * ppm_width;
                  double radians = MIN_ANGLE_RADIANS + r * ANGULAR_BIN_SIZE;
                  double slope = tan (radians);
                  double y_at_center = (double) ppm_row_index
                                     - x_offset * slope;
                  int yc = (int) floor (y_at_center + 0.5);
                  if (CENTER_MIN <= yc && yc <= CENTER_MAX)
                    counts[yc-CENTER_MIN][r] += 1;
		}
	    }
	  at += 3;
	}
    }
  while (next_row ());
  finish_ppm ();

  for (r = 0; r < N_ANGULAR_BINS; r++)
    for (y = CENTER_MIN; y <= CENTER_MAX; y++)
      printf ("%.4f degrees; y %3u: %5u\n",
              (MIN_ANGLE_RADIANS + r * ANGULAR_BIN_SIZE) * RTOD,
              y,
              counts[y - CENTER_MIN][r]);

  return 0;
}

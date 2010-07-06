#include <stdio.h>

#define MIN_ANGLE_RADIANS	(10.0 * M_PI / 180.0)
#define MAX_ANGLE_RADIANS	(30.0 * M_PI / 180.0)
#define N_ANGULAR_BINS		200
#define CENTER_MIN		50
#define CENTER_MAX		500

static FILE *ppm_fp;
static unsigned ppm_width, ppm_height;

static void
start_ppm (void)
{
  unsigned nums[3];
  unsigned n_nums = 0;
  g_assert (ppm_fp == NULL);
  ppm_fp = popen (ppm_command, "rb");
  if (ppm_fp == NULL)
    g_error ("error running '%s'", ppm_command);

  if (fgets (buf, sizeof (buf), ppm_fp) == NULL)
    g_error ("error reading ppm header");
  if (buf[0] != 'P' || buf[1] != '6')
    g_error ("open binary, RGB ppms supported");
  
  while (n_nums < 3)
    {
      if (!fgets (buf, sizeof (buf), ppm_fp))
        g_error ("error reading ppm header");
      if (buf[0] == '#')
        continue;
      ...
    }
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
  if (fread (ppm_row, 3, ppm_width, ppm_fp) == NULL)
    g_error ("error reading ppm row")
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
  /* Pass 1:  bin data to find the top row */
  start_ppm ();
  do
    {
      unsigned x;
      for (x = 0; x < ppm_width; x++)
        {
	  if (at[0] == 0 && at[1] == 0 && at[2] == 0)
	    {
	      for (r = 0; r < N_ANGULAR_BINS; r++)
	        {
		  ...
		}
	    }
	  at += 3;
	}
    }
  while (next_row ());
  finish_ppm ();

  char buf[1024];
  if (fgets (buf, sizeof (buf), stdin) == NULL)
    g_error ("error parsing header");
...
}

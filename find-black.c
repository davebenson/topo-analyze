#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <math.h>

#define RTOD                    (180.0 / M_PI)

#define BLACK_LEVEL             40
#define MIN_ANGLE_RADIANS	(10.0 * M_PI / 180.0)
#define MAX_ANGLE_RADIANS	(30.0 * M_PI / 180.0)
#define N_ANGULAR_BINS		2000
#define CENTER_MIN		50
#define CENTER_MAX		500
#define N_CENTER_BINS           (CENTER_MAX-CENTER_MIN+1)

#define ANGULAR_BIN_SIZE        ((MAX_ANGLE_RADIANS-MIN_ANGLE_RADIANS)/N_ANGULAR_BINS)

#define PAD_BIG     8
#define PAD_SMALL   2
#define HV_THRESH   40

static FILE *ppm_fp;
static unsigned ppm_width, ppm_height;
static guint8 *ppm_row;
static unsigned ppm_row_index;

static gboolean next_row (void);

static unsigned counts[N_CENTER_BINS][N_ANGULAR_BINS];

static const char *ppm_command = "tifftopnm o36118d2.tif";

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

struct Coord
{
  unsigned x,y;
  gboolean used;
  struct Coord *stack_next;
};

static int compare_coords (const void *a, const void *b)
{
  const struct Coord *A = a;
  const struct Coord *B = b;
  if (A->y < B->y) return -1;
  if (A->y > B->y) return 1;
  if (A->x < B->x) return -1;
  if (A->x > B->x) return 1;
  return 0;
}
static struct Coord *
find_coord (unsigned x,
            unsigned y,
            GArray *array)
{
  struct Coord dummy = {x,y,FALSE,NULL};
  return bsearch (&dummy, array->data, array->len,
                  sizeof (struct Coord), compare_coords);
}

static void
add_to_stack_if_needed (unsigned x,
                        unsigned y,
                        GArray *array,
                        struct Coord **stack)
{
  struct Coord *c = find_coord (x, y, array);
  if (c != NULL && !c->used)
    {
      c->stack_next = *stack;
      c->used = TRUE;
      *stack = c;
    }
}

static GArray *
find_longest_chain (GArray *array)
{
  struct Coord *best = NULL;
  unsigned best_len = 0;
  unsigned i;
  for (i = 0; i < array->len; i++)
    g_array_index (array, struct Coord, i).used = FALSE;;
  for (i = 0; i < array->len; i++)
    {
      if (!g_array_index (array, struct Coord, i).used)
        {
          struct Coord *stack = &g_array_index (array, struct Coord, i);
          struct Coord *first = stack;
          unsigned count = 0;
          stack->used = TRUE;
          stack->stack_next = NULL;
          while (stack != NULL)
            {
              struct Coord *c = stack;
              stack = stack->stack_next;
              count++;
              add_to_stack_if_needed (c->x - 1, c->y - 1, array, &stack);
              add_to_stack_if_needed (c->x,     c->y - 1, array, &stack);
              add_to_stack_if_needed (c->x + 1, c->y - 1, array, &stack);
              add_to_stack_if_needed (c->x - 1, c->y,     array, &stack);
              add_to_stack_if_needed (c->x + 1, c->y,     array, &stack);
              add_to_stack_if_needed (c->x - 1, c->y + 1, array, &stack);
              add_to_stack_if_needed (c->x,     c->y + 1, array, &stack);
              add_to_stack_if_needed (c->x + 1, c->y + 1, array, &stack);
            }
          if (count > best_len)
            {
              best = first;
              best_len = count;
            }
        }
    }
  for (i = 0; i < array->len; i++)
    g_array_index (array, struct Coord, i).used = FALSE;;

  GArray *best_chain = g_array_new (FALSE, FALSE, sizeof (struct Coord));
  {
    struct Coord *stack = best;
    stack->used = TRUE;
    stack->stack_next = NULL;
    while (stack != NULL)
      {
        struct Coord *c = stack;
        stack = stack->stack_next;
        g_array_append_val (best_chain, *c);
        add_to_stack_if_needed (c->x - 1, c->y - 1, array, &stack);
        add_to_stack_if_needed (c->x,     c->y - 1, array, &stack);
        add_to_stack_if_needed (c->x + 1, c->y - 1, array, &stack);
        add_to_stack_if_needed (c->x - 1, c->y,     array, &stack);
        add_to_stack_if_needed (c->x + 1, c->y,     array, &stack);
        add_to_stack_if_needed (c->x - 1, c->y + 1, array, &stack);
        add_to_stack_if_needed (c->x,     c->y + 1, array, &stack);
        add_to_stack_if_needed (c->x + 1, c->y + 1, array, &stack);
      }
  }
  g_array_sort (best_chain, compare_coords);
  return best_chain;
}
static void
box_threshold_points (GArray *input,
                      int     padx,
                      int     pady,
                      int     thresh,
                      GArray *output)
{
  unsigned i;
  for (i = 0; i < input->len; i++)
    {
      struct Coord *c = &g_array_index(input, struct Coord, i);
      int sx, sy, count = 0;
      int x = c->x;
      int y = c->y;
      for (sx = x - PAD_SMALL; sx <= x + PAD_SMALL; sx++)
        for (sy = y - PAD_BIG; sy <= y + PAD_BIG; sy++)
          if (sx != x || sy != y)
            if (find_coord (sx, sy, input))
              count++;
      if (count > thresh)
        g_array_append_val (output, *c);
    }
}

static void
render_pgm_from_sorted_coords (FILE *out,
                               GArray *coords)
{
  unsigned i = 0;
  unsigned x,y;
  for (y = 0; y < ppm_height; y++)
    {
      memset (ppm_row, 0, ppm_width);
      for (x = 0; x < ppm_height; x++)
        {
          while (i < coords->len
              && g_array_index (coords, struct Coord, i).y == y)
            {
              ppm_row[g_array_index (coords, struct Coord, i).x] = 255;
              i++;
            }
        }
      fwrite (ppm_row, ppm_width, 1, out);
    }
}
static void
find_vertical_chains (GArray *best_chain,
                      GArray **left_chain_out,
                      GArray **right_chain_out,
                      unsigned x_thresh)
{
  GArray *vertical_points = g_array_new (FALSE, FALSE, sizeof (struct Coord));
  box_threshold_points (best_chain, PAD_SMALL, PAD_BIG, HV_THRESH, vertical_points);
  GArray *left_points = g_array_new (FALSE, FALSE, sizeof (struct Coord));
  GArray *right_points = g_array_new (FALSE, FALSE, sizeof (struct Coord));
  unsigned i;
  for (i = 0; i < vertical_points->len; i++)
    if (g_array_index (vertical_points, struct Coord, i).x < x_thresh)
      g_array_append_val (left_points, g_array_index (vertical_points, struct Coord, i));
    else
      g_array_append_val (right_points, g_array_index (vertical_points, struct Coord, i));
  g_assert (left_points->len > 0);
  g_assert (right_points->len > 0);
  *left_chain_out = find_longest_chain (left_points);
  *right_chain_out = find_longest_chain (right_points);
  g_array_free (left_points, TRUE);
  g_array_free (right_points, TRUE);
  g_array_free (vertical_points, TRUE);
}

static void
fit_line (GArray *chain,
          double *slope_out,
          double *intercept_out)
{
  /* e = sum((y_i - (x_i*a + b))^2)
       = sum(y_i^2 - 2 x_i y_i a - 2 b y_i + x_i^2 a^2 + 2 x_i a b + b^2)
       = sum(y_i^2) + sum(-2 x_i y_i) a + sum(-2 y_i) b 
         + sum(x_i^2) a^2 + 2 a b sum(x_i) + N b^2
   *
   * de/da = sum(-2 x_i y_i) + sum(2 x_i^2) a + b sum(2 x_i) = 0
   * de/db = sum(-2 y_i) + sum(2 x_i) a + 2N b = 0.
   * 
   * sxx = sum(x_i^2)    sxy = sum(x_i y_i)   sx = sum(x_i)   sy = sum(y_i)
   * sxx a + sx b = sxy
   * sx  a + N  b = sy
   * sx*(sx/N) a + sx b = (sx sy / N)
   * (sxx - sx*sx/N) a = sxy - (sx sy / N)
   * (N sxx - sx*sx) a = N sxy - sx sy
   *
   * a = (N sxy - sx sy) / (N sxx - sx sx)
   * b = (sy - sx a) / N
   */
  double sx = 0, sxx = 0, sxy = 0, sy = 0, N = 0;
  unsigned i;
  for (i = 0; i < chain->len; i++)
    {
      double x = g_array_index (chain, struct Coord, i).x;
      double y = g_array_index (chain, struct Coord, i).y;
      sx += x;
      sy += y;
      sxy += x * y;
      sxx += x * x;
      N += 1.0;
    }

  *slope_out = (N * sxy - sx * sy) / (N * sxx - sx * sx);
  *intercept_out = (sy - sx * (*slope_out)) / N;
}

/* point output satifies y=slope*x + intercept
                         x=vslope*y + vintercept.
  y = slope*vslope*y + slope * vintercept + intercept
  y * (1 - slope * vslope) = slope * vintercept + intercept.
  y = (slope * vintercept + intercept) / (1 - slope * vslope) */
static void
intersect_lines (double slope, double intercept,
                 double vslope, double vintercept,
                 double *x_out,
                 double *y_out)
{
  *y_out = (slope * vintercept + intercept) / (1.0 - slope * vslope);
  *x_out = vslope * (*y_out) + vintercept;
}
static void
swap_xy (GArray *array)
{
  struct Coord *c = (struct Coord*) (array->data);
  unsigned rem = array->len;
  while (rem--)
    {
      int swap = c->x;
      c->x = c->y;
      c->y = swap;
      c++;
    }
}

int main(int argc, char **argv)
{
  unsigned x;
  unsigned r;
  unsigned y;
  unsigned i;
  GArray *array = g_array_new (FALSE, FALSE, sizeof (struct Coord));
  /* Pass 1:  bin data to find the top row */
  start_ppm ();
  printf ("P5\n%u %u\n255\n", ppm_width, ppm_height);
  do
    {
      guint8 *at = ppm_row;
      for (x = 0; x < ppm_width; x++)
        {
	  if (at[0] <= BLACK_LEVEL && at[1] <= BLACK_LEVEL && at[2] <= BLACK_LEVEL)
            {
              struct Coord c = {x,ppm_row_index,FALSE,NULL};
              g_array_append_val (array, c);
              //fputc (255, stdout);
            }
          //else
            //fputc (0, stdout);
	  at += 3;
	}
    }
  while (next_row ());
  finish_ppm ();

  GArray *best_chain = find_longest_chain (array);

  GArray *left_chain, *right_chain;
  find_vertical_chains (best_chain, &left_chain, &right_chain, ppm_width/2);

  GArray *top_chain, *bottom_chain;
  swap_xy (best_chain);
  g_array_sort (best_chain, compare_coords);
  find_vertical_chains (best_chain, &top_chain, &bottom_chain, ppm_height/2);
  swap_xy (best_chain);
  g_array_sort (best_chain, compare_coords);
  swap_xy (top_chain);
  swap_xy (bottom_chain);

  double left_slope, left_intercept;
  double right_slope, right_intercept;
  double top_slope, top_intercept;
  double bottom_slope, bottom_intercept;
  swap_xy (left_chain);
  fit_line (left_chain, &left_slope, &left_intercept);
  swap_xy (left_chain);

  swap_xy (right_chain);
  fit_line (right_chain, &right_slope, &right_intercept);
  swap_xy (right_chain);

  fit_line (top_chain, &top_slope, &top_intercept);
  fit_line (bottom_chain, &bottom_slope, &bottom_intercept);

  double tl_x, tl_y, tr_x, tr_y;
  double bl_x, bl_y, br_x, br_y;
  intersect_lines (top_slope, top_intercept,
                   left_slope, left_intercept,
                   &tl_x, &tl_y);
  intersect_lines (top_slope, top_intercept,
                   right_slope, right_intercept,
                   &tr_x, &tr_y);
  intersect_lines (bottom_slope, bottom_intercept,
                   left_slope, left_intercept,
                   &bl_x, &bl_y);
  intersect_lines (bottom_slope, bottom_intercept,
                   right_slope, right_intercept,
                   &br_x, &br_y);

  fprintf (stderr, "top-left:      %.6f,%.6f\n", tl_x, tl_y);
  fprintf (stderr, "top-right:     %.6f,%.6f\n", tr_x, tr_y);
  fprintf (stderr, "bottom-left:   %.6f,%.6f\n", bl_x, bl_y);
  fprintf (stderr, "bottom-right:  %.6f,%.6f\n", br_x, br_y);

#if 0
  for (i = 0; i < vertical_points->len; i++)
    fprintf (stderr, "%u %u\n",
             
             g_array_index (vertical_points, struct Coord, i).y);
#endif

  render_pgm_from_sorted_coords (stdout, left_chain);

  return 0;
}


#include "config.h"

#include <glib.h>
#include <gst/math-compat.h>
#include <math.h>

#include "gstcms.h"

#define SCALE 256

static void
get_taps (double *taps, double x)
{
  taps[3] = x * x * (x - 1);
  taps[2] = x * (-x * x + x + 1);
  x = 1 - x;
  taps[1] = x * (-x * x + x + 1);
  taps[0] = x * x * (x - 1);
}

int
main (int argc, char *argv[])
{
  int i;

  g_print ("/* This file is autogenerated.  Do not edit.*/\n");
  g_print ("#include <glib.h>\n");
  g_print ("gint8 cog_resample_table_4tap[256][4] = {\n");
  for (i = 0; i < 256; i++) {
    double x = i / 256.0;
    double taps[4];
    int t[4];
    int sum;

    get_taps (taps, x);
    taps[0] *= SCALE;
    taps[1] *= SCALE;
    taps[2] *= SCALE;
    taps[3] *= SCALE;

    t[0] = floor (taps[0]);
    t[1] = floor (taps[1]);
    t[2] = floor (taps[2]);
    t[3] = floor (taps[3]);
    sum = t[0] + t[1] + t[2] + t[3];

    for (; sum < SCALE; sum++) {
      int i;
      double max = 0;
      int max_i = -1;
      for (i = 0; i < 4; i++) {
        if (max_i == -1 || (t[i] < taps[i] && (taps[i] - t[i]) > max)) {
          max_i = i;
          max = taps[i] - t[i];
        }
      }
      t[max_i]++;
    }
    sum = t[0] + t[1] + t[2] + t[3];

    g_print ("  { %d, %d, %d, %d }, /* %d %d */\n",
        t[0], t[1], t[2], t[3], t[2] + t[0], t[1] + t[3]);
#if 0
    g_print ("/* %.3f %.3f %.3f %.3f %d */\n",
        taps[0] - t[0], taps[1] - t[1], taps[2] - t[2], taps[3] - t[3], sum);
#endif
  }
  g_print ("};\n");
  g_print ("\n");


  {
    int cm, bits;

    for (cm = 0; cm < 2; cm++) {
      for (bits = 6; bits <= 8; bits += 2) {

        ColorMatrix matrix;

        /*
         * At this point, everything is in YCbCr
         * All components are in the range [0,255]
         */
        color_matrix_set_identity (&matrix);

        /* offset required to get input video black to (0.,0.,0.) */
        /* we don't do this because the code does it for us */
        color_matrix_offset_components (&matrix, -16, -128, -128);

        color_matrix_scale_components (&matrix, (1 / 219.0), (1 / 224.0),
            (1 / 224.0));

        /* colour matrix, YCbCr -> RGB */
        /* Requires Y in [0,1.0], Cb&Cr in [-0.5,0.5] */
        if (cm) {
          color_matrix_YCbCr_to_RGB (&matrix, 0.2126, 0.0722);  /* HD */
        } else {
          color_matrix_YCbCr_to_RGB (&matrix, 0.2990, 0.1140);  /* SD */
        }

        /*
         * We are now in RGB space
         */

        /* scale to output range. */
        color_matrix_scale_components (&matrix, 255.0, 255.0, 255.0);

        /* because we're doing 8-bit matrix coefficients */
        color_matrix_scale_components (&matrix, 1 << bits, 1 << bits,
            1 << bits);

        g_print ("static const int cog_ycbcr_to_rgb_matrix_%dbit_%s[] = {\n",
            bits, cm ? "hdtv" : "sdtv");
        g_print ("  %d, %d, %d, %d,\n",
            (int) rint (matrix.m[0][0]),
            (int) rint (matrix.m[0][1]),
            (int) rint (matrix.m[0][2]), (int) rint (matrix.m[0][3]));
        g_print ("  %d, %d, %d, %d,\n",
            (int) rint (matrix.m[1][0]),
            (int) rint (matrix.m[1][1]),
            (int) rint (matrix.m[1][2]), (int) rint (matrix.m[1][3]));
        g_print ("  %d, %d, %d, %d,\n",
            (int) rint (matrix.m[2][0]),
            (int) rint (matrix.m[2][1]),
            (int) rint (matrix.m[2][2]), (int) rint (matrix.m[2][3]));
        g_print ("};\n");
      }
    }
  }

  {
    int cm, bits;

    for (cm = 0; cm < 2; cm++) {
      for (bits = 8; bits <= 8; bits += 1) {

        ColorMatrix matrix;

        color_matrix_set_identity (&matrix);

        color_matrix_scale_components (&matrix, (1 / 255.0), (1 / 255.0),
            (1 / 255.0));

        /* colour matrix, RGB -> YCbCr */
        if (cm) {
          color_matrix_RGB_to_YCbCr (&matrix, 0.2126, 0.0722);  /* HD */
        } else {
          color_matrix_RGB_to_YCbCr (&matrix, 0.2990, 0.1140);  /* SD */
        }

        /*
         * We are now in YCbCr space
         */

        color_matrix_scale_components (&matrix, 219.0, 224.0, 224.0);

        color_matrix_offset_components (&matrix, 16, 128, 128);

        /* because we're doing 8-bit matrix coefficients */
        color_matrix_scale_components (&matrix, 1 << bits, 1 << bits,
            1 << bits);

        g_print ("static const int cog_rgb_to_ycbcr_matrix_%dbit_%s[] = {\n",
            bits, cm ? "hdtv" : "sdtv");
        g_print ("  %d, %d, %d, %d,\n",
            (int) rint (matrix.m[0][0]),
            (int) rint (matrix.m[0][1]),
            (int) rint (matrix.m[0][2]), (int) rint (matrix.m[0][3]));
        g_print ("  %d, %d, %d, %d,\n",
            (int) rint (matrix.m[1][0]),
            (int) rint (matrix.m[1][1]),
            (int) rint (matrix.m[1][2]), (int) rint (matrix.m[1][3]));
        g_print ("  %d, %d, %d, %d,\n",
            (int) rint (matrix.m[2][0]),
            (int) rint (matrix.m[2][1]),
            (int) rint (matrix.m[2][2]), (int) rint (matrix.m[2][3]));
        g_print ("};\n");
      }
    }
  }

  {
    int cm, bits;

    for (cm = 0; cm < 2; cm++) {
      for (bits = 8; bits <= 8; bits += 1) {

        ColorMatrix matrix;

        color_matrix_set_identity (&matrix);

        /* offset required to get input video black to (0.,0.,0.) */
        /* we don't do this because the code does it for us */
        color_matrix_offset_components (&matrix, -16, -128, -128);

        color_matrix_scale_components (&matrix, (1 / 219.0), (1 / 224.0),
            (1 / 224.0));

        /* colour matrix, RGB -> YCbCr */
        if (cm) {
          color_matrix_YCbCr_to_RGB (&matrix, 0.2126, 0.0722);  /* HD */
          color_matrix_RGB_to_YCbCr (&matrix, 0.2990, 0.1140);  /* SD */
        } else {
          color_matrix_YCbCr_to_RGB (&matrix, 0.2990, 0.1140);  /* SD */
          color_matrix_RGB_to_YCbCr (&matrix, 0.2126, 0.0722);  /* HD */
        }

        /*
         * We are now in YCbCr space
         */

        color_matrix_scale_components (&matrix, 219.0, 224.0, 224.0);

        color_matrix_offset_components (&matrix, 16, 128, 128);

        /* because we're doing 8-bit matrix coefficients */
        color_matrix_scale_components (&matrix, 1 << bits, 1 << bits,
            1 << bits);

        g_print
            ("static const int cog_ycbcr_%s_to_ycbcr_%s_matrix_%dbit[] = {\n",
            cm ? "hdtv" : "sdtv", cm ? "sdtv" : "hdtv", bits);
        g_print ("  %d, %d, %d, %d,\n", (int) rint (matrix.m[0][0]),
            (int) rint (matrix.m[0][1]), (int) rint (matrix.m[0][2]),
            (int) rint (matrix.m[0][3]));
        g_print ("  %d, %d, %d, %d,\n", (int) rint (matrix.m[1][0]),
            (int) rint (matrix.m[1][1]), (int) rint (matrix.m[1][2]),
            (int) rint (matrix.m[1][3]));
        g_print ("  %d, %d, %d, %d,\n", (int) rint (matrix.m[2][0]),
            (int) rint (matrix.m[2][1]), (int) rint (matrix.m[2][2]),
            (int) rint (matrix.m[2][3]));
        g_print ("};\n");
      }
    }
  }

  return 0;
}

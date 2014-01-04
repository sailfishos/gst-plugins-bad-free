/* 
 * Copyright 2006, 2007, 2008, 2009, 2010 Fluendo S.A.
 *  Authors: Jan Schmidt <jan@fluendo.com>
 *           Kapil Agrawal <kapil@fluendo.com>
 *           Julien Moutte <julien@fluendo.com>
 *
 * This library is licensed under 4 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * four licenses are the MPL 1.1, the LGPL, the GPL and the MIT
 * license.
 *
 * MPL:
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * GPL:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * MIT:
 *
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mpegtsmux_h264.h"
#include <string.h>

#define GST_CAT_DEFAULT mpegtsmux_debug

#define SPS_PPS_PERIOD GST_SECOND

typedef struct PrivDataH264 PrivDataH264;

struct PrivDataH264
{
  GstBuffer *last_codec_data;
  GstClockTime last_resync_ts;
  GstBuffer *cached_es;
  guint8 nal_length_size;
};

void
mpegtsmux_free_h264 (gpointer prepare_data)
{
  PrivDataH264 *h264_data = (PrivDataH264 *) prepare_data;
  if (h264_data->cached_es) {
    gst_buffer_unref (h264_data->cached_es);
    h264_data->cached_es = NULL;
  }
  g_free (prepare_data);
}

static inline gboolean
mpegtsmux_process_codec_data_h264 (MpegTsPadData * data, MpegTsMux * mux)
{
  PrivDataH264 *h264_data;
  gboolean ret = FALSE;

  /* Initialize our private data structure for caching */
  if (G_UNLIKELY (!data->prepare_data)) {
    data->prepare_data = g_new0 (PrivDataH264, 1);
    h264_data = (PrivDataH264 *) data->prepare_data;
    h264_data->last_resync_ts = GST_CLOCK_TIME_NONE;
  }

  h264_data = (PrivDataH264 *) data->prepare_data;

  /* Detect a codec data change */
  if (h264_data->last_codec_data != data->codec_data) {
    if (h264_data->cached_es) {
      gst_buffer_unref (h264_data->cached_es);
      h264_data->cached_es = NULL;
    }
    ret = TRUE;
  }

  /* Generate the SPS/PPS ES header that will be prepended regularly */
  if (G_UNLIKELY (!h264_data->cached_es)) {
    gint offset = 4, i = 0, nb_sps = 0, nb_pps = 0;
    gsize out_offset = 0;
    guint8 startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
    h264_data->last_codec_data = data->codec_data;
    h264_data->cached_es =
        gst_buffer_new_and_alloc (GST_BUFFER_SIZE (data->codec_data) * 10);

    /* Get NAL length size */
    h264_data->nal_length_size =
        (GST_READ_UINT8 (GST_BUFFER_DATA (data->codec_data) + offset) & 0x03) +
        1;
    GST_LOG_OBJECT (mux, "NAL length will be coded on %u bytes",
        h264_data->nal_length_size);
    offset++;

    /* How many SPS */
    nb_sps =
        GST_READ_UINT8 (GST_BUFFER_DATA (data->codec_data) + offset) & 0x1f;
    GST_DEBUG_OBJECT (mux, "we have %d Sequence Parameter Set", nb_sps);
    offset++;

    /* For each SPS */
    for (i = 0; i < nb_sps; i++) {
      guint16 sps_size =
          GST_READ_UINT16_BE (GST_BUFFER_DATA (data->codec_data) + offset);

      GST_LOG_OBJECT (mux, "Sequence Parameter Set is %d bytes", sps_size);

      /* Jump over SPS size */
      offset += 2;

      /* Fake a start code */
      memcpy (GST_BUFFER_DATA (h264_data->cached_es) + out_offset,
          startcode, 4);
      out_offset += 4;
      /* Now push the SPS */
      memcpy (GST_BUFFER_DATA (h264_data->cached_es) + out_offset,
          GST_BUFFER_DATA (data->codec_data) + offset, sps_size);

      out_offset += sps_size;
      offset += sps_size;
    }

    /* How many PPS */
    nb_pps = GST_READ_UINT8 (GST_BUFFER_DATA (data->codec_data) + offset);
    GST_LOG_OBJECT (mux, "we have %d Picture Parameter Set", nb_sps);
    offset++;

    /* For each PPS */
    for (i = 0; i < nb_pps; i++) {
      gint pps_size =
          GST_READ_UINT16_BE (GST_BUFFER_DATA (data->codec_data) + offset);

      GST_LOG_OBJECT (mux, "Picture Parameter Set is %d bytes", pps_size);

      /* Jump over PPS size */
      offset += 2;

      /* Fake a start code */
      memcpy (GST_BUFFER_DATA (h264_data->cached_es) + out_offset,
          startcode, 4);
      out_offset += 4;
      /* Now push the PPS */
      memcpy (GST_BUFFER_DATA (h264_data->cached_es) + out_offset,
          GST_BUFFER_DATA (data->codec_data) + offset, pps_size);

      out_offset += pps_size;
      offset += pps_size;
    }
    GST_BUFFER_SIZE (h264_data->cached_es) = out_offset;
    GST_DEBUG_OBJECT (mux, "generated a %" G_GSIZE_FORMAT
        " bytes SPS/PPS header", out_offset);
  }
  return ret;
}

GstBuffer *
mpegtsmux_prepare_h264 (GstBuffer * buf, MpegTsPadData * data, MpegTsMux * mux)
{
  guint8 startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
  gsize out_offset = 0, in_offset = 0;
  GstBuffer *out_buf;
  gboolean changed;
  PrivDataH264 *h264_data;
  GstClockTimeDiff diff = GST_CLOCK_TIME_NONE;

  GST_DEBUG_OBJECT (mux, "Preparing H264 buffer for output");

  changed = mpegtsmux_process_codec_data_h264 (data, mux);
  h264_data = (PrivDataH264 *) data->prepare_data;

  if (GST_CLOCK_TIME_IS_VALID (h264_data->last_resync_ts) &&
      GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buf))) {
    diff = GST_CLOCK_DIFF (h264_data->last_resync_ts,
        GST_BUFFER_TIMESTAMP (buf));
  }

  if (changed || (GST_CLOCK_TIME_IS_VALID (diff) && diff > SPS_PPS_PERIOD)) {
    out_buf = gst_buffer_new_and_alloc (GST_BUFFER_SIZE (buf) * 2 +
        GST_BUFFER_SIZE (h264_data->cached_es));
    h264_data->last_resync_ts = GST_BUFFER_TIMESTAMP (buf);
    memcpy (GST_BUFFER_DATA (out_buf), GST_BUFFER_DATA (h264_data->cached_es),
        GST_BUFFER_SIZE (h264_data->cached_es));
    out_offset = GST_BUFFER_SIZE (h264_data->cached_es);
    GST_DEBUG_OBJECT (mux, "prepending SPS/PPS information to that packet");
  } else {
    out_buf = gst_buffer_new_and_alloc (GST_BUFFER_SIZE (buf) * 2);
  }

  /* We want the same metadata */
  gst_buffer_copy_metadata (out_buf, buf, GST_BUFFER_COPY_ALL);

  while (in_offset < GST_BUFFER_SIZE (buf) &&
      out_offset < GST_BUFFER_SIZE (out_buf) - 4) {
    guint32 nal_size = 0;

    switch (h264_data->nal_length_size) {
      case 1:
        nal_size = GST_READ_UINT8 (GST_BUFFER_DATA (buf) + in_offset);
        break;
      case 2:
        nal_size = GST_READ_UINT16_BE (GST_BUFFER_DATA (buf) + in_offset);
        break;
      case 4:
        nal_size = GST_READ_UINT32_BE (GST_BUFFER_DATA (buf) + in_offset);
        break;
      default:
        GST_WARNING_OBJECT (mux, "unsupported NAL length size %u",
            h264_data->nal_length_size);
    }
    in_offset += h264_data->nal_length_size;

    /* Generate an Elementary stream buffer by inserting a startcode */
    memcpy (GST_BUFFER_DATA (out_buf) + out_offset, startcode, 4);
    out_offset += 4;
    memcpy (GST_BUFFER_DATA (out_buf) + out_offset,
        GST_BUFFER_DATA (buf) + in_offset,
        MIN (nal_size, GST_BUFFER_SIZE (out_buf) - out_offset));
    in_offset += nal_size;
    out_offset += nal_size;
  }

  if (out_offset > GST_BUFFER_SIZE (out_buf)) {
    GST_WARNING_OBJECT (mux, "Calculated buffer size %" G_GSIZE_FORMAT
        " is greater than max expected size %u, "
        "using max expected size (Input might not be in "
        "avc format", out_offset, GST_BUFFER_SIZE (out_buf));
    out_offset = GST_BUFFER_SIZE (out_buf);
  }
  GST_BUFFER_SIZE (out_buf) = out_offset;

  return out_buf;
}

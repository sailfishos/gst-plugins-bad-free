/*
 * Opus Payloader Gst Element
 *
 *   @author: Danilo Cesar Lemes de Paula <danilo.eu@gmail.com>
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
 */

#ifndef __GST_RTP_OPUS_PAY_H__
#define __GST_RTP_OPUS_PAY_H__

#include <gst/gst.h>
#include <gst/rtp/gstbasertppayload.h>

G_BEGIN_DECLS

#define GST_TYPE_RTP_OPUS_PAY \
  (gst_rtp_opus_pay_get_type())
#define GST_RTP_OPUS_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_OPUS_PAY,GstRtpOPUSPay))
#define GST_RTP_OPUS_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_OPUS_PAY,GstRtpOPUSPayClass))
#define GST_IS_RTP_OPUS_PAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_OPUS_PAY))
#define GST_IS_RTP_OPUS_PAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_OPUS_PAY))

typedef struct _GstRtpOPUSPay GstRtpOPUSPay;
typedef struct _GstRtpOPUSPayClass GstRtpOPUSPayClass;

struct _GstRtpOPUSPay
{
  GstBaseRTPPayload payload;
};

struct _GstRtpOPUSPayClass
{
  GstBaseRTPPayloadClass parent_class;
};

GType gst_rtp_opus_pay_get_type (void);

G_END_DECLS

#endif /* __GST_RTP_OPUS_PAY_H__ */

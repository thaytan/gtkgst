/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <matthew@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gtkgstsink
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgtksink.h"

GST_DEBUG_CATEGORY (gst_debug_gtk_sink);
#define GST_CAT_DEFAULT gst_debug_gtk_sink

static void gst_gtk_sink_finalize (GObject * object);
static void gst_gtk_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_gtk_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_gtk_sink_stop (GstBaseSink * bsink);

static gboolean gst_gtk_sink_query (GstBaseSink * bsink, GstQuery * query);

static GstStateChangeReturn
gst_gtk_sink_change_state (GstElement * element, GstStateChange transition);

static void gst_gtk_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_gtk_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static GstFlowReturn gst_gtk_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);

static GstStaticPadTemplate gst_gtk_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE("BGRA"))
    );

enum
{
  ARG_0,
  PROP_WIDGET
};

enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

static guint gst_gtk_sink_signals[LAST_SIGNAL] = { 0 };

#define gst_gtk_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtkSink, gst_gtk_sink,
    GST_TYPE_VIDEO_SINK, GST_DEBUG_CATEGORY_INIT (gst_debug_gtk_sink, "gtksink", 0,
        "Gtk Video Sink"));

static void
gst_gtk_sink_class_init (GstGtkSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_gtk_sink_set_property;
  gobject_class->get_property = gst_gtk_sink_get_property;

  gst_element_class_set_metadata (gstelement_class, "Gtk Video Sink",
      "Sink/Video", "A video sink the renders to a GtkGstWidget",
      "Matthew Waters <matthew@centricular.com>");

  g_object_class_install_property (gobject_class, PROP_WIDGET,
      g_param_spec_object ("widget", "Gtk Widget",
          "Which GtkGstWidget to render into",
          GTK_TYPE_GST_WIDGET, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_gtk_sink_template));

  gobject_class->finalize = gst_gtk_sink_finalize;

  gstelement_class->change_state = gst_gtk_sink_change_state;
  gstbasesink_class->query = gst_gtk_sink_query;
  gstbasesink_class->set_caps = gst_gtk_sink_set_caps;
  gstbasesink_class->get_times = gst_gtk_sink_get_times;
  gstbasesink_class->stop = gst_gtk_sink_stop;

  gstvideosink_class->show_frame = gst_gtk_sink_show_frame;
}

static void
gst_gtk_sink_init (GstGtkSink * gtk_sink)
{
}

static void
gst_gtk_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGtkSink *gtk_sink;

  g_return_if_fail (GST_IS_GTK_SINK (object));

  gtk_sink = GST_GTK_SINK (object);

  switch (prop_id) {
    case PROP_WIDGET:
      GST_OBJECT_LOCK (gtk_sink);
      if (gtk_sink->widget)
        g_object_unref (gtk_sink->widget);
      gtk_sink->widget = g_value_get_object (value);
      GST_OBJECT_UNLOCK (gtk_sink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gtk_sink_finalize (GObject * object)
{
  GstGtkSink *gtk_sink;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gtk_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGtkSink *gtk_sink;

  gtk_sink = GST_GTK_SINK (object);

  switch (prop_id) {
    case PROP_WIDGET:
      g_value_set_object (value, gtk_sink->widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gtk_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstGtkSink *gtk_sink = GST_GTK_SINK (bsink);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }

  return res;
}

static gboolean
gst_gtk_sink_stop (GstBaseSink * bsink)
{
  return TRUE;
}

static GstStateChangeReturn
gst_gtk_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstGtkSink *gtk_sink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  gtk_sink = GST_GTK_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_gtk_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstGtkSink *gtk_sink;

  gtk_sink = GST_GTK_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&gtk_sink->v_info),
            GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info));
      }
    }
  }
}

gboolean
gst_gtk_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstGtkSink *gtk_sink = GST_GTK_SINK (bsink);

  GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&gtk_sink->v_info, caps))
    return FALSE;

  if (!gtk_sink->widget)
    return FALSE;

  if (!gtk_gst_widget_set_caps (gtk_sink->widget, caps))
    return FALSE;

  return TRUE;
}

static GstFlowReturn
gst_gtk_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstGtkSink *gtk_sink;
  GstBuffer *stored_buffer;

  GST_TRACE ("rendering buffer:%p", buf);

  gtk_sink = GST_GTK_SINK (vsink);

  if (gtk_sink->widget)
    gtk_gst_widget_set_buffer (gtk_sink->widget, buf);

  return GST_FLOW_OK;
}

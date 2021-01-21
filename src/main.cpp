/*
 * Copyright 2020 Xilinx Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <gst/gst.h>
#include <string>
#include <array>
#include <vector>
#include <sstream>
#include <memory>
#include <stdexcept>

static gchar** filenames = NULL;
static gchar** rtspaddrs = NULL;
static gint   fr = 30;
static gint mipi = -1;
static gint usb = -1;
static gint w = 1920;
static gint h = 1080;
static gboolean reportFps = FALSE;
static gboolean roiOff = FALSE;
static GOptionEntry entries[] =
{
    { "rtsp", 't', 0, G_OPTION_ARG_STRING_ARRAY, &rtspaddrs, "rtsp stream addr", "rtsp uri, multiple"},
    { "file", 'f', 0, G_OPTION_ARG_STRING_ARRAY, &filenames, "location of h26x file as input", "file path"},
//    { "width", 'W', 0, G_OPTION_ARG_INT, &w, "resolution w of the input", "1920"},
//    { "height", 'H', 0, G_OPTION_ARG_INT, &h, "resolution h of the input", "1080"},
//    { "framerate", 'r', 0, G_OPTION_ARG_INT, &fr, "framerate of the input", "30"},
//    { "report", 'R', 0, G_OPTION_ARG_NONE, &reportFps, "report fps", NULL },
    { NULL }
};

static gboolean
my_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_INFO:{
      GError *err;
      gchar *debug;
      gst_message_parse_info (message, &err, &debug);
      g_print ("Info: %s\n", debug);
      break;
    }
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;
      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", debug);
      break;
    }
    default:
      /* unhandled message */
      break;
  }

  return TRUE;
}



int
main (int argc, char *argv[])
{
    GMainLoop *loop;
    GOptionContext *optctx;
    GError *error = NULL;
    guint busWatchId;


    optctx = g_option_context_new ("- AI Application of pedestrian + reid + tracking for multi RTSP streams, on SoM board of Xilinx.");
    g_option_context_add_main_entries (optctx, entries, NULL);
    g_option_context_add_group (optctx, gst_init_get_option_group ());
    if (!g_option_context_parse (optctx, &argc, &argv, &error)) {
        g_printerr ("Error parsing options: %s\n", error->message);
        g_option_context_free (optctx);
        g_clear_error (&error);
        return -1;
    }
    g_option_context_free (optctx);


    std::string confdir("/opt/xilinx/share/aibox_aa2");
    char pip[12500];
    pip[0] = '\0';

    char *perf = (char*)"";

    char *rtspsrc = (char*)1, *filesrc = (char*)1;
    int indr = 0, indf = 0;
    int i = 0;
    for (i = 0; i < 4; i++) 
    {
        std::ostringstream srcOss;
        if (rtspaddrs && rtspsrc)
        {
            rtspsrc = rtspaddrs[indr++];
            if (!rtspsrc)
            {
                i--;
                continue;
            }
            else
            {
                srcOss << "rtspsrc location=\"" << rtspsrc << "\" ! queue ! rtph264depay ! queue ";
            }
        }
        else if (filenames && filesrc)
        {
            filesrc = filenames[indf++];
            if (!filesrc)
            {
                break;
            }
            else
            {
                srcOss << "filesrc location=\"" << filesrc << "\"";
            }
        }
        else
        {
            break;
        }

        sprintf(pip + strlen(pip),
                " %s \
                ! h264parse ! queue ! omxh264dec internal-entropy-buffers=3 \
                ! video/x-raw, format=NV12 \
                ! tee name=t%d t%d.src_0 ! queue \
                ! ivas_xmultisrc kconfig=\"%s/ped_pp.json\" \
                ! queue ! ivas_xfilter name=refinedet_%d kernels-config=\"%s/refinedet.json\" \
                ! queue ! ivas_xfilter name=crop_%d      kernels-config=\"%s/crop.json\" \
                ! queue ! ivas_xfilter kernels-config=\"%s/reid.json\" \
                ! queue ! scalem%d.sink_master ivas_xmetaaffixer name=scalem%d scalem%d.src_master \
                ! fakesink \
                t%d.src_1 \
                ! queue ! scalem%d.sink_slave_0 scalem%d.src_slave_0 \
                ! queue ! ivas_xfilter kernels-config=\"%s/draw_reid.json\" \
                ! queue ! %s \
                ", srcOss.str().c_str()
                , i, i
                , confdir.c_str()
                , i, confdir.c_str()
                , i, confdir.c_str()
                , confdir.c_str()
                , i, i, i
                , i
                , i, i
                , confdir.c_str()
                , (i==0) ? "kmssink driver-name=xlnx plane-id=39 sync=false fullscreen-overlay=true" : "fakesink"
               );
    }


    if (i > 0)
    {
        loop = g_main_loop_new (NULL, FALSE);
        GstElement *pipeline = gst_parse_launch(pip, NULL);
        gst_element_set_state (pipeline, GST_STATE_PLAYING);
        /* Wait until error or EOS */
        GstBus *bus = gst_element_get_bus (pipeline);
        busWatchId = gst_bus_add_watch (bus, my_bus_callback, loop);
        g_main_loop_run (loop);

        gst_object_unref (bus);
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_object_unref (pipeline);
        g_source_remove (busWatchId);
        g_main_loop_unref (loop);
    }
    else
    {
        printf("Error: No input source.\n", pip);
    }
    return 0;
}

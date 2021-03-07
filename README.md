
# Development Guide

   If you want to build from source, follow these steps, otherwise skip this section.

   1) Install the SoM sdk.sh to the path you choose or default. Suppose SDKPATH.
   2) Run "./build.sh ${SDKPATH}" to build the somapp application.
   3) The build process in 2) will produce a rpm package SoMApp-1.0.1-1.aarch64.rpm under build/, upload to the board,
      and run "rmp -ivh --force ./AIbox_aa2-1.0.1-1.aarch64.rpm" to update install.


# File structure

The application is installed as:

Binary File: => /opt/xilinx/bin

aibox_aa2                   main app

configuration File: => /opt/xilinx/share/aibox_aa2

|||
|-|-|
|ped_pp.json   |Config of preprocess for refinedet.
| refinedet.json   |           Config of refinedet.
| crop.json        |           Config of cropping for reid.
| reid.json        |           Config of reid.
| draw_reid.json   |           Config of final results drawing.

Example video files: => /usr/share/somapp/movies/AA2

# Fireware Loading

The accelerated application (AA) firmware consist of bitstream, device tree overlay (dtbo) and xclbinfile. The AA firmware is loaded dynamically on user request once Linux is fully booted. The xmutil utility can be used for that purpose.
   1. To list the available AA applications, run:

         `xmutil listapps`

         You should see similar output to this:

>         Accelerator,     Type,   Active
>         kv260-aa2,       flat,   0
>         kv260-aa1,       flat,   0

         The Active column shows which AA is currently loaded in the system. It will change to 1 after the firmware is loaded.

   2. To load the AA1 application firmware consisting of PL bitstream, device tree overlay and xclbin,

         run the following command:

         `xmutil loadapp kv260-aa2`

   3. After you are done running the application, you can unload the curently loaded AA application firmware by running:

         `xmutil unloadapp`

# How to run application:

## Prerequisites:

### Monitor

This application needs 4K monitor, so that up to 4 channels of 1080p video could be displayed.

Before booting the board, please connect the monitor to the board via either DP or HDMI port.

### Setup RTSP server

   * If no RTSP server on hand, smartcam_aa1 can be used as the RTSP server on the AA2 platform, with -n option, which turn off the server side AI inference.

      e.g. 

      `smartcam_aa1 -f /usr/share/somapp/movies/AA2/AA2-park.nv12.30fps.1080p.h264 -W 1920 -H 1080 -r 30 -t rtsp -p 5000 -n &`

      console output example:

      `stream ready at:`

      `rtsp://192.168.33.123:5000/test`

      **note** 192.168.33.123 will be your local network IP.

   * If using a direct connection (no DHCP) see AA1 read me on how to set IP address.

     To setup multiple servers you can use different "-p port" option to get different rtsp addresses.

   * You may also use other RTSP sources, such as an IP camera. 

   * Run AA2 application:

      * Run one channel with one process:
         aibox_aa2 -s rtsp://192.168.33.123:5000/test -t rtsp -p 0 

         * Run multiple channels together with one process:
               aibox_aa2 -s rtsp://192.168.33.123:5000/test -t rtsp -p 2 
                         -s /usr/share/somapp/movies/AA2/AA2-shop.nv12.30fps.1080p.h264 -t file -p 1

       Only one aibox_aa2 process could be actually running, because it requires locked access to DPU, and there is just one in aibox platform.

   * Known Limitations or issues under debug:

      * When you use smartcam_aa1 as file based RTSP server as [here](#Setup-RTSP-server) states or use file type source, because the video file is played in an infinite loop, you may notice that when a new loop starts the tracking ID of the same person will change to a new one. This is not the algorithm issue or bug, but because the algorithm will take the motion trail of the object into account, so a sudden change of scene will cause the generation of the new ID.

      * Stream may freeze after some time if you use this SOM as RTSP server as 3.b)i) and leave AA2 application running for extended period of time.

      * After start the application, Sometimes the screen still does not show anyting. To fix this, unplug and replug the DP/HDMI cable.

      * There is a small memory leak when aibox_aa2 is running.

      * When streaming from other RTSP sources (than the [locally started one](#setup-rtsp-server), you may notice some extra seconds of lag occassionally

      * When multiple streams are active, the image displayed can be laggy. 

4. Command Option:

>      Usage:
>
>     aibox_aa2 [OPTION?] - AI Application of pedestrian + reid + tracking for multi RTSP streams, on SoM board of 
>      Help Options:
>
>        -h, --help                                       Show help options
>        --help-all                                       Show all help options
>        --help-gst                                       Show GStreamer Options
>
>      Application Options:
>
>        -s, --src=[rtsp://server:port/id |file path]     URI of rtsp src, or location of h264|h265 video file. Must set. Can set up to 4 times
>        -t, --srctype=[f|file, r|rtsp]                   Type of the input source: file (f)|rtsp (r). Optional. Can set up to 4 times.
>        -e, --srcenc=[h264|h265]                         Encoding type of the input source. Optional. Can set up to 4 times.
>        -p, --pos=[0|1|2|3]                              Location of the display in the 4 grids of 4k monitor. Optional. 
>                                                         0: top left, 1: top right, 2: bottom left, 3: bottom right. Optional. Can set up to 4 times.
>        -R, --report                                     Report fps


<p align="center"><sup>Copyright&copy; 2021 Xilinx</sup></p>

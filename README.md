
# Development Guide

If you want to cross compile the source in Linux PC machine, follow these steps, otherwise skip this section.

1. Refer to the `K260 SOM Starter Kit Tutorial` to build the cross-compilation SDK, and install it to the path you choose or default. Suppose it's SDKPATH.

2. Run "./build.sh ${SDKPATH}" in the source code folder of current application, to build the application. <a name="build-app"></a>

3. The build process in [2](#build-app) will produce a rpm package aibox-reid-1.0.1-1.aarch64.rpm under build/, upload to the board, and run "rmp -ivh --force ./aibox-reid-1.0.1-1.aarch64.rpm" to update install.

# Setting up the Board

1. Hardware Setup:

    * Monitor:
    
      This application requires **4K** monitor, so that up to 4 channels of 1080p video could be displayed.

      Before booting the board, please connect the monitor to the board via either DP or HDMI port.
    
    * UART/JTAG interface:
    
      For interacting and seeing boot-time information, connect a USB debugger to the J4.
    
    * Network connection:
    
      Connect the Ethernet cable to your local network with DHCP enabled or a direct PC connection with a static IP configuration.
 
2.  Get the latest application package.

    1.  Get the list of available packages in the feed.

        `xmutil      getpkgs`

    2.  Install the package with dnf install:

        `sudo dnf install packagegroup-kv260-aibox-reid.noarch`

3.  Dynamically load the application package.

    The firmware consist of bitstream, device tree overlay (dtbo) and xclbin file. The firmware is loaded dynamically on user request once Linux is fully booted. The xmutil utility can be used for that purpose.

    1. Show the list and status of available acceleration platforms and AI Applications:

        `xmutil      listapps`

    2.  Switch to a different platform for different AI Application:

        *  When there's no active accelerator by inspecting with xmutil listapps, just active the one you want to switch.

            `xmutil      loadapp kv260-aibox-reid`

        *  When there's already an accelerator being activated, unload it first, then switch to the one you want.

            `xmutil      unloadapp `

            `xmutil      loadapp kv260-aibox-reid`

# How to run application:

## Two types of input source

The AIBOX application is targeted to run wih RTSP streams as input source, but for convienience, we also support video files source as input.

We assume the RTSP or video file to be **1080P H264/H265**

   * RTSP source
   
     IP camerars normally have configuration page to configure the RTSP stream related parameter, please refer to the manual of you camera, and configure it to **1080P H264/H265**, and get the RTSP URL to use as input parameter of the application as bellow.
     The URL is in the form of "rtsp://user:passwd@ip-address:port/name"


   * File source

      To be able to demostrate the function of the application in case you have no IP camera in hand, we support the file video source too.
  
      You can download video files from the following links, which is of MP4 format, you can transcode it to what we required with following command.
  
      > ffmpeg -i input-video.mp4 -c:v libx264 -pix_fmt nv12 -r 30 output.nv12.h264

      Demo videos:

      * https://pixabay.com/videos/liverpool-people-couple-pier-head-46090/
      * https://pixabay.com/videos/liverpool-pier-head-england-uk-46098/
      * https://pixabay.com/videos/spring-walk-park-trees-flowers-15252/
      * https://pixabay.com/videos/walking-people-city-bucharest-6099/

## There are two ways to interact with the application. 

### Juypter notebook.

    Use a web-browser (e.g. Chrome, Firefox) to interact with the platform.

    The Jupyter notebook URL can be find with command:

> sudo jupyter notebook list

    Output example:

> Currently running servers:
>
> `http://ip:port/?token=xxxxxxxxxxxxxxxxxx`  :: /opt/xilinx/share/notebooks

### Comman Line

**Notice** The application need to be ran with ***sudo*** .

#### Examples:

   * Run one channel RTSP stream 
      > sudo aibox-reid -s rtsp://192.168.3.123:5000/test -t rtsp -p 0 

   * Run one channel video file
      > sudo aibox-reid -s /tmp/movies/shop.nv12.30fps.1080p.h264 -t file -p 1

   * Run multiple channels
     > sudo aibox-reid -s rtsp://192.168.3.123:5000/test -t rtsp -p 2 
     >           -s /tmp/movies/shop.nv12.30fps.1080p.h264 -t file -p 1 

   **notice** Only one aibox-reid process could be actually running, because it requires locked access to DPU, and there is just one in aibox-reid platform.

#### Command Option:

The examples show the capability of the aibox-reid for specific configurations. User can get more and detailed application options as following by invoking 

`   aibox-reid --help`

```
   Usage:

   aibox-reid [OPTION?] - AI Application of pedestrian + reid + tracking for multi RTSP streams, on SoM board of 

   Help Options:

   -h, --help      Show help options

        --help-all                                       Show all help options
        --help-gst                                       Show GStreamer Options

   Application Options:

        -s, --src=[rtsp://server:port/id |file path]     URI of rtsp src, or location of h264|h265 video file. Must set. Can set up to 4 times
        -t, --srctype=[f|file, r|rtsp]                   Type of the input source: file (f)|rtsp (r). Optional. Can set up to 4 times.
        -e, --srcenc=[h264|h265]                         Encoding type of the input source. Optional. Can set up to 4 times.
        -p, --pos=[0|1|2|3]                              Location of the display in the 4 grids of 4k monitor. Optional. 
                                                         0: top left, 1: top right, 2: bottom left, 3: bottom right. Optional. Can set up to 4 times.
        -R, --report                                     Report fps
```

# Files structure of the application

* The application is installed as:

  * Binary File: => /opt/xilinx/bin

      | filename | description |
      |----------|-------------|
      |aibox-reid| main app|

  * configuration File: => /opt/xilinx/share/ivas/aibox-reid

      | filename | description |
      |-|-|
      |ped_pp.json       |           Config of preprocess for refinedet.
      | refinedet.json   |           Config of refinedet.
      | crop.json        |           Config of cropping for reid.
      | reid.json        |           Config of reid.
      | draw_reid.json   |           Config of final results drawing.

   * Jupyter notebook file: => /opt/xilinx/share/notebooks/aibox-reid

     | filename | description |
     |----------|-------------|
     |aibox-reid.ipynb | Jupyter notebook file for aibox-reid.|

<p align="center"><sup>Copyright&copy; 2021 Xilinx</sup></p>
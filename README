/******************************************************************************
* Copyright (C) 2010 - 2021 Xilinx, Inc. All rights reserved.
* SPDX-License-Identifier: Apache-2.0
******************************************************************************/

1. Development Guide

    If you want to cross compile the source in Linux PC machine, follow these steps, otherwise skip this section.

    1. Refer to the `K260 SOM Starter Kit Tutorial` to build the cross-compilation SDK, and install it to the path you choose or default. Suppose it's SDKPATH.

    2. Run "./build.sh ${SDKPATH}" in the source code folder of current application, to build the application.

    3. The build process in [1.2] will produce a rpm package aibox-reid-1.0.1-1.aarch64.rpm under build/, upload to the board, and run "rpm -ivh --force ./aibox-reid-1.0.1-1.aarch64.rpm" to update install.

2. Setting up the Board

    1. Get the SD Card Image from (http://xilinx.com/) and follow the instructions in UG1089 to burn the SD card. And install the SD card to J11.

    2. Hardware Setup:

        * Monitor:

          This application requires **4K** monitor, so that up to 4 channels of 1080p video could be displayed.

          Before booting the board, please connect the monitor to the board via either DP or HDMI port.

        * UART/JTAG interface:

          For interacting and seeing boot-time information, connect a USB debugger to the J4.

        * Network connection:

          Connect the Ethernet cable to your local network with DHCP enabled to install packages and run Jupyter Notebooks

    3. Power on the board, login with username `petalinux`, and you need to setup the password for the first time bootup.

    4.  Get the latest application package.

        1.  Get the list of available packages in the feed.

            `sudo xmutil      getpkgs`

        2.  Install the package with dnf install:

            `sudo dnf install packagegroup-kv260-aibox-reid.noarch`

          Note: For setups without access to the internet, it is possible to download and use the packages locally. Please refer to the `K260 SOM Starter Kit Tutorial` for instructions.

    5.  Dynamically load the application package.

        The firmware consist of bitstream, device tree overlay (dtbo) and xclbin file. The firmware is loaded dynamically on user request once Linux is fully booted. The xmutil utility can be used for that purpose.

        1. Shows the list and status of available acceleration platforms and AI Applications:

            `sudo xmutil      listapps`

        2.  Switch to a different platform for different AI Application:

            *  When xmutil listapps reveals that no accelerator is currently active, select the desired application:

                `sudo xmutil      loadapp kv260-aibox-reid`

            *  When xmutil listapps reveals that an accellerator is already active, unload it first, then select the desired application:

                `sudo xmutil      unloadapp `

                `sudo xmutil      loadapp kv260-aibox-reid`

3. How to run the application:

    1. Two types of input sources

        The AIBOX application is targeted to run with RTSP streams as input source, but for convienience, we also support video files as input.

        We assume the RTSP or video file to be **1080P H264/H265**

         * RTSP source

           IP camerars normally have a configuration page to configure the RTSP stream related parameters. Please refer to the manual of your camera, and configure it to **1080P H264/H265**, and get the RTSP URL to be used as input parameter for the AIBox application. The URL is in the form of "rtsp://user:passwd@ip-address:port/name"

         * File source

            To demonstrate the application in the case where no IP camera is available, a video source may be played from a file on the SD card instead.
            You can download video files from the following links, which is of MP4 format.

            * https://pixabay.com/videos/liverpool-people-couple-pier-head-46090/
            * https://pixabay.com/videos/liverpool-pier-head-england-uk-46098/
            * https://pixabay.com/videos/spring-walk-park-trees-flowers-15252/
            * https://pixabay.com/videos/walking-people-city-bucharest-6099/

            Then you need to transcode it to H264 file which is the supported input format.

            > ffmpeg -i input-video.mp4 -c:v libx264 -pix_fmt nv12 -r 30 output.nv12.h264

            Finally, please upload or copy these transcoded H264 files to the board, place it to somewhere under /home/petalinux, which is the home directory of the user you login as.

    2. Interacting with the application

        There are two ways to interact with application, via Jyputer notebook or Command line 

        1. Juypter notebook

            Use a web-browser (e.g. Chrome, Firefox) to interact with the platform.

            The Jupyter notebook URL can be found with command:

            > sudo jupyter notebook list

            Output example:

            > Currently running servers:
            >
            > `http://ip:port/?token=xxxxxxxxxxxxxxxxxx`  :: /opt/xilinx/share/notebooks

        2. Command Line

            **Note** The application needs to be run with ***sudo*** .

            * Examples:

                * Run one channel RTSP stream 
                   > sudo aibox-reid -s rtsp://username:passwd@ip_address:port/name -t rtsp -p 0 

                * Run one channel video file
                   > sudo aibox-reid -s /tmp/movies/shop.nv12.30fps.1080p.h264 -t file -p 1

                * Run multiple channels
                  > sudo aibox-reid -s rtsp://username:passwd@ip_address:port/name -t rtsp -p 2 -s /tmp/movies/shop.nv12.30fps.1080p.h264 -t file -p 1 

                **Note** Only one instance of aibox-reid application can run at a time because it requires exclusive access to a DPU engine and there is only one instance of DPU that exists in the aibox-reid platform.

            * Command Options:

              The examples show the capability of the aibox-reid for specific configurations. User can get more and detailed application options as following by invoking 

              `   aibox-reid --help`

              ```
                 Usage:

                 aibox-reid [OPTION?] - AI Application of pedestrian + reid + tracking for multi RTSP streams, on SoM board of Xilinx

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
                      -r, --framerate                                  Framerate of the input. Optional. Can set up to 4 times.
                      -R, --report                                     Report fps
              ```

4. Files structure

    The application is installed as:

    * Binary File Directory: /opt/xilinx/bin

        | filename    | description |
        |-------------|-------------|
        |aibox-reid   | main app    |

    * Configuration file directory: /opt/xilinx/share/ivas/aibox-reid

        | filename         | description                                  |
        |------------------|----------------------------------------------|
        |ped_pp.json       |           Config of preprocess for refinedet.
        | refinedet.json   |           Config of refinedet.
        | crop.json        |           Config of cropping for reid.
        | reid.json        |           Config of reid.
        | draw_reid.json   |           Config of final results drawing.

     * Jupyter Notebook Directory: /opt/xilinx/share/notebooks/aibox-reid

       | filename         | description |
       |------------------|-------------|
       |aibox-reid.ipynb  | Jupyter notebook file for aibox-reid.|

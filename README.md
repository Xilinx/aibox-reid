
# Development Guide

If you want to cross compile the source in Linux PC machine, follow these steps, otherwise skip this section.

1. Refer to the [K260 SOM Starter Kit Tutorial](https://xilinx.github.io/kria-apps-docs/main/build/html/docs/build_petalinux.html#build-the-sdk) to build the cross-compilation SDK, and install it to the path you choose or default. Suppose it's SDKPATH.

2. Run "./build.sh ${SDKPATH}" in the source code folder of current application, to build the application. <a name="build-app"></a>

3. The build process in [2](#build-app) will produce a rpm package aibox-reid-1.0.1-1.aarch64.rpm under build/, upload to the board, and run "rpm -ivh --force ./aibox-reid-1.0.1-1.aarch64.rpm" to update install.

# Setting up the Board

1. Get the SD Card Image from [Boot Image Site](https://xilinx.github.io/kria-apps-docs/2021.1/build/html/index.html) and follow the instructions in UG1089 to burn the SD card. And install the SD card to J11.

This guide and prebuilt is target for Ubuntu and 22.1. The previious version of this application (21.1) which is targeted to Petalinux is still available [online](https://xilinx.github.io/kria-apps-docs/2021.1/build/html/index.html).

2. Hardware Setup:

    * Monitor:
    
      This application requires **4K** monitor, so that up to 4 channels of 1080p video could be displayed.

      Before booting the board, please connect the monitor to the board via either DP or HDMI port.
    
    * UART/JTAG interface:
    
      For interacting and seeing boot-time information, connect a USB debugger to the J4.
    
    * Network connection:
    
      Connect the Ethernet cable to your local network with DHCP enabled to install packages and run Jupyter Notebooks
 
3. Power on the board, and booting your Starter Kit (Ubuntu):

   * Follow the instruction from the page below to boot linux

  	https://www.xilinx.com/products/som/kria/kr260-robotics-starter-kit/kr260-getting-started/booting-your-starter-kit.html

> **Note:** Steps under the section "Set up the Xilinx Development & Demonstration Environment for Ubuntu 22.04 LTS" may not be needed for TSN-ROS demo.

4. Set System Timezone and locale:

    * Set timezone

       ```bash
		sudo timedatectl set-ntp true
		sudo timedatectl set-timezone America/Los_Angeles
		timedatectl
       ```
	
	* Set locale

       ```bash
		sudo locale-gen en_US en_US.UTF-8
		sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
		export LANG=en_US.UTF-8
		locale
       ```

5. Update Bootfirmware

The SOM Starter Kits have factory pre-programmed boot firmware that is installed and maintained in the SOM QSPI device. Update the Boot firmware in the SOM QSPI device to '2022.1 Boot FW' Image.

Follow the link below to obtain Boot firmware binary and instructions to update QSPI image using xmutil, after linux boot.  

https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/1641152513/Kria+K26+SOM#Boot-Firmware-Updates

6. Install Docker from [here]{https://docs.docker.com/engine/install/ubuntu/}

7. Get the latest kv260-aibox-reid firmware package:

	* Add archive for the Xilinx Apps demo

      ```bash
      sudo add-apt-repository ppa:xilinx-apps
      sudo apt update
      sudo apt upgrade
      ```

	* Search package feed for packages compatible with Kv260

    ```bash
		ubuntu@kria:~$ sudo apt search xlnx-firmware-kv260
		Sorting... Done
		Full Text Search... Done
		xlnx-firmware-kv260-aibox-reid/jammy 0.1-0xlnx1 arm64 [installed]
		 FPGA firmware for Xilinx boards - kv260 aibox-reid application

		xlnx-firmware-kv260-benchmark-b4096/jammy 0.1-0xlnx1 arm64
		 FPGA firmware for Xilinx boards - kv260 benchmark-b4096 application

		xlnx-firmware-kv260-defect-detect/jammy 0.1-0xlnx1 arm64
		 FPGA firmware for Xilinx boards - kv260 defect-detect application

		xlnx-firmware-kv260-nlp-smartvision/jammy,now 0.1-0xlnx1 arm64 
		 FPGA firmware for Xilinx boards - kv260 nlp-smartvision application

		xlnx-firmware-kv260-smartcam/jammy 0.1-0xlnx1 arm64 
		 FPGA firmware for Xilinx boards - kv260 smartcam application
    ```

	* Install firmware binaries and restart dfx-mgr

       ```bash
      sudo apt install xlnx-firmware-kv260-aibox-reid
      sudo systemctl restart dfx-mgr.service
      ```

      > Note : Installing firmware binaries (xlnx-firmware-kv260-aibox-reid) causes dfx-mgr to crash and a restart is needed, which is listed in the known issues section. Once this is fixed an newer updates are available for dfx-manager, restart may not be needed.
  
8. Dynamically load the application package:

    The firmware consist of bitstream, device tree overlay (dtbo) file. The firmware is loaded dynamically on user request once Linux is fully booted. The xmutil utility can be used for that purpose.

    * Disable the desktop environment:

       ```bash
       sudo xmutil      desktop_disable
       ```

       After running the application, the desktop environment can be enable again with:

       ```bash
       sudo xmutil      desktop_enable
       ```

    * Show the list and status of available acceleration platforms :

       ```bash
      sudo xmutil listapps
        ```

   * Switch to a different platform for different AI Application:

      * When xmutil listapps reveals that no accelerator is currently active, just activate kv260-aibox-reid:

         ```
         sudo xmutil      dp_unbind
         sudo xmutil      loadapp kv260-aibox-reid
         sudo xmutil      dp_bind
         ```

      * When xmutil listapps reveals that there's already another accelerator being activated apart from kv260-aibox-reid, unload it first, then switch to kv260-aibox-reid.

         ```
         sudo xmutil      dp_unbind
         sudo xmutil      unloadapp
         sudo xmutil      loadapp kv260-aibox-reid
         sudo xmutil      dp_bind
         ```
9. Enable your user to properly use the docker commands without using sudo for every command. 

    ```bash
    sudo groupadd docker
    sudo usermod -a -G docker  $USER
	```

10. Pull the latest docker image for aibox-reid using the below command.

    ```bash
    docker pull xilinx/aibox-reid:latest
    ```

11. Launch the docker using the below command

    ```bash
    docker run \
    --env="DISPLAY" \
    -h "xlnx-docker" \
    --env="XDG_SESSION_TYPE" \
    --net=host \
    --privileged \
    --volume="$HOME/.Xauthority:/root/.Xauthority:rw" \
    -v /tmp:/tmp \
    -v /dev:/dev \
    -v /sys:/sys \
    -v /etc/vart.conf:/etc/vart.conf \
    -v /lib/firmware/xilinx:/lib/firmware/xilinx \
    -v /run:/run \
    -it xilinx/aibox-reid:latest bash
    ```

    It will launch the aibox-reid image in a new container

    ```bash
    root@xlnx-docker/#
    ```
12. The storage volume on the SD card is limited with multiple dockers. You can use following command to remove the existing container.

	```
    docker rmi --force aibox-reid
	```

# How to run the application:

## Two types of input sources

The AIBOX application is targeted to run with RTSP streams as input source, but for convienience, we also support video files as input.

We assume the RTSP or video file to be **1080P H264/H265 30FPS**. AIBOX application can adjust for other FPS with -r flag, but resolution must be 1080p. 

* RTSP source <a name="rtsp-source"> </a>

  * IP Camera

    IP cameras normally have a configuration page to configure the RTSP stream related parameters. Please refer to the manual of your camera, and configure it to **1080P H264/H265 30FPS**, and get the RTSP URL to be used as input parameter for the AIBox application. The URL is in the form of "rtsp://user:passwd@ip-address:port/name"

  * VLC player

    Alternatively, you can use VLC in windows to setup RTSP Streaming server. You must first turn off any firewalls (McAfee, etc) and VPN, and make sure your windows machine is on the same subnet as SOM board. In the folder with vlc.exe (typically C:\Program Files\VideoLAN\VLC), do a shift-rightclick and select "Open PowerShell window here". In PowerShell window, enter

    ```
    > .\vlc.exe -vvv path_to_a_mp4_file --sout '#rtp{dst=windows_ip_address,port=1234,sdp=rtsp://windows_ip_address/test.sdp}' --loop
    ```

* File source

  To demonstrate the application in the case where no IP camera is available, a video source may be played from a file on the SD card instead.

  You can download video files from the following links, which is of MP4 format.

  * https://pixabay.com/videos/liverpool-people-couple-pier-head-46090/
  * https://pixabay.com/videos/liverpool-pier-head-england-uk-46098/
  * https://pixabay.com/videos/spring-walk-park-trees-flowers-15252/
  * https://pixabay.com/videos/walking-people-city-bucharest-6099/

  Then you need to transcode it to H264 file which is the supported input format.

  ```
  ffmpeg -i input-video.mp4 -c:v libx264 -pix_fmt nv12 -vf scale=1920:1080 -r 30 output.nv12.h264
  ```

  Finally, upload or copy these transcoded H264 files to the board, place it to somewhere under /tmp.

## Interacting with the application

There are two ways to interact with application, via Jupyuter notebook or Command line

### Jupyter notebook

* User need to run following command to install the package shipped notebooks which reside in `/opt/xilinx/kv260-aibox-reid/share/notebooks` to the folder `$root/notebooks/aibox-reid`.

  ``` $ aibox-reid-install.py ```

  This script also provides more options to install the notebook of current application to specified location.

```
    usage: aibox-reid-install [-h] [-d DIR] [-f]

    Script to copy aibox-reid Jupyter notebook to user directory

    optional arguments:
      -h, --help         show this help message and exit
      -d DIR, --dir DIR  Install the Jupyter notebook to the specified directory.
      -f, --force        Force to install the Jupyter notebook even if the destination directory exists.
```

* Please get the list of running Jupyter servers with command:

    ```$ jupyter-server list ```

  Output example:

  > Currently running servers:
  >
  > `http://ip:port/?token=xxxxxxxxxxxxxxxxxx`  :: /root/notebooks/aibox-reid

* Stop the currently running server with command:

    ```$ jupyter-server stop 8888 ```

* To launch Jupyter notebook on the target, run below command.

``` bash
    python3 /usr/local/bin/jupyter-lab --notebook-dir=/root/notebooks/aibox-reid --allow-root --ip=ip-address &

    // fill in ip-address from ifconfig 
```

Output example:

``` bash
[I 2022-09-05 10:26:26.644 LabApp] JupyterLab extension loaded from /usr/local/lib/python3.10/dist-packages/jupyterlab
[I 2022-09-05 10:26:26.644 LabApp] JupyterLab application directory is /usr/local/share/jupyter/lab
[I 2022-09-05 10:26:26.664 ServerApp] jupyterlab | extension was successfully loaded.
[I 2022-09-05 10:26:26.683 ServerApp] nbclassic | extension was successfully loaded.
[I 2022-09-05 10:26:26.685 ServerApp] Serving notebooks from local directory: /root/notebooks/aibox-reid
[I 2022-09-05 10:26:26.685 ServerApp] Jupyter Server 1.18.1 is running at:
[I 2022-09-05 10:26:26.685 ServerApp] http://192.168.1.233:8888/lab?token=385858bbf1e5541dbba08d811bcac67d805b051ef37c6211
[I 2022-09-05 10:26:26.686 ServerApp]  or http://127.0.0.1:8888/lab?token=385858bbf1e5541dbba08d811bcac67d805b051ef37c6211
[I 2022-09-05 10:26:26.686 ServerApp] Use Control-C to stop this server and shut down all kernels (twice to skip confirmation).
[W 2022-09-05 10:26:26.702 ServerApp] No web browser found: could not locate runnable browser.
[C 2022-09-05 10:26:26.703 ServerApp]

    To access the server, open this file in a browser:
        file:///root/.local/share/jupyter/runtime/jpserver-40-open.html
    Or copy and paste one of these URLs:
        http://192.168.1.233:8888/lab?token=385858bbf1e5541dbba08d811bcac67d805b051ef37c6211
     or http://127.0.0.1:8888/lab?token=385858bbf1e5541dbba08d811bcac67d805b051ef37c6211
```

* User can access the server by opening the server URL from previous steps with the Chrome browser.

  In the notebook, we will construct the GStreamer pipeline string, you can get it by adding simple python code to print it out, and played with gst-launch-1.0 command in the console, and there are some user options variables that can be changed and run with. For other parts of the pipeline, you can also change and play to see the effect easily.

**Note:** [Known limitation](issue-aib.md#notebook-one-channel)

### Command Line

#### Examples:

* Run one channel RTSP stream

    > aibox-reid -s [rtsp://username:passwd@ip_address:port/name](#rtsp-source) -t rtsp -p 0

  or

    > aibox-reid -s [rtsp://username:passwd@ip_address:port](#rtsp-source) -t rtsp -p 0

  or (for windows VLC server setup):

    > aibox-reid -s rtsp://windows_ip_address:1234/test.sdp -t rtsp -p 0

* Run one channel video file

    > aibox-reid -s /tmp/movies/shop.nv12.30fps.1080p.h264 -t file -p 1

* Run multiple channels

  > aibox-reid -s [rtsp://username:passwd@ip_address:port/name](#rtsp-source) -t rtsp -p 2 -s /tmp/movies/shop.nv12.30fps.1080p.h264 -t file -p 1

**Note:**: Only one instance of aibox-reid application can run at a time because it requires exclusive access to a DPU engine and there is only one instance of DPU that exists in the aibox-reid platform.

#### Command Options:

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
        -e, --srcenc=[h264|h265]                         Encoding type of the input source. Optional and defaults to h264. Can set up to 4 times.
        -p, --pos=[0|1|2|3]                              Location of the display in the 4 grids of 4k monitor. Optional.
                                                         0: top left, 1: top right, 2: bottom left, 3: bottom right. Optional. Can set up to 4 times.
        -r, --framerate                                  Framerate of the input. Optional. Can set up to 4 times.
        -R, --report                                     Report fps
```

# Files structure

* The application is installed as:

  * Binary File Directory: /opt/xilinx/bin

      | filename | description |
      |----------|-------------|
      |aibox-reid| main app|

  * Configuration file directory: /opt/xilinx/share/vvas/aibox-reid

      | filename | description |
      |-|-|
      |ped_pp.json       |           Config of preprocess for refinedet.
      | refinedet.json   |           Config of refinedet.
      | crop.json        |           Config of cropping for reid.
      | reid.json        |           Config of reid.
      | draw_reid.json   |           Config of final results drawing.

  * Configuration File Directory: /opt/xilinx/kv260-aibox-reid/share/vitis_ai_library/models
    
      The model files integrated in the application use the B3136 DPU configuration.

      | foldername | description |
      |----------|-------------|
      |personreid-res18_pt | Model files for reid|
      |refinedet_pruned_0_96| Model files for refinedet| 	  

   * Jupyter Notebook Directory: /opt/xilinx/share/notebooks/aibox-reid

     | filename | description |
     |----------|-------------|
     |aibox-reid.ipynb | Jupyter notebook file for aibox-reid.|

<p align="center"><sup>Copyright&copy; 2022 Xilinx</sup></p>

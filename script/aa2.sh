# 

gst-launch-1.0 -v \
filesrc location="/usr/share/somapp/movies/walking-people.nv12.30fps.1080p.h264" \
  ! h264parse ! omxh264dec internal-entropy-buffers=3 \
  ! queue ! \
  tee name=t0 t0.src_0 ! \
    queue ! \
    ivas_xm2m kconfig="./aa2_ped_pp.json" ! \
    ivas_xfilter kernels-config="./kernel_refinedet_pruned_0_96.json" ! \
    scalem0.sink_master ivas_xmetaaffixer name=scalem0 scalem0.src_master ! \
  fakesink \
  t0.src_1 ! \
    scalem0.sink_slave_0 scalem0.src_slave_0 ! \
    queue \
    ! ivas_xfilter kernels-config="./kernel_crop.json" ! queue \
    ! ivas_xfilter kernels-config="./kernel_reid.json" ! queue \
    ! ivas_xfilter kernels-config="/opt/xilinx/share/ivas/kernel_boundingbox_facedetect.json" ! queue \
    ! kmssink driver-name=xlnx plane-id=39 sync=false fullscreen-overlay=true

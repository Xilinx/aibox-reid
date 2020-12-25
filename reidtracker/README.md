## require 
Before use reid tracker, you shoud install vitis-ai-library.   
Copy images form :/group/dphi_software/reidtracker_images

## build
cd reidtracker/  
./cmake.sh 
 
## exec 
find exec file:  
cd  $HOME/build/build.RedHatEnterpriseWorkstation.7.4.x86_64.Debug/reidtracker/test/   
(Different system environment compile path is different)  
test_imgs <$images path>  
eg: test_imgs reidtracker_images/

you will get tracker results on resultge.txt and video result results.avi.
# SimuSound on Raspberry Pi

## Requirements

- OpenCV headers and shared libs
- Tensorflow Lite

Libs and headers are now bundled with repo, no need to build

### Building OpenCV (not needed anymore)

Two options:

1. Build on rpi
    - Easy and will likely work first try
    - Very slow (2 hours or more on rpi 3B+)
    - Don't do this on SD card, use powered external hard drive or NFS/CIFS then copy built files over
    - Tutorial used: https://learnopencv.com/install-opencv-4-on-raspberry-pi/
    - Would like to skip `make install` step and distribute shared libs in same folder as app, but couldn't figure cmake out

2. Cross-compile
    - Much faster builds on PC or server
    - Could be hard to setup
    - Likely no need if we don't change OpenCV that often
    - Dockcross seems like good option for cross-compiling

### Building Tensorflow Lite (not needed anymore)

TODO

https://qengineering.eu/install-tensorflow-2-lite-on-raspberry-pi-4.html

## How to build SimuSound

```
mkdir build
cd build
cmake ..
make SimuSound_rpi
```

## How to build gattlib
cmake .. -DGATTLIB_BUILD_DOCS=OFF -DGATTLIB_SHARED_LIB=OFF -DGATTLIB_PYTHON_INTERFACE=OFF -DGATTLIB_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=./install_dir
make install

## Set up RPi CM4

```
get usb working, add to /boot/config:
dtoverlay=dwc2,dr_mode=host
OR
otg_mode=1

https://www.raspberrypi.org/documentation/hardware/computemodule/cmio-camera.md

raspi-config:
enable interfaces, do not enable i2c
set wifi
auto login to CLI, not GUI

https://askubuntu.com/a/966153/1375476 or add Disable=Headset

coral: run install_udev_rule.sh in edgetpu/misc with sudo
```

### headless gui (vnc)

after connecting via vnc, disable authentication with `xhost +`

#### tigervnc (faster)

```
sudo apt install tigervnc-standalone-server
vncserver -localhost no -geometry 1600x900 -depth 24
set password, say no to view only password
get a vnc client like tigervnc viewer, fastest settings are Tight encoding, no custom compression level, jpeg compression level 0
connect to <pi_ip_address>:5901
```

#### x11vnc

```
ssh in
sudo apt install xvfb x11vnc
x11vnc -create -forever -loop -noxdamage -repeat -shared &
get vnc client and connect to `<ip>:5900`, don't use Tight encoding for x11vnc, caused graphics glitches for me
sudo systemctl start lightdm
startlxde-pi &
```

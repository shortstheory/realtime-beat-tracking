# Realtime Beat Detection

## Introduction

Used for obtaining a visual representation of the FFT of a specified sample taken real time from the computer's microphone using the RtAudio API.

## Installation

First makes sure you have installed the required libraries using:

```
sudo apt install libfftw3-dev libfftw3-bin librtaudio-dev libsfml-dev cmake
```

Then run:

```
mkdir build
cd build
cmake ..
make
```

And then execute the program in the ```build/``` folder using:

```
./realtime-beat
```

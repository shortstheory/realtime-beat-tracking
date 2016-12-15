#include "RtAudio.h"
#include <fftw3.h>
#include <iostream>
#include <cstdlib>
#include <math.h>
#include <cstring>

unsigned int sampleRate = 44100;
unsigned int bufferFrames = 512; // 512 sample frames
const int bandNumber = 32;
unsigned int historyBins = sampleRate / (bufferFrames * 2 * 2);
int a = 0;
std::vector<signed short> window;
std::vector<double> outputHistory;
std::vector<double> meanHistory;
std::vector<double> v;

void fft(std::vector<signed short> &rawValues, std::vector<double> &output) //move this over to GPU_FFT
{
    int n = rawValues.size();
    int i;
    fftw_complex *inputChannel = new fftw_complex[n];
    fftw_complex *outputChannel = new fftw_complex[n];

    for (i = 0; i < n; i++) {
        inputChannel[i][0] = rawValues[i];
        inputChannel[i][1] = 0;
        outputChannel[i][0] = 0;
        outputChannel[i][1] = 0;
    }
    fftw_plan p = fftw_plan_dft_1d(n, inputChannel, outputChannel, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);
    for ( i = 0; i < n / 2; i++) {
        output.push_back(sqrt(outputChannel[i][0] * outputChannel[i][0] + outputChannel[i][1] * outputChannel[i][1]));
    }
    output[0] = 0;

    delete[] inputChannel;
    delete[] outputChannel;
}

float computeMean(std::vector<signed short> &v)
{
    std::cout << v.size() << std::endl;
    long sum = 0;
    for (auto it = v.begin(); it != v.end(); it++) {
        sum += (*it > 0) ? *it : *it*-1;
    }
    return (float)sum / (float)v.size();
}

std::vector<double> returnSubbands(/*std::vector<double> &subbands, */std::vector<double> &input, int bandNumber)
{
    int n = input.size();
    std::vector<double> subbands;
    int stepWidth = input.size() / bandNumber;
    int i;
    int j;
    double sum;
    for (i = 0; i < n; i += stepWidth) {
        sum = 0;
        for (j = 0; j < stepWidth; j++) {
            sum += input[i + j];
        }
        sum /= stepWidth;
        subbands.push_back(sum);
    }
    return subbands;
}

void processBuffer()
{
    int i;
    int j;

    int n = window.size() / 2;

    std::vector<double> output;
    fft(window, output);
    for (i = 0; i < n; i++) {
//        std::cout << i*44100.0/(n*2) << ' ' << (output[i]) << std::endl; //use log10 or not?
    }
    v = returnSubbands(output, bandNumber);
    //std::cout << historyBins << std::endl;

    for (i = 0; i < bandNumber; i++) {
        outputHistory.erase(outputHistory.begin() + i*historyBins);
        outputHistory.insert(outputHistory.begin() + (i+1)*historyBins - 1, v[i]);
    //    std::cout << i << ' ' << (v[i]) << std::endl;
    }
//    for (i = 0; i < outputHistory.size(); i++) {
    //    std::cout << i << ' ' << outputHistory[i] << std::endl;
//    }

    for (i = 0; i < (bandNumber / 2); i++) {
        double mean = 0;
        for (j = 0; j < historyBins; j++) {
            mean += outputHistory[i*bandNumber + j];
        }
        meanHistory[i] = (mean / historyBins);
        //std::cout << i << ' ' << meanHistory[i] << ' ' << v[i] << std::endl;
        if ((v[i]) > meanHistory[i] * 3) {
            std::cout << "beat " << (v[i]) << ' ' << meanHistory[i] << ' ' << i << std::endl;
        }
    }
}

int record(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
            double streamTime, RtAudioStreamStatus status, void *userData)
{
    if (status) {
        std::cout << "Stream overflow detected!" << std::endl;
    }

    int i = 0;
    signed short *a = (signed short*)inputBuffer;

    while (window.size() < nBufferFrames*2 && i < nBufferFrames) {
        window.push_back(a[i++]);
    }

    processBuffer();
//add a function for processing the data here
    if (window.size() == nBufferFrames*2) {
        window.erase(window.begin(), window.begin() + nBufferFrames);
    }

    return 0;
}

int main()
{
    RtAudio adc;
    if (adc.getDeviceCount() < 1) {
        std::cout << "No audio devices found!\n";
        return -1;
    }

    RtAudio::StreamParameters parameters;
    parameters.deviceId = adc.getDefaultInputDevice();
    parameters.nChannels = 1;
    parameters.firstChannel = 0;

    outputHistory.resize(historyBins * bandNumber/*, std::vector<double>(bufferFrames*/, 0);
    meanHistory.resize(bandNumber, 0);
    try {
        adc.openStream(NULL, &parameters, RTAUDIO_SINT16,
                        sampleRate, &bufferFrames, &record);
        adc.startStream();
        std::cout << adc.getVersion();
    } catch (RtAudioError& e) {
        e.printMessage();
        return -1;
    }

    char input;
    std::cout << "\nRecording ... press <enter> to quit.\n";
    std::cin.get( input );
    try {
        adc.stopStream();
    } catch (RtAudioError& e) {
        e.printMessage();
    }

    if (adc.isStreamOpen()) {
        adc.closeStream();
    }
    return 0;
}

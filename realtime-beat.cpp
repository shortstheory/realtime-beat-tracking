#include "RtAudio.h"
#include <fftw3.h>
#include <iostream>
#include <cstdlib>
#include <stdlib.h>
#include <string>
#include <math.h>
#include <SFML/Graphics.hpp>
#include <cstring>
#include <chrono>
#include <fstream>
unsigned int sampleRate = 44100;
unsigned int bufferFrames = 512; // 512 sample frames
const int bandNumber = 64;
const int width = bufferFrames / bandNumber;
const int historyValues = sampleRate / (bufferFrames * 2);
int a = 0;
std::vector<signed short> window;
std::vector<double> v;
std::vector<std::vector<double>> historyBuffer; //rows are frequency, cols are histories
std::vector<double> meanHistory(bandNumber);

std::chrono::steady_clock::time_point begin;
std::ofstream output("beats.txt");

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

double computeMean(std::vector<double> &v)
{
//    std::cout << v.size() << std::endl;
    double sum = 0;
    for (auto it = v.begin(); it != v.end(); it++) {
        sum += (*it > 0) ? *it : *it*-1;
    }
    return sum / v.size();
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
    if (historyBuffer.size() < bandNumber) {
        for (i = 0; i < bandNumber; i++) {
            std::vector<double> temp;
            temp.push_back(v[i]);
            historyBuffer.push_back(temp);
        }
    }
    if (historyBuffer[0].size() < historyValues) {
        for (i = 0; i < bandNumber; i++) {
            historyBuffer[i].push_back(v[i]);
        }
    } else {
        for (i = 0; i < bandNumber; i++) {
            historyBuffer[i].erase(historyBuffer[i].begin());
            historyBuffer[i].push_back(v[i]);
        }
    }
    for (i = 0; i < bandNumber; i++) {
        meanHistory[i] = computeMean(historyBuffer[i]);
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
    for (i = 0; i < bandNumber / 2; i++) {
        if (log10(v[i]) > log10(meanHistory[i]) * 1.2) {
//            std::cout << log10(v[i]) / log10(meanHistory[i]) << ' ' << "beat@" << ' ' << i << std::endl;
            std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - begin;
            output << elapsed_seconds.count() << std::endl;
            std::cout<<elapsed_seconds.count() << std::endl;
        }
    }
//add a function for processing the data here
    if (window.size() == nBufferFrames*2) {
        window.erase(window.begin(), window.begin() + nBufferFrames);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    RtAudio adc;
    if (adc.getDeviceCount() < 1) {
        std::cout << "No audio devices found!\n";
        return -1;
    }

    std::vector<sf::RectangleShape> bars(bandNumber), historyBars(bandNumber);
    int i;
    for (i = 0; i < bandNumber; i++) {
        bars[i].setFillColor(sf::Color(200, 000, 200));
        historyBars[i].setFillColor(sf::Color(0, 250, 0, 100));
    }


    RtAudio::StreamParameters parameters;
    if (argc > 1) {
        parameters.deviceId = atoi(argv[1]);
    } else {
        parameters.deviceId = adc.getDefaultInputDevice();
    }
    parameters.nChannels = 1;
    parameters.firstChannel = 0;

    try {
        adc.openStream(NULL, &parameters, RTAUDIO_SINT16,
                        sampleRate, &bufferFrames, &record);
        adc.startStream();
        std::cout << adc.getVersion();
    } catch (RtAudioError& e) {
        e.printMessage();
        return -1;
    }

    std::cout << "\nPress any letter and then press enter\n";
    std::cin.ignore();

    begin = std::chrono::steady_clock::now();
    sf::RenderWindow window(sf::VideoMode(1280, 900), "FFT visualiser");

    window.setVerticalSyncEnabled(true);
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }
        std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - begin;
        if (elapsed_seconds.count() > 5) {
            //cleanup
            output.close();
            return 0;
        }
        for (i = 0; i < bandNumber / 2; i++) {
            double height = log10(v[i]) * 100;
            double historyHeight = log10(meanHistory[i]) * 100;
            bars[i].setSize(sf::Vector2f(2*width, height));
            bars[i].setPosition(i*(width*2 + 1), 1000 - height);
            historyBars[i].setSize(sf::Vector2f(2*width, historyHeight));
            historyBars[i].setPosition(i*(width*2 + 1), 1000 - historyHeight);
        }
        //window.clear(sf::Color::Black)2
        window.clear();
        for (i = 0; i < bandNumber / 2; i++) {
            window.draw(bars[i]);
            window.draw(historyBars[i]);
        }
        window.display();
    }
    output.close();
    return 0;
}

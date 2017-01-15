#include "RtAudio.h"
#include <fftw3.h>
#include <iostream>
#include <cstdlib>
#include <math.h>
#include <SFML/Graphics.hpp>
#include <cstring>

const float pi = 3.14159265;

unsigned int sampleRate = 44100;
unsigned int bufferFrames = 512; // 512 sample frames
const int bandNumber = 128;
const int width = bufferFrames / bandNumber;
const int historyValues = sampleRate / (bufferFrames * 2);

const float nodeRadius = 100;
const float angularWidth = 2.0 * pi / bandNumber;
const float barWidth = angularWidth * nodeRadius;

int a = 0;
std::vector<signed short> window;
std::vector<double> v;
std::vector<std::vector<double>> historyBuffer; //rows are frequency, cols are histories
std::vector<double> meanHistory(bandNumber);

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

    std::vector<sf::RectangleShape> bars(bandNumber), historyBars(bandNumber);
    int i;
    for (i = 0; i < bandNumber; i++) {
        bars[i].setFillColor(sf::Color(200, (256 / bandNumber) * i, (256 / bandNumber) * i));
        historyBars[i].setFillColor(sf::Color(20, 40, 250, 100));
    }

    sf::CircleShape node(nodeRadius, 48);
    node.setFillColor(sf::Color::Black);
    node.setOrigin(nodeRadius, nodeRadius);

    sf::RectangleShape testShape;
    testShape.setFillColor(sf::Color::Yellow);

    RtAudio::StreamParameters parameters;
    parameters.deviceId = adc.getDefaultInputDevice();
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

    char input;
    std::cout << "\nRecording ... press <enter> to quit.\n";
//    std::cin.get( input );

    sf::RenderWindow window(sf::VideoMode(1280, 900), "FFT visualiser");

    window.setVerticalSyncEnabled(true);
    while (window.isOpen()) {

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }
        //gen shapes
        node.setPosition(640, 450);
        testShape.setSize(sf::Vector2f((int)barWidth, 200));
        for (i = 0; i < bandNumber; i++) {
            double height = log10(v[i]) * 50;
            double historyHeight = log10(meanHistory[i]) * 50;
            bars[i].setSize(sf::Vector2f(barWidth, height - 50));
            bars[i].setOrigin(bars[i].getSize().x / 2, bars[i].getSize().y);
            bars[i].setRotation(angularWidth * i * 180.0 / pi);
            bars[i].setPosition(node.getPosition().x + nodeRadius * sin(angularWidth * i), node.getPosition().y - nodeRadius * cos(angularWidth * i));

            historyBars[i].setSize(sf::Vector2f(barWidth, historyHeight - 50));
//            historyBars[i].setPosition(i*(width*2 + 1), 1000 - historyHeight);

            historyBars[i].setOrigin(historyBars[i].getSize().x / 2, historyBars[i].getSize().y);
            historyBars[i].setRotation(angularWidth * i * 180.0 / pi);
            historyBars[i].setPosition(node.getPosition().x + nodeRadius * sin(angularWidth * i), node.getPosition().y - nodeRadius * cos(angularWidth * i));
            if (height > historyHeight * 1.2) {
                bars[i].setFillColor(sf::Color(0, 200, 0));
            }
        }
//        testShape.setSize(sf::Vector2f(400, 400));
        testShape.setOrigin(testShape.getSize().x / 2, testShape.getSize().y);
        testShape.setRotation(30);
        testShape.setPosition(node.getPosition().x + nodeRadius * sin(pi/6), node.getPosition().y - nodeRadius * cos(pi/6));
        window.clear();
        //draw
//        window.draw(testShape);
        window.draw(node);
        for (i = 0; i < bandNumber; i++) {
            window.draw(bars[i]);
            window.draw(historyBars[i]);
        }
        window.display();

    }
    return 0;
}

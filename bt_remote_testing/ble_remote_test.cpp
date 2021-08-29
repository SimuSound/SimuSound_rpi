#include <cstdio>

#include <unistd.h>

#include "remote_control_io.h"

int main(int argc, char **argv)
{
    RemoteControlIO remote;
    RemoteInputData inputData;
    // RemoteInputData prevInputData;
    RemoteInputData inputDataAllFalse;
    RemoteOutputData outputData;

    inputData.Button1Pressed = false;
    inputData.Button2Pressed = false;
    inputData.Button3Pressed = false;
    inputData.Button4Pressed = false;
    inputData.Button5Pressed = false;

    // prevInputData = inputData;  // default copy constructor, I think
    inputDataAllFalse = inputData;

    outputData.vibrationIntensity = 0;

    remote.setup();

    for(int i = 0;; ++i)
    {
        remote.getInputData(inputData);
        printf("done read\n");

        if (inputData.Button1Pressed) {outputData.vibrationIntensity = 1;}
        else if (inputData.Button2Pressed) {outputData.vibrationIntensity = 2;}
        else if (inputData.Button3Pressed) {outputData.vibrationIntensity = 3;}
        else if (inputData.Button4Pressed) {outputData.vibrationIntensity = 4;}
        else if (inputData.Button5Pressed) {outputData.vibrationIntensity = 5;}
        else {outputData.vibrationIntensity = 0;}

        if (outputData.vibrationIntensity != 0)
        {
            remote.sendOutputData(outputData);
            printf("done write\n");
        }

        // prevInputData = inputData;
        inputData = inputDataAllFalse;

        usleep(200*1000);
    }

    return 0;
}
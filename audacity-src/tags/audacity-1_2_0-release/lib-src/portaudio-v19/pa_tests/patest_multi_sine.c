/** @file patest_multi_sine.c
	@brief Play a different sine wave on each channel.
	@todo needs to be updated to use the V19 API
	@author Phil Burk  http://www.softsynth.com
*/
/*
 * $Id: patest_multi_sine.c,v 1.1 2003-09-18 22:13:24 habes Exp $
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <math.h>
#include "portaudio.h"

#define OUTPUT_DEVICE       (Pa_GetDefaultOutputDevice())
//#define NON_INTERLEAVED
#define SAMPLE_RATE         (44100)
#define FRAMES_PER_BUFFER   (0) // take what we get(256)
#define FREQ_INCR           (300.0 / SAMPLE_RATE)
#define MAX_CHANNELS        (64)

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

typedef struct
{
    int      numChannels;
    double   phases[MAX_CHANNELS];
}
paTestData;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int patestCallback( void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           PaTimestamp outTime, void *userData )
{
    float **outputs = (float**)outputBuffer;

#ifdef NON_INTERLEAVED
    paTestData *data = (paTestData*)userData;
    float *out = (float*)outputBuffer;
    int frameIndex, channelIndex;
    int finished = 0;
    (void) outTime; /* Prevent unused variable warnings. */
    (void) inputBuffer;

    for( frameIndex=0; frameIndex<(int)framesPerBuffer; frameIndex++ )
    {
        for( channelIndex=0; channelIndex<data->numChannels; channelIndex++ )
        {
            /* Output sine wave on every channel. */
            outputs[channelIndex][frameIndex] = (float) sin(data->phases[channelIndex]);

            /* Play each channel at a higher frequency. */
            data->phases[channelIndex] += FREQ_INCR * (4 + channelIndex);
            if( data->phases[channelIndex] >= (2.0 * M_PI) ) data->phases[channelIndex] -= (2.0 * M_PI);
        }
    }
    
#else /* interleaved version */

    paTestData *data = (paTestData*)userData;
    float *out = (float*)outputBuffer;
    int frameIndex, channelIndex;
    int finished = 0;
    (void) outTime; /* Prevent unused variable warnings. */
    (void) inputBuffer;

    for( frameIndex=0; frameIndex<(int)framesPerBuffer; frameIndex++ )
    {
        for( channelIndex=0; channelIndex<data->numChannels; channelIndex++ )
        {
            /* Output sine wave on every channel. */
            *out++ = (float) sin(data->phases[channelIndex]);

            /* Play each channel at a higher frequency. */
            data->phases[channelIndex] += FREQ_INCR * (4 + channelIndex);
            if( data->phases[channelIndex] >= (2.0 * M_PI) ) data->phases[channelIndex] -= (2.0 * M_PI);
        }
    }
#endif /* NON_INTERLEAVED */
    return 0;
}

/*******************************************************************/
int main(void);
int main(void)
{
    PaStream *stream;
    PaError err;
    const PaDeviceInfo *pdi;
    paTestData data = {0};
    printf("PortAudio Test: output sine wave on each channel.\n" );

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    pdi = Pa_GetDeviceInfo( OUTPUT_DEVICE );
    data.numChannels = pdi->maxOutputChannels;
    if( data.numChannels > MAX_CHANNELS ) data.numChannels = MAX_CHANNELS;
    printf("Number of Channels = %d\n", data.numChannels );
    
    err = Pa_OpenStream(
              &stream,
              paNoDevice, /* default input device */
              0,              /* no input */
              paFloat32,  /* 32 bit floating point input */
              0, /* default input latency */
              NULL,
              OUTPUT_DEVICE,
              data.numChannels,
#ifdef NON_INTERLEAVED
              paFloat32 | paNonInterleaved,  /* 32 bit floating point output */
#else
              paFloat32,
#endif
              0, /* default output latency */
              NULL,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,  /* frames per buffer */
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              patestCallback,
              &data );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

    printf("Hit ENTER to stop sound.\n");
    getchar();

    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;

    Pa_CloseStream( stream );
    Pa_Terminate();
    printf("Test finished.\n");
    return err;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}

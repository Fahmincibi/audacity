#ifndef PA_CPULOAD_H
#define PA_CPULOAD_H
/*
 * $Id: pa_cpuload.h,v 1.1 2003-09-18 22:13:24 habes Exp $
 * Portable Audio I/O Library CPU Load measurement functions
 * Portable CPU load measurement facility.
 *
 * Based on the Open Source API proposed by Ross Bencina
 * Copyright (c) 2002 Ross Bencina
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
 */
 
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


typedef struct {
    double samplingPeriod;
    double measurementStartTime;
    double averageLoad;
} PaUtilCpuLoadMeasurer; /** @todo need better name than measurer */

void PaUtil_InitializeCpuLoadMeasurer( PaUtilCpuLoadMeasurer* measurer, double sampleRate );
void PaUtil_BeginCpuLoadMeasurement( PaUtilCpuLoadMeasurer* measurer );
void PaUtil_EndCpuLoadMeasurement( PaUtilCpuLoadMeasurer* measurer, unsigned long framesProcessed );
double PaUtil_GetCpuLoad( PaUtilCpuLoadMeasurer* measurer );


#ifdef __cplusplus
}
#endif /* __cplusplus */     
#endif /* PA_CPULOAD_H */

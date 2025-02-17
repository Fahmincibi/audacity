/*
 * $Id: pa_sgi.c,v 1.1 2003-09-18 22:13:24 habes Exp $
 * PortAudio Portable Real-Time Audio Library. Copyright (c) 1999-2001 Phil Burk.
 * Latest Version at: http://www.portaudio.com
 *
 * Silicon Graphics IRIX implementation by Pieter Suurmond, september 23, 2001.
 * pa_sgi-sub-version number 0.21, for PortAudio v15.
 * This implementation uses the sproc()-method, not the POSIX method.
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
Modfication History:
  8/12/2001 - Pieter Suurmond - took the v15 pa_linux_oss.c file and started to adapt for IRIX 6.2.
  8/17/2001 - v15 pa_sgi sub-version #0.04 (unstable alpha release) Sent to Phil & Ross. 
  9/23/2001 - #0.21 Many fixes and changes: POLLIN for input, not POLLOUT.
              Open and close ALports in the audio-process-thread.
              Proper semaphore communication now.
              Hopefully stable enough now (for beta release?).
TODO:
  - Test under IRIX 6.5.
  - Dynamically switch to 32 bit float as native format when appropriate (let SGI do the conversion), 
    and maybe also the other natively supported formats? (might increase performance)
  - The minimal number of buffers setting... I do not yet fully understand it.. I now just take *4.
REFERENCES:
  - IRIX 6.2 man pages regarding SGI AL library.
  - IRIS Digital MediaProgramming Guide (online books and man-pages come
    with IRIX 6.2 and may not be publically available on the internet).
*/

#include <stdio.h>              /* Standard libraries. */
#include <stdlib.h>

#include "../pa_common/portaudio.h"          /* BETTER PATH !!!???? Portaudio headers. */
#include "../pa_common/pa_host.h"
#include "../pa_common/pa_trace.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>       /* For schedctl(NDPRI, NDPHIMIN). */
#include <fcntl.h>              /* fcntl.h needed.                */
#include <unistd.h>             /* For streams, ioctl(), etc.     */
#include <ulocks.h>
#include <poll.h>
#include <dmedia/audio.h>      /* System specific (IRIX 6.2). */

/*--------------------------------------------*/
#define PRINT(x)   { printf x; fflush(stdout); }
#define ERR_RPT(x) PRINT(x)
#define DBUG(x)    /* PRINT(x) */
#define DBUGX(x)   /* PRINT(x) */

#define MAX_CHARS_DEVNAME           (16)        /* Was 32 in OSS (20 for AL but "in"/"out" is concat. */
#define MAX_SAMPLE_RATES            (8)         /* Known from SGI AL there are 7 (was 10 in OSS v15). */

typedef struct                    internalPortAudioDevice                 /* IRIX specific device info:      */
{
  PaDeviceID           /* NEW: */ pad_DeviceID;                           /* THIS "ID" IS NEW HERE (Pieter)! */
  long                            pad_ALdevice;                           /* SGI-number!                     */
  double                          pad_SampleRates[MAX_SAMPLE_RATES];      /* for pointing to from pad_Info   */
  char                            pad_DeviceName[MAX_CHARS_DEVNAME+1];    /* +1 for \0, one more than OSS.   */
  PaDeviceInfo                    pad_Info;                               /* pad_Info (v15) contains:        */
   	                              /* int            structVersion;                                           */
	                              /* const char*    name;                                                    */
	                              /* int            maxInputChannels, maxOutputChannels;                     */
	                              /* int            numSampleRates;       Num rates, or -1 if range supprtd. */
                                  /* const double*  sampleRates;          Array of supported sample rates,   */
	                              /* PaSampleFormat nativeSampleFormats;  or {min,max} if range supported.   */
  struct internalPortAudioDevice* pad_Next;                               /* Singly linked list, (NULL=end). */
} internalPortAudioDevice;

typedef struct      PaHostSoundControl              /* Structure to contain all SGI IRIX specific data. */
{
  ALconfig          pahsc_ALconfigIN,               /* IRIX-audio-library-datatype. Configuration       */
                    pahsc_ALconfigOUT;              /* stucts separate for input and output ports.      */
  ALport            pahsc_ALportIN,                 /* IRIX-audio-library-datatype. ALports can only be */
                    pahsc_ALportOUT;                /* unidirectional, so we sometimes need 2 of them.  */
  int               pahsc_threadPID;                /* Sproc()-result, written by PaHost_StartEngine(). */
  short             *pahsc_NativeInputBuffer,       /* Allocated here, in this file, if necessary.      */
                    *pahsc_NativeOutputBuffer;
  unsigned int      pahsc_BytesPerInputBuffer,      /* Native buffer sizes in bytes, really needed here */
                    pahsc_BytesPerOutputBuffer;     /* to free FAST memory, if buffs were alloctd FAST. */
  unsigned int      pahsc_SamplesPerInputBuffer,    /* These amounts are needed again and again in the  */
                    pahsc_SamplesPerOutputBuffer;   /* audio-thread (don't need to be kept globally).   */
  struct itimerval  pahsc_EntryTime,                /* For measuring CPU utilization (same as linux).   */
                    pahsc_LastExitTime;
  long              pahsc_InsideCountSum,
                    pahsc_TotalCountSum;
} PaHostSoundControl;

/*-------------------------------------------------------- Shared Data -------------------------------*/
static internalPortAudioDevice* sDeviceList = NULL;     /* FIXME - put Mutex around this shared data. */
static int                      sPaHostError = 0;       /* Maybe more than one process writing errs!? */
usema_t                         *SendSema,              /* These variables are shared between the     */
                                *RcvSema;               /* audio handling process and main process.   */
/*--------------------------*/
long Pa_GetHostError(void)
{
    return (long)sPaHostError;
}

/*----------------------------- BEGIN CPU UTILIZATION MEASUREMENT -----------------*/
/*                              (copied from source pa_linux_oss/pa_linux_oss.c)   */
static void Pa_StartUsageCalculation( internalPortAudioStream   *past )
{
	struct itimerval itimer;
	PaHostSoundControl *pahsc = (PaHostSoundControl *) past->past_DeviceData;
	if( pahsc == NULL ) return;
/* Query system timer for usage analysis and to prevent overuse of CPU. */
	getitimer( ITIMER_REAL, &pahsc->pahsc_EntryTime );
}

static long SubtractTime_AminusB( struct itimerval *timeA, struct itimerval *timeB )
{
	long secs = timeA->it_value.tv_sec - timeB->it_value.tv_sec;
	long usecs = secs * 1000000;
	usecs += (timeA->it_value.tv_usec - timeB->it_value.tv_usec);
	return usecs;
}

static void Pa_EndUsageCalculation( internalPortAudioStream   *past )
{
	struct itimerval currentTime;
	long  insideCount;
	long  totalCount;       /* Measure CPU utilization during this callback. */

#define LOWPASS_COEFFICIENT_0   (0.95)
#define LOWPASS_COEFFICIENT_1   (0.99999 - LOWPASS_COEFFICIENT_0)

	PaHostSoundControl *pahsc = (PaHostSoundControl *) past->past_DeviceData;
	if (pahsc == NULL)
        return;
	if (getitimer( ITIMER_REAL, &currentTime ) == 0 )
	    {
		if (past->past_IfLastExitValid)
		    {
			insideCount = SubtractTime_AminusB( &pahsc->pahsc_EntryTime, &currentTime );
			pahsc->pahsc_InsideCountSum += insideCount;
			totalCount =  SubtractTime_AminusB( &pahsc->pahsc_LastExitTime, &currentTime );
			pahsc->pahsc_TotalCountSum += totalCount;
		/*  DBUG(("insideCount = %d, totalCount = %d\n", insideCount, totalCount )); */
            /* Low pass filter the result because sometimes we get called several times in a row. */
            /* That can cause the TotalCount to be very low which can cause the usage to appear   */
            /* unnaturally high. So we must filter numerator and denominator separately!!!        */
 			if (pahsc->pahsc_InsideCountSum > 0)
			    {
				past->past_AverageInsideCount = ((LOWPASS_COEFFICIENT_0 * past->past_AverageInsideCount) +
					                             (LOWPASS_COEFFICIENT_1 * pahsc->pahsc_InsideCountSum));
				past->past_AverageTotalCount  = ((LOWPASS_COEFFICIENT_0 * past->past_AverageTotalCount) +
					                             (LOWPASS_COEFFICIENT_1 * pahsc->pahsc_TotalCountSum));
				past->past_Usage = past->past_AverageInsideCount / past->past_AverageTotalCount;
				pahsc->pahsc_InsideCountSum = 0;
				pahsc->pahsc_TotalCountSum = 0;
			    }
		    }
		past->past_IfLastExitValid = 1;
	    }
	pahsc->pahsc_LastExitTime.it_value.tv_sec = 100;
	pahsc->pahsc_LastExitTime.it_value.tv_usec = 0;
	setitimer( ITIMER_REAL, &pahsc->pahsc_LastExitTime, NULL );
	past->past_IfLastExitValid = 1;
}   /*----------- END OF CPU UTILIZATION CODE (from pa_linux_oss/pa_linux_oss.c v15)--------------------*/


/*--------------------------------------------------------------------------------------*/
PaError translateSGIerror(void) /* Calls oserror(), may be used after an SGI AL-library */
{                               /* call to report via ERR_RPT(), yields a PaError-num.  */
    const char* a = "SGI AL ";  /* (Not absolutely sure errno came from THIS thread!    */
    switch(oserror())           /* Read IRIX man-pages about the _SGI_MP_SOURCE macro.) */
        {
        case AL_BAD_OUT_OF_MEM:
             ERR_RPT(("%sout of memory.\n", a)); 
             return paInsufficientMemory;                   /* Known PaError.   */
        case AL_BAD_CONFIG:
             ERR_RPT(("%sconfiguration invalid or NULL.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_CHANNELS:
             ERR_RPT(("%schannels not 1,2 or 4.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_NO_PORTS:
             ERR_RPT(("%sout of audio ports.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_DEVICE:
             ERR_RPT(("%swrong device number.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_DEVICE_ACCESS:
             ERR_RPT(("%swrong device access.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_DIRECTION:
             ERR_RPT(("%sinvalid direction.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_SAMPFMT:
             ERR_RPT(("%sdoesn't accept sampleformat.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_FLOATMAX:
             ERR_RPT(("%smax float value is zero.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_WIDTH:
             ERR_RPT(("%sunsupported samplewidth.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_QSIZE:
             ERR_RPT(("%sinvalid queue size.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_PVBUFFER:
             ERR_RPT(("%sPVbuffer null.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_BUFFERLENGTH_NEG:
             ERR_RPT(("%snegative bufferlength.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_BUFFERLENGTH_ODD:
             ERR_RPT(("%sodd bufferlength.\n", a));
             return paHostError;                            /* Generic PaError. */
        case AL_BAD_PARAM:
             ERR_RPT(("%sparameter not valid for device.\n", a));
             return paHostError;                            /* Generic PaError. */
        default:
             ERR_RPT(("%sunknown error.\n", a));
             return paHostError;                            /* Generic PaError. */
        }
}

/*------------------------------------------------------------------------------------------*/
/* Tries to set various rates and formats and fill in the device info structure.            */
static PaError Pa_sgiQueryDevice(long                     ALdev,  /* (AL_DEFAULT_DEVICE)    */
                                 PaDeviceID               id,     /* (DefaultI|ODeviceID()) */
                                 char*                    name,   /* (for example "SGI AL") */
                                 internalPortAudioDevice* pad)    /* Result written to pad. */
{
	int     format;
    long    min, max;                           /* To catch hardware characteristics.       */
    ALseterrorhandler(0);                       /* 0 = turn off the default error handler.  */
    /*--------------------------------------------------------------------------------------*/
    pad->pad_ALdevice = ALdev;                              /* Set the AL device number.    */
    pad->pad_DeviceID = id;                                 /* Set the PA device number.    */
    if (strlen(name) > MAX_CHARS_DEVNAME)                   /* MAX_CHARS defined above.     */
        {
        ERR_RPT(("Pa_QueryDevice(): name too long (%s).\n", name));
        return paHostError;
        }
    strcpy(pad->pad_DeviceName, name);                      /* Write name-string.           */
	pad->pad_Info.name = pad->pad_DeviceName;               /* Set pointer,..hmmm.          */
    /*--------------------------------- natively supported sample formats: -----------------*/
    pad->pad_Info.nativeSampleFormats = paInt16; /* Later also include paFloat32 | ..| etc. */
                                                 /* Then also choose other CallConvertXX()! */
    /*--------------------------------- number of available i/o channels: ------------------*/
    if (ALgetminmax(ALdev, AL_INPUT_COUNT, &min, &max))
        return translateSGIerror();
	pad->pad_Info.maxInputChannels = max;
	DBUG(("Pa_QueryDevice: maxInputChannels = %d\n", pad->pad_Info.maxInputChannels))
    if (ALgetminmax(ALdev, AL_OUTPUT_COUNT, &min, &max))
        return translateSGIerror();
	pad->pad_Info.maxOutputChannels = max;
	DBUG(("Pa_QueryDevice: maxOutputChannels = %d\n", pad->pad_Info.maxOutputChannels))
    /*--------------------------------- supported samplerates: ----------------------*/ 
	pad->pad_Info.numSampleRates = 7;   
	pad->pad_Info.sampleRates = pad->pad_SampleRates;
    pad->pad_SampleRates[0] = (double)AL_RATE_8000;     /* long -> double. */
    pad->pad_SampleRates[1] = (double)AL_RATE_11025;
    pad->pad_SampleRates[2] = (double)AL_RATE_16000;
    pad->pad_SampleRates[3] = (double)AL_RATE_22050;
    pad->pad_SampleRates[4] = (double)AL_RATE_32000;
    pad->pad_SampleRates[5] = (double)AL_RATE_44100;
    pad->pad_SampleRates[6] = (double)AL_RATE_48000;
    if (ALgetminmax(ALdev, AL_INPUT_RATE, &min, &max))  /* Ask INPUT rate-max.       */
        return translateSGIerror();                     /* double -> long.           */
    if (max != (long)(0.5 + pad->pad_SampleRates[6]))   /* FP-compare not recommndd. */
        goto weird;
    if (ALgetminmax(ALdev, AL_OUTPUT_RATE, &min, &max)) /* Ask OUTPUT rate-max.      */
        return translateSGIerror();
    if (max != (long)(0.5 + pad->pad_SampleRates[6]))
        {
weird:  ERR_RPT(("Pa_sgiQueryDevice() did not confirm max samplerate (%ld)\n",max));
        return paHostError;             /* Or make it a warning and just carry on... */
        }
    /*-------------------------------------------------------------------------------*/ 
	return paNoError;
}


/*--------------------------------------------------------------------------------*/
int Pa_CountDevices()       /* Name of this function suggests it only counts and  */
{                           /* is NOT destructive, it however resets whole PA !   */   
    int                        numDevices = 0;        /* Let 's not do that here. */
    internalPortAudioDevice*   currentDevice = sDeviceList;   /* COPY GLOBAL VAR. */
#if 0                       /* Remains from linux_oss v15: Pa_Initialize(), on    */
    if (!currentDevice)     /* its turn, calls PaHost_Init() via file pa_lib.c.   */
        Pa_Initialize();    /* Isn't that a bit too 'rude'?        Don't be too   */
#endif                      /* friendly to clients that forgot to initialize PA.  */
    while (currentDevice)   /* Slower but more elegant than the sNumDevices-way:  */
        {
        numDevices++;
        currentDevice = currentDevice->pad_Next;
        }
	return numDevices;
}

/*-------------------------------------------------------------------------------*/
static internalPortAudioDevice *Pa_GetInternalDevice(PaDeviceID id)
{
    int                         numDevices = 0;
    internalPortAudioDevice     *res = (internalPortAudioDevice*)NULL;
	internalPortAudioDevice     *pad = sDeviceList;         /* COPY GLOBAL VAR.  */
    while (pad)                         /* pad may be NULL, that's ok, return 0. */
        {  /* (Added ->pad_DeviceID field to the pad-struct, Pieter, 2001.)      */
        if (pad->pad_DeviceID == id)    /* This the device we were looking for?  */
            res = pad;                  /* But keep on(!) counting so we don't   */
        numDevices++;                   /* have to call Pa_CountDevices() later. */
        pad = pad->pad_Next;            /* Advance to the next device or NULL.   */
        }                               /* No assumptions about order of ID's in */
    if (!res)                           /* the list.                             */
        ERR_RPT(("Pa_GetInternalDevice() could not find specified ID (%d).\n",id));
    if ((id < 0) || (id >= numDevices))
        {
        ERR_RPT(("Pa_GetInternalDevice() supplied with an illegal ID (%d).\n",id));
#if 1                                             /* Be strict, even when found, */
        res = (internalPortAudioDevice*)NULL;     /* do not accept illegal ID's. */
#endif
        }
	return res;
}

/*----------------------------------------------------------------------*/
const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceID id)
{
    PaDeviceInfo*             res = (PaDeviceInfo*)NULL;
	internalPortAudioDevice*  pad = Pa_GetInternalDevice(id);  /* Call. */
    if (pad)
	    res = &pad->pad_Info;   /* Not finding the specified ID is not  */
    if (!res)                   /* the same as &pad->pad_Info == NULL.  */
        ERR_RPT(("Pa_GetDeviceInfo() could not find it (ID=%d).\n", id));
    return res;                 /* So (maybe) a second/third ERR_RPT(). */
}

/*------------------------------------------------*/
PaDeviceID Pa_GetDefaultInputDeviceID(void)
{
	return 0;	/* 0 is the default device ID. */
}
/*------------------------------------------------*/
PaDeviceID Pa_GetDefaultOutputDeviceID(void)
{
	return 0;
}

/*-------------------------------------------------------------------------------------------------*/
/* Build linked a list with all the available audio devices on this SGI machine (only 1 for now).  */
PaError PaHost_Init(void)                              /* Called by Pa_Initialize() from pa_lib.c. */
{
    internalPortAudioDevice*    pad;
	PaError                     r = paNoError;
    int                         audioLibFileID;             /* To test for the presence of audio.  */

    if (sDeviceList)                                        /* Allow re-init, only warn, no error. */
        {
        ERR_RPT(("Warning: PaHost_Init() did not really re-init PA.\n"));
        return r;
        }
    /*------------- ADD THE SGI DEFAULT DEVICE TO THE LIST: ---------------------------------------*/
    audioLibFileID = open("/dev/hdsp/hdsp0master", O_RDONLY);   /* Try to open Indigo style audio  */
    if (audioLibFileID < 0)                                     /* IO port. On failure, machine    */
        {                                                       /* has no audio ability.           */
        ERR_RPT(("PaHost_Init(): This machine has no (Indigo-style) audio abilities.\n"));
        return paHostError;
        }
    close(audioLibFileID);                              /* Allocate fast mem to hold device info.  */
	pad = PaHost_AllocateFastMemory(sizeof(internalPortAudioDevice));
	if (pad == NULL)
        return paInsufficientMemory;
    memset(pad, 0, sizeof(internalPortAudioDevice));    /* "pad->pad_Next = NULL" is more elegant. */
	r = Pa_sgiQueryDevice(AL_DEFAULT_DEVICE,            /* Set AL device num (AL_DEFAULT_DEVICE).  */
                          Pa_GetDefaultOutputDeviceID(),/* Set PA device num (or InputDeviceID()). */
                          "AL default",                 /* A suitable name.                        */
                          pad);                         /* Write args and queried info into pad.   */
	if (r != paNoError)
		{
        ERR_RPT(("Pa_QueryDevice for '%s' returned: %d\n", pad->pad_DeviceName, r));
		PaHost_FreeFastMemory(pad, sizeof(internalPortAudioDevice));   /* sDeviceList still NULL ! */
		}
    else
        sDeviceList = pad;            /* First element in linked list. pad->pad_Next already NULL. */
    /*------------- QUERY AND ADD MORE POSSIBLE SGI DEVICES TO THE LINKED LIST: -------------------*/
    /*---------------------------------------------------------------------------------------------*/
	return r;
}

/*--------------------------------------------------------------------------------------------*/
#define MIN(a,b)    ((a)<(b)?(a):(b))   /* MIN()-function is used below.                      */
#define kPollSEMA   0                   /* To index the pollfd-array, reads nicer than just   */
#define kPollOUT    1                   /* numbers.                                           */
#define kPollIN     2
void Pa_SgiAudioProcess(void *v)        /* This function is sproc-ed by PaHost_StartEngine()  */
{                                       /* as a separate thread. (Argument must be void*).    */
    short                   evtLoop;    /* Reset by parent indirectly, or at local errors.    */
    PaError                 result;
    struct pollfd           PollFD[3];  /* To catch kPollSEMA-, kPollOUT- and kPollIN-events. */
    internalPortAudioStream *past  = (internalPortAudioStream*)v;   /* Copy void-ptr-argument.*/
    PaHostSoundControl      *pahsc;
    short                   inputEvent, outputEvent,   /* .revents members are of type short. */
                            semaEvent = 0;
    DBUG(("Entering sproc-thread.\n"));
    if (!past)
        {
        sPaHostError = paInternalError;                 /* Or paBadStreamPtr ? */
        ERR_RPT(("argument NULL!\n"));
        goto skip;
        }
    pahsc = (PaHostSoundControl*)past->past_DeviceData;
	if (!pahsc)
        {
        sPaHostError = paInternalError; /* The only way is to signal error to shared area?!   */
        ERR_RPT(("past_DeviceData NULL!\n"));
        goto skip;                      /* Sproc-ed threads MAY NOT RETURN paInternalError.   */
        }
    /*----------------------------- open AL-ports here, after sproc(): -----------------------*/
    if (past->past_NumInputChannels > 0)                                  /* Open input port. */
	    {	    
        pahsc->pahsc_ALportIN = ALopenport("PA sgi in", "r", pahsc->pahsc_ALconfigIN);
        if (!pahsc->pahsc_ALportIN)
            {
            ERR_RPT(("Failed to open AL input port.\n"));
            sPaHostError = paInternalError;
            goto skip;
            }
        DBUG(("Opened %d input channel(s).\n", past->past_NumInputChannels));
	    }
     if (past->past_NumOutputChannels > 0)                               /* Open output port. */
	    {	    
        pahsc->pahsc_ALportOUT = ALopenport("PA sgi out", "w", pahsc->pahsc_ALconfigOUT);
        if (!pahsc->pahsc_ALportOUT)
            {
            ERR_RPT(("Failed to open AL output port.\n"));
            sPaHostError = paInternalError;                 /* Assume pahsc_ALconfigs are the */
            goto skip;                                      /* same for IN and OUT in case    */
            }                                               /* both ports are opened (bidir). */
        DBUG(("Opened %d output channel(s).\n", past->past_NumOutputChannels));
	    }    
    /*-----------------------------------------------------------------------*/
	past->past_IsActive = 1;            /* Wasn't this already done by the calling parent?!   */
    PollFD[kPollIN].fd = ALgetfd(pahsc->pahsc_ALportIN);    /* ALgetfd returns -1 on failures */
    PollFD[kPollIN].events = POLLIN;                        /* such as ALport not there.      */
    PollFD[kPollOUT].fd = ALgetfd(pahsc->pahsc_ALportOUT);
    PollFD[kPollOUT].events = POLLOUT;                      /* .events = POLLOUT is OK.       */
    schedctl(NDPRI, NDPHIMIN);              /* Sets non-degrading priority for this process.  */
    PollFD[kPollSEMA].fd = usopenpollsema(SendSema, 0777);  /* To communicate with parent.    */
    PollFD[kPollSEMA].events = POLLIN;                      /* .events = POLLIN is OK.        */
    uspsema(SendSema);              /* Blocks until ... MUST be here, this uspsema(). */
    evtLoop = ((past->past_StopNow | past->past_StopSoon) == 0);
    while (evtLoop)
        {
        /*---------------------------- SET FILLPOINTS AND WAIT UNTIL SOMETHING HAPPENS: ----------*/
        if (pahsc->pahsc_NativeInputBuffer)         /* Then pahsc_ALportIN should also be there!  */
            /* For input port, fill point is number of locations in the sample queue that must be */
            /* filled in order to trigger a return from select(). (or poll())                     */
            /* Notice IRIX docs mention number of samples as argument, not number of sampleframes.*/
	        if (ALsetfillpoint(pahsc->pahsc_ALportIN, pahsc->pahsc_SamplesPerInputBuffer))
                {                                       /* Same amount as transferred per time.   */
                ERR_RPT(("ALsetfillpoint() for ALportIN failed.\n"));
                sPaHostError = paInternalError;         /* (Using exit(-1) would be a bit rude.)  */
                goto skip;
                }
        if (pahsc->pahsc_NativeOutputBuffer)        /* Then pahsc_ALportOUT should also be there! */
            /* For output port, fill point is number of locations that must be free in order to   */
            /* wake up from select(). (or poll())                                                 */
	        if (ALsetfillpoint(pahsc->pahsc_ALportOUT, pahsc->pahsc_SamplesPerOutputBuffer))
                {
                ERR_RPT(("ALsetfillpoint() for ALportOUT failed.\n"));
                sPaHostError = paInternalError;         /* (Using exit(-1) would be a bit rude.)  */
                goto skip;
                }                   /* poll() with timeout=-1 makes it block until a requested    */
	    poll(PollFD, 3, -1);        /* event occurs or until call is interrupted. If fd-value in  */
                                    /* array <0, events is ignored and revents is set to 0.       */
        /*---------------------------- MESSAGE-EVENT FROM PARENT THREAD: -------------------------*/
        semaEvent = PollFD[kPollSEMA].revents & POLLIN;
        if (semaEvent)
            {
            if (past->past_StopSoon)
                evtLoop = 0;
            if (past->past_StopNow)
                goto skip;
	        }
        /*------------------------------------- FILLED-EVENT FROM INPUT BUFFER: --------------------------*/
        inputEvent = PollFD[kPollIN].revents & POLLIN;
        if (inputEvent)         /* Don't need to check (pahsc->pahsc_NativeInputBuffer):  */
            {                   /* if buffer was not there, ALport not there, no events!  */
            if (ALreadsamps(pahsc->pahsc_ALportIN, (void*)pahsc->pahsc_NativeInputBuffer,
                            pahsc->pahsc_SamplesPerInputBuffer))
                {                           /* Here again: number of samples instead of number of frames. */
                ERR_RPT(("ALreadsamps() failed.\n"));
                sPaHostError = paInternalError;
                goto skip;
                }
            }
        outputEvent = PollFD[kPollOUT].revents & POLLOUT;
        /*------------------------------------- USER-CALLBACK-ROUTINE: -----------------------------------*/
        if (inputEvent | outputEvent)           /* (Bitwise is ok.) */
            { /* To be sure we that really DID input-transfer or gonna DO output-transfer, and that it is */
              /* not just "sema"- (i.e. user)-event, or some other system-event that awakened the poll(). */
		    Pa_StartUsageCalculation(past);                         /* Convert 16 bit native data to      */
		    result = Pa_CallConvertInt16(past,                      /* user data and call user routine.   */
				                         pahsc->pahsc_NativeInputBuffer,
				                         pahsc->pahsc_NativeOutputBuffer);
		    Pa_EndUsageCalculation(past);
		    if (result) 
		        {
			    DBUG(("Pa_CallConvertInt16() returned %d, stopping...\n", result));
                goto skip;                      /* This is apparently NOT an error!       */
		        }                               /* Just letting the userCallBack stop us. */
            }                 
        /*------------------------------------- FREE-EVENT FROM OUTPUT BUFFER: ---------------------------*/
        if (outputEvent)        /* Don't need to check (pahsc->pahsc_NativeOutputBuffer)  */
            {                   /* because if filedescriptor not there, no event for it.  */
            if (ALwritesamps(pahsc->pahsc_ALportOUT, (void*)pahsc->pahsc_NativeOutputBuffer,
                             pahsc->pahsc_SamplesPerOutputBuffer))
                {
                ERR_RPT(("ALwritesamps() failed.\n"));  /* Better use SEMAS for messaging back to parent! */
                sPaHostError = paInternalError;
                goto skip;
                }
	        }
        }
skip:
    /*------------------------------- close AL-ports ----------------------------*/
    if (pahsc->pahsc_ALportIN)
        {
        if (ALcloseport(pahsc->pahsc_ALportIN))
            translateSGIerror();   /* Translates SGI AL-code to PA-code and ERR_RPTs string. */
        else                                /* But go on anyway... to release other stuff... */
            pahsc->pahsc_ALportIN = (ALport)0;
        }
    if (pahsc->pahsc_ALportOUT)
        {
        if (ALcloseport(pahsc->pahsc_ALportOUT))
            translateSGIerror();
        else
            pahsc->pahsc_ALportOUT = (ALport)0;
        }
    past->past_IsActive = 0;
    if (semaEvent)
        {
        uspsema(SendSema);  /* StopEngine() was still waiting for this acknowledgement. */
	    usvsema(RcvSema);   /* (semaEvent initialized with 0.)          */
        }
    DBUG(("Leaving sproc-thread.\n"));
}


#define kALinternalQueuesizeFact    4L               /* Internal queue 4 times as large as transferSize. */
                                                     /* Used below, twice: for input and for output.     */
/*--------------------------------------------------------------------------------------*/
PaError PaHost_OpenStream(internalPortAudioStream *past)
{
	PaError                 result = paNoError;
	PaHostSoundControl      *pahsc;
	unsigned int            minNumBuffers;
	internalPortAudioDevice *padIN, *padOUT;        /* For looking up native AL-numbers. */
    long                    pvbuf[8];               /* To get/set hardware configs.      */
    long                    sr;
    DBUG(("PaHost_OpenStream() called.\n"));        /* Alloc FASTMEM and init host data. */
    if (!past)
        {
        ERR_RPT(("Streampointer NULL!\n"));
        result = paBadStreamPtr;        goto done;
        }
	pahsc = (PaHostSoundControl*)PaHost_AllocateFastMemory(sizeof(PaHostSoundControl));
	if (pahsc == NULL)
	    {
        ERR_RPT(("FAST Memory allocation failed.\n"));  /* Pass trough some ERR_RPT-exit-  */
        result = paInsufficientMemory;  goto done;      /* code (nothing will be freed).   */
	    }
	memset(pahsc, 0, sizeof(PaHostSoundControl));
    pahsc->pahsc_threadPID = -1;                    /* Should pahsc_threadPID be inited to */
	past->past_DeviceData = (void*)pahsc;           /* -1 instead of 0 ??                  */
    /*------------------------------------------ Manipulate hardware if necessary and allowed: --*/
    ALseterrorhandler(0);                           /* 0 = turn off the default error handler.   */
    pvbuf[0] = AL_INPUT_RATE;
    pvbuf[2] = AL_INPUT_COUNT;
    pvbuf[4] = AL_OUTPUT_RATE;              /* TO FIX: rates may be logically, not always in Hz! */
    pvbuf[6] = AL_OUTPUT_COUNT;
    sr = (long)(past->past_SampleRate + 0.5);   /* Common for input and output :-)               */
    /*---------------------------------------------------- SET INPUT CONFIGURATION: ------------------------*/
    if (past->past_NumInputChannels > 0)                                /* We need to lookup the corre-  */
        {                                                               /* sponding native AL-number(s). */
        /*--------------------------------------------------- Allocate native buffers: --------*/
        pahsc->pahsc_SamplesPerInputBuffer = past->past_FramesPerUserBuffer * /* Needed by the */
                                             past->past_NumInputChannels;     /* audio-thread. */
	    pahsc->pahsc_BytesPerInputBuffer   = pahsc->pahsc_SamplesPerInputBuffer * sizeof(short);	    
		pahsc->pahsc_NativeInputBuffer = (short*)PaHost_AllocateFastMemory(pahsc->pahsc_BytesPerInputBuffer);        
		if (!pahsc->pahsc_NativeInputBuffer)
		    {
            ERR_RPT(("Fast memory allocation failed (in).\n"));
			result = paInsufficientMemory; 
            goto done;
		    }
        padIN = Pa_GetInternalDevice(past->past_InputDeviceID);
        if (!padIN)
            {
            ERR_RPT(("Pa_GetInternalDevice() for input failed.\n"));
	        result = paHostError;
            goto done;
            }
        if (ALgetparams(padIN->pad_ALdevice, &pvbuf[0], 4)) /* Although input and output will both be on */
            goto sgiError;                                  /* the same AL-device, the AL-library might  */
        if (pvbuf[1] != sr)                                 /* contain more than AL_DEFAULT_DEVICE in    */
            {  /* Rate different from current harware-rate?    the future. Therefore 2 seperate queries. */
            if (pvbuf[3] > 0)     /* Means, there's other clients using AL-input-ports */
                {
                ERR_RPT(("Sorry, not allowed to switch input-hardware to %ld Hz because \
another process is currently using input at %ld kHz.\n", sr, pvbuf[1]));
                result = paHostError;
                goto done;
                }
            pvbuf[1] = sr;                  /* Then set input-rate. */
            if (ALsetparams(padIN->pad_ALdevice, &pvbuf[0], 2))
                goto sgiError;        /* WHETHER THIS SAMPLERATE WAS REALLY PRESENT IN OUR ARRAY OF RATES, */
            }                         /* IS NOT CHECKED, AT LEAST NOT BY ME, WITHIN THIS FILE! Does PA do? */
        pahsc->pahsc_ALconfigIN = ALnewconfig();                       /* Released at PaHost_CloseStream().  */
        if (pahsc->pahsc_ALconfigIN == (ALconfig)0)
            goto sgiError;
        if (ALsetsampfmt(pahsc->pahsc_ALconfigIN, AL_SAMPFMT_TWOSCOMP))/* Choose paInt16 as native i/o-format.      */
            goto sgiError;
        if (ALsetwidth (pahsc->pahsc_ALconfigIN, AL_SAMPLE_16))        /* Only meaningful when sample format for    */
            goto sgiError;                                      /* config is set to two's complement format. */
        /************************ Future versions might (dynamically) switch to 32-bit floats? *******
        if (ALsetsampfmt(pahsc_ALconfigIN, AL_SAMPFMT_FLOAT))    (Then also call another CallConvert-func.)
            goto sgiError;
        if (ALsetfloatmax (pahsc_ALconfigIN, 1.0))       Only meaningful when sample format for config 
            goto sgiError;                               is set to AL_SAMPFMT_FLOAT or AL_SAMPFMT_DOUBLE. */
        /*--------- Set internal AL queuesize (in samples) -------------------------------*/
        if (ALsetqueuesize(pahsc->pahsc_ALconfigIN, (long)pahsc->pahsc_SamplesPerInputBuffer *
                                                    (long)kALinternalQueuesizeFact))    
            goto sgiError;                              /* Or should we use past_NumUserBuffers here?     */
                                                        /* Do 4 timea, using 2 times may give glitches.   */
        if (ALsetchannels (pahsc->pahsc_ALconfigIN, (long)(past->past_NumInputChannels)))
            goto sgiError;                              /* Returns 0 on success, -1 on failure. */
        }
    /*---------------------------------------------------- SET OUTPUT CONFIGURATION: ------------------------*/
    if (past->past_NumOutputChannels > 0)               /* CARE: padOUT/IN may NOT be NULL if Channels <= 0! */
        {                                               /* We use padOUT/IN later on, or at least 1 of both. */
        pahsc->pahsc_SamplesPerOutputBuffer = past->past_FramesPerUserBuffer * /* Needed by the */
                                              past->past_NumOutputChannels;    /* audio-thread. */
	    pahsc->pahsc_BytesPerOutputBuffer   = pahsc->pahsc_SamplesPerOutputBuffer * sizeof(short);

	    pahsc->pahsc_NativeOutputBuffer = (short*)PaHost_AllocateFastMemory(pahsc->pahsc_BytesPerOutputBuffer);
	    if (!pahsc->pahsc_NativeOutputBuffer)
		    {
            ERR_RPT(("Fast memory allocation failed (out).\n"));
		    result = paInsufficientMemory;
            goto done;
		    }
        padOUT = Pa_GetInternalDevice(past->past_OutputDeviceID);
        if (!padOUT)
            {
            ERR_RPT(("Pa_GetInternalDevice() for output failed.\n"));
	        result = paHostError;
            goto done;
            }
        if (ALgetparams(padOUT->pad_ALdevice,&pvbuf[4], 4))
            goto sgiError;
        if (pvbuf[5] != sr)
            {   /* Output needed and rate different from current harware-rate. */
            if (pvbuf[7] > 0)     /* Means, there's other clients using AL-output-ports */
                {
                ERR_RPT(("Sorry, not allowed to switch output-hardware to %ld Hz because \
another process is currently using output at %ld kHz.\n", sr, pvbuf[5]));            
                result = paHostError;
                goto done;                              /* Will free again the inputbuffer */
                }                                       /* that was just created above.    */
            pvbuf[5] = sr;                  /* Then set output-rate. */
            if (ALsetparams(padOUT->pad_ALdevice, &pvbuf[4], 2))
                goto sgiError;
            }
        pahsc->pahsc_ALconfigOUT = ALnewconfig();                          /* Released at PaHost_CloseStream().  */
        if (pahsc->pahsc_ALconfigOUT == (ALconfig)0)
            goto sgiError;
        if (ALsetsampfmt(pahsc->pahsc_ALconfigOUT, AL_SAMPFMT_TWOSCOMP))   /* Choose paInt16 as native i/o-format.      */
            goto sgiError;
        if (ALsetwidth (pahsc->pahsc_ALconfigOUT, AL_SAMPLE_16))           /* Only meaningful when sample format for    */
            goto sgiError;                                          /* config is set to two's complement format. */
        /************************************ Future versions might (dynamically) switch to 32-bit floats? *******/
        if (ALsetqueuesize(pahsc->pahsc_ALconfigOUT, (long)pahsc->pahsc_SamplesPerOutputBuffer *
                                                     (long)kALinternalQueuesizeFact))    
            goto sgiError;                          /* Or should we use past_NumUserBuffers here?*/
        if (ALsetchannels (pahsc->pahsc_ALconfigOUT, (long)(past->past_NumOutputChannels)))
            goto sgiError;
        }
    /*---------- ?? --------------------*/
	/* DBUG(("PaHost_OpenStream: pahsc_MinFramesPerHostBuffer = %d\n", pahsc->pahsc_MinFramesPerHostBuffer )); */
    minNumBuffers = Pa_GetMinNumBuffers(past->past_FramesPerUserBuffer, past->past_SampleRate);
	past->past_NumUserBuffers = (minNumBuffers > past->past_NumUserBuffers) ? 
                                 minNumBuffers : past->past_NumUserBuffers; /* I don't yet use past_NumUserBuffers */
    /*----------------------------------------------- TEST DEVICE ID's: --------------------*/
	if ((past->past_OutputDeviceID != past->past_InputDeviceID) &&          /* Who SETS these devive-numbers? */
	    (past->past_NumOutputChannels > 0) && (past->past_NumInputChannels > 0))
	    { 
        ERR_RPT(("Cannot setup bidirectional stream between different devices.\n"));
        result = paHostError;
        goto done;
        }
    goto done;  /* (no errors occured) */
sgiError:   
    result = translateSGIerror();   /* Translates SGI AL-code to PA-code and ERR_RPTs string. */
done:
    if (result != paNoError)
        PaHost_CloseStream(past);               /* Frees memory (only if really allocated!).  */
    return result;
}

/*-----------------------------------------------------*/
PaError PaHost_StartOutput(internalPortAudioStream *past)
{
	return paNoError;   /* Hmm, not implemented yet? */
}
PaError PaHost_StartInput(internalPortAudioStream *past)
{
	return paNoError;
}

/*------------------------------------------------------------------------------*/
PaError PaHost_StartEngine(internalPortAudioStream *past)
{
	PaHostSoundControl  *pahsc;
    usptr_t             *arena;
    if (!past)          /* Test argument. */
        {
        ERR_RPT(("PaHost_StartEngine(NULL)!\n"));
        return paBadStreamPtr;
        }
    pahsc = (PaHostSoundControl*)past->past_DeviceData;
    if (!pahsc)
        {
        ERR_RPT(("PaHost_StartEngine(arg): arg->past_DeviceData = NULL!\n"));
        return paHostError;
        }
	past->past_StopSoon = 0;            /* Assume SGI ALport is already opened! */
	past->past_StopNow  = 0;            /* Why don't we check pahsc for NULL?   */
    past->past_IsActive = 1;
    
    /* Although the pthread_create() function, as well as <pthread.h>, may be   */
    /* available in IRIX, use sproc() on SGI to create audio-background-thread. */
    /* (Linux/oss uses pthread_create() instead of __clone() because:           */
    /*  - pthread_create also works for other UNIX systems like Solaris,        */
    /*  - Java HotSpot VM crashes in pthread_setcanceltype() using __clone().)  */
    
    usconfig(CONF_ARENATYPE, US_SHAREDONLY);    /* (From SGI-AL-examples, file  */
    arena = usinit(tmpnam(0));                  /*  motifexample.c, function    */
    SendSema = usnewpollsema(arena, 0);         /*  InitializeAudioProcess().)  */
    RcvSema = usnewsema(arena, 1);              /* 1= common mutual exclusion semaphore, where 1 and only 1 process
                                                   will be permitted through a semaphore at a time.  Values > 1
                                                   imply that up to val resources may be simultaneously used, but requests
                                                   for more than val resources cause the calling process to block until a
                                                   resource comes free (by a process holding a resource performing a
                                                   usvsema().                IS THIS usnewsema() TOO PLATFORM SPECIFIC? */
    prctl(PR_SETEXITSIG, 0);                    /* No not (void*)9, but 0, which doesn't kill the parent! */
                                                /* PR_SETEXITSIG controls whether all members of a share group will be
                                                   signaled if any one of them leaves the share group (either via exit()
                                                   or exec()).  If 2nd arg, interpreted as an int is 0, then normal IRIX
                                                   process termination rules apply, namely that the parent is sent a 
                                                   SIGCLD upon death of child, but no indication of death of parent is 
                                                   given.  If the second argument is a valid signal number then if any 
                                                   member of a share group leaves the share group, a signal is
                                                   sent to ALL surviving members of the share group.  */
    /* SPAWN AUDIO-CHILD: */
    pahsc->pahsc_threadPID = sproc(Pa_SgiAudioProcess, /* Returns process ID of */
                                   PR_SALL,            /* new process, or -1.   */
                                   (void*)past);       /* Pass past as optional */  /* IS THIS SAFE, will past never */
    if (pahsc->pahsc_threadPID == -1)                  /* third void-ptr-arg.   */  /* be moved around in memory???? */
        {
        ERR_RPT(("PaHost_StartEngine() failed to spawn audio-thread.\n"));
        sPaHostError = oserror();   /* Pass native error-number to shared area. */
        return paHostError;         /* But return the generic error-number.     */
        }
    return paNoError;               /* Hmmm, errno may come from other threads in same group! */
}                                   /* ("man sproc" in IRIX6.2 to read about _SGI_MP_SOURCE.) */

/*------------------------------------------------------------------------------*/
PaError PaHost_StopEngine(internalPortAudioStream *past, int abort)
{
	int                 hres;
	long                timeOut;
	PaError             result = paNoError;
	PaHostSoundControl  *pahsc;
    
    DBUG(("PaHost_StopEngine() called.\n"));
    if (!past)
        return paBadStreamPtr;
    pahsc = (PaHostSoundControl*)past->past_DeviceData;
                                                    /* Prevent from doing this twice!! */
    if ((!pahsc) ||                                 /* Some tests call this CLOSE twice!! */
        (!past->past_IsActive) || 
        past->past_StopSoon || past->past_StopNow)
        return result;              /* paNoError (already stopped, no err?).       */
    past->past_StopSoon = 1;        /* Tell background thread to stop generating   */
    if (abort)                      /* more and to let current data play out. If   */
        past->past_StopNow = 1;     /* aborting, tell backgrnd thread to stop NOW! */
                                    /*---- USE SEMAPHORE LOCK TO COMMUNICATE: -----*/
    usvsema(SendSema);              /* Increments count associated with SendSema.  */
                                    /* Wait for the response.                      */
    uspsema(RcvSema);               /* Decrements count of previously allocated    */
                                    /* semaphore specified by RcvSema.             */
    while (past->past_IsActive)     /* REALLY WAIT. */
        {
        /* DBUG(("wait 1 ms for audio-thread to stop.\n")); */
        Pa_Sleep(1);
        }

#if 0   /* We don't need to KILL(), just COMMUNICATE and be patient...             */
	if (pahsc->pahsc_threadPID != -1)   /* Did we really init it to -1 somewhere?  */
	    {
        DBUG(("PaHost_StopEngine() is about to kill(SIGKILL) audio-thread.\n"));
        if (kill(pahsc->pahsc_threadPID, SIGKILL))  /* Or SIGTERM or SIGQUIT(core)  */
            {                                       /* Returns -1 in case of error. */
            result = paHostError;
            sPaHostError = oserror();   /* Hmmm, other threads may also write here! */
            ERR_RPT(("PaHost_StopEngine() failed to kill audio-thread.\n"));
            }
        else
            pahsc->pahsc_threadPID = -1;    /* Notify that we've killed this thread. */
	    }
#endif
    past->past_IsActive = 0;    /* Even when kill() failed and pahsc_threadPID still there??? */
	return result;
}

/*---------------------------------------------------------------*/
PaError PaHost_StopOutput(internalPortAudioStream *past, int abort)
{
	return paNoError;                   /* Not implemented yet? */
}
PaError PaHost_StopInput(internalPortAudioStream *past, int abort )
{
	return paNoError;
}

/*******************************************************************/
PaError PaHost_CloseStream(internalPortAudioStream *past)
{
	PaHostSoundControl  *pahsc;
    PaError             result = paNoError;
    
    DBUG(("PaHost_CloseStream() called.\n"));
	if (!past)
        return paBadStreamPtr;
	pahsc = (PaHostSoundControl *) past->past_DeviceData;
	if (!pahsc)             /* If pahsc not NULL, past_DeviceData will be freed, and set to NULL. */
        return result;      /* This test prevents from freeing NULL-pointers. */

    if (pahsc->pahsc_ALconfigIN)
        {                                   /* Release configuration structs, only if allocated. */
        ALfreeconfig(pahsc->pahsc_ALconfigIN);
        pahsc->pahsc_ALconfigIN = NULL;
        }
    if (pahsc->pahsc_ALconfigOUT)
        {
        ALfreeconfig(pahsc->pahsc_ALconfigOUT); /* (Al-ports were already closed by audioProcess). */
        pahsc->pahsc_ALconfigOUT = NULL;
        }
	if (pahsc->pahsc_NativeInputBuffer)
	    {
        PaHost_FreeFastMemory(pahsc->pahsc_NativeInputBuffer, pahsc->pahsc_BytesPerInputBuffer);        
		pahsc->pahsc_NativeInputBuffer = NULL;
	    }
	if (pahsc->pahsc_NativeOutputBuffer)
	    {
        PaHost_FreeFastMemory(pahsc->pahsc_NativeOutputBuffer, pahsc->pahsc_BytesPerOutputBuffer);
		pahsc->pahsc_NativeOutputBuffer = NULL;
	    }
	PaHost_FreeFastMemory(pahsc, sizeof(PaHostSoundControl));
    past->past_DeviceData = NULL;    /* PaHost_OpenStream() allocated FAST MEM. */
	return result;
}

/*************************************************************************
** Determine minimum number of buffers required for this host based
** on minimum latency. Latency can be optionally set by user by setting
** an environment variable. For example, to set latency to 200 msec, put:
**    set PA_MIN_LATENCY_MSEC=200
** in the AUTOEXEC.BAT file and reboot.
** If the environment variable is not set, then the latency will be 
** determined based on the OS. Windows NT has higher latency than Win95.
*/
#define PA_LATENCY_ENV_NAME  ("PA_MIN_LATENCY_MSEC")

int Pa_GetMinNumBuffers( int framesPerBuffer, double sampleRate )
{
	return 2;
}
/* Hmmm, the note above isn't appropriate for SGI I'm afraid... */
/* Do we HAVE to do it this way under IRIX???....               */
/*--------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
PaError PaHost_Term(void)   /* Frees all of the linked audio-devices.  */
{                           /* Called by Pa_Terminate() from pa_lib.c. */
	internalPortAudioDevice *pad = sDeviceList,
                            *nxt;
	while (pad)
	    {
		DBUG(("PaHost_Term: freeing %s\n", pad->pad_DeviceName));
        nxt = pad->pad_Next;
		PaHost_FreeFastMemory(pad, sizeof(internalPortAudioDevice));
		pad = nxt;              /* PaHost_Init allocated this FAST MEM.*/
	    }
	sDeviceList = (internalPortAudioDevice*)NULL;
	return 0;                           /* Got rid of	sNumDevices=0; */
}

/***********************************************************************/
void Pa_Sleep( long msec )  /* Sleep requested number of milliseconds. */
{
#if 0
	struct timeval timeout;
	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;
	select(0, NULL, NULL, NULL, &timeout);
#else
	long usecs = msec * 1000;
	usleep( usecs );
#endif
}

/*---------------------------------------------------------------------------------------*/
/* Allocate memory that can be accessed in real-time. This may need to be held in physi- */
/* cal memory so that it is not paged to virtual memory. This call MUST be balanced with */
/* a call to PaHost_FreeFastMemory().                                                    */
void *PaHost_AllocateFastMemory(long numBytes)
{
	void *addr = malloc(numBytes);  /* mpin() reads into memory all pages over the given */
	if (addr)                       /* range and locks the pages into memory. A counter  */
        {                           /* is incremented each time the page is locked. The  */
        if (mpin(addr, numBytes))   /* superuser can lock as many pages as it wishes,    */
            {                       /* others are limited to the configurable PLOCK_MA.  */
            ERR_RPT(("PaHost_AllocateFastMemory() failed to mpin() memory.\n"));
#if 1
            free(addr);             /* You MAY cut out these 2 lines to be less strict,  */
            addr = NULL;            /* you then only get the warning but PA goes on...   */
#endif                              /* Only problem then may be corresponding munpin()   */
            }                       /* call at PaHost_FreeFastMemory(), below.           */
        memset(addr, 0, numBytes);  /* Locks established with mlock are not inherited by */
        }                           /* a child process after a fork. Furthermore, IRIX-  */
	return addr;                    /* man-pages warn against mixing both mpin and mlock */
}                                   /* in 1 piece of code, so stick to mpin()/munpin() ! */


/*---------------------------------------------------------------------------------------*/
/* Free memory that could be accessed in real-time. This call MUST be balanced with a    */
/* call to PaHost_AllocateFastMemory().                                                  */
void PaHost_FreeFastMemory(void *addr, long numBytes)
{
	if (addr)
        {
        if (munpin(addr, numBytes))     /* Will munpin() fail when it was never mpinned? */
            ERR_RPT(("WARNING: PaHost_FreeFastMemory() failed to munpin() memory.\n"));
        free(addr);                     /* But go on, try to release it, just warn...    */
        }
}

/*----------------------------------------------------------*/
PaError PaHost_StreamActive( internalPortAudioStream   *past )
{
	PaHostSoundControl *pahsc;
	if (past == NULL)
        return paBadStreamPtr;
	pahsc = (PaHostSoundControl *) past->past_DeviceData;
	if (pahsc == NULL)
        return paInternalError;
	return (PaError)(past->past_IsActive != 0);
}

/*-------------------------------------------------------------------*/
PaTimestamp Pa_StreamTime( PortAudioStream *stream )
{
	internalPortAudioStream *past = (internalPortAudioStream *) stream; 
/* FIXME - return actual frames played, not frames generated.
** Need to query the output device somehow.
*/
	return past->past_FrameCount;
}

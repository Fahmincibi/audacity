/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: window functions
 last mod: $Id: window.h,v 1.4.4.1 2004-07-30 06:57:30 mbrubeck Exp $

 ********************************************************************/

#ifndef _V_WINDOW_
#define _V_WINDOW_

extern float *_vorbis_window(int type,int left);
extern void _vorbis_apply_window(float *d,float *window[2],long *blocksizes,
				 int lW,int W,int nW);


#endif

/**********************************************************************

  Audacity: A Digital Audio Editor

  ODComputeSummaryTask.cpp

  Created by Michael Chinen (mchinen) on 8/10/08.
  Audacity(R) is copyright (c) 1999-2008 Audacity Team.
  License: GPL v2.  See License.txt.

******************************************************************//**

\class ODComputeSummaryTask
\brief Computes the summary data for a PCM (WAV) file and writes it to disk,
updating the ODPCMAliasBlockFile and the GUI of the newly available data.

*//*******************************************************************/

#include "ODDecodeTask.h"
#include "../blockfile/ODDecodeBlockFile.h"
#include <wx/wx.h>

///Creates a new task that computes summaries for a wavetrack that needs to be specified through SetWaveTrack()
ODDecodeTask::ODDecodeTask()
{
   mMaxBlockFiles = 0;
   mComputedBlockFiles = 0;
}
 
     
///Computes and writes the data for one BlockFile if it still has a refcount. 
void ODDecodeTask::DoSomeInternal()
{
   if(mBlockFiles.size()<=0)
   {
      mPercentCompleteMutex.Lock();
      mPercentComplete = 1.0;
      mPercentCompleteMutex.Unlock();
      return;
   }
   
   ODDecodeBlockFile* bf;
   sampleCount blockStartSample;
   sampleCount blockEndSample;
   bool success =false;
   
   for(size_t i=0; i < mWaveTracks.size() && mBlockFiles.size();i++)
   {
      bf = mBlockFiles[0];
      
      //first check to see if the ref count is at least 2.  It should have one 
      //from when we added it to this instance's mBlockFiles array, and one from
      //the Wavetrack/sequence.  If it doesn't it has been deleted and we should forget it.
      if(bf->RefCount()>=2)
      {
         //OD TODO: somehow pass the bf a reference to the decoder that manages it's file.
         bf->SetODFileDecoder(GetOrCreateMatchingFileDecoder(bf));
         bf->DoWriteBlockFile();
         success = true;
         blockStartSample = bf->GetStart();
         blockEndSample = blockStartSample + bf->GetLength();
         mComputedBlockFiles++;
      }
      else
      {
         //the waveform in the wavetrack now is shorter, so we need to update mMaxBlockFiles
         //because now there is less work to do.
         mMaxBlockFiles--;
      }
      
      //Release the refcount we placed on it.
      bf->Deref();
      //take it out of the array - we are done with it.
      mBlockFiles.erase(mBlockFiles.begin());
      
      //upddate the gui for all associated blocks.  It doesn't matter that we're hitting more wavetracks then we should
      //because this loop runs a number of times equal to the number of tracks, they probably are getting processed in
      //the next iteration at the same sample window.
      mWaveTrackMutex.Lock();
      for(size_t i=0;i<mWaveTracks.size();i++)
      {
         if(success && mWaveTracks[i])
            mWaveTracks[i]->AddInvalidRegion(blockStartSample,blockEndSample);
      }
      mWaveTrackMutex.Unlock();
   }   
   
   //update percentage complete.
   CalculatePercentComplete();
}

void ODDecodeTask::CalculatePercentComplete()
{
   mPercentCompleteMutex.Lock();
   mPercentComplete = (float) 1.0 - ((float)mBlockFiles.size() / (mMaxBlockFiles+1));
   mPercentCompleteMutex.Unlock();
}

///by default creates the order of the wavetrack to load.
void ODDecodeTask::Update()
{

   std::vector<ODDecodeBlockFile*> tempBlocks;
   
   mWaveTrackMutex.Lock();
   
   for(size_t j=0;j<mWaveTracks.size();j++)
   {
      if(mWaveTracks[j])
      {
         WaveClip *clip;
         BlockArray *blocks;
         Sequence *seq;
         
         //gather all the blockfiles that we should process in the wavetrack.
         WaveClipList::compatibility_iterator node = mWaveTracks[j]->GetClipIterator();
         
         int numBlocksDone;         
         while(node) {
            clip = node->GetData();
            seq = clip->GetSequence();
            //TODO:this lock is way to big since the whole file is one sequence.  find a way to break it down.
            seq->LockDeleteUpdateMutex();
            
            //See Sequence::Delete() for why need this for now..
            blocks = clip->GetSequenceBlockArray();
            int i;
            int numBlocksIn;
            int insertCursor;
            
            numBlocksIn=0;
            
            insertCursor =0;//OD TODO:see if this works, removed from inner loop (bfore was n*n)
            for(i=0; i<(int)blocks->GetCount(); i++)
            {
               //since we have more than one ODBlockFile, we will need type flags to cast.
               if(!blocks->Item(i)->f->IsDataAvailable() && ((ODDecodeBlockFile*)blocks->Item(i)->f)->GetDecodeType()==this->GetDecodeType())
               {
                  blocks->Item(i)->f->Ref();
                  ((ODDecodeBlockFile*)blocks->Item(i)->f)->SetStart(blocks->Item(i)->start);
                  ((ODDecodeBlockFile*)blocks->Item(i)->f)->SetClipOffset((sampleCount)(clip->GetStartTime()*clip->GetRate()));
                  
                  //these will always be linear within a sequence-lets take advantage of this by keeping a cursor.
                  while(insertCursor<(int)tempBlocks.size()&& 
                     (sampleCount)(tempBlocks[insertCursor]->GetStart()+tempBlocks[insertCursor]->GetClipOffset()) < 
                        (sampleCount)(((ODDecodeBlockFile*)blocks->Item(i)->f)->GetStart()+((ODDecodeBlockFile*)blocks->Item(i)->f)->GetClipOffset()))
                     insertCursor++;
                  
                  
                  tempBlocks.insert(tempBlocks.begin()+insertCursor++,(ODDecodeBlockFile*)blocks->Item(i)->f);
                  numBlocksIn++;
               }
            }   
            numBlocksDone = numBlocksIn;
            
            seq->UnlockDeleteUpdateMutex();
            node = node->GetNext();
         }
      }
   }
   mWaveTrackMutex.Unlock();
   
   //get the new order.
   OrderBlockFiles(tempBlocks);
}



///Orders the input as either On-Demand or default layered order.
void ODDecodeTask::OrderBlockFiles(std::vector<ODDecodeBlockFile*> &unorderedBlocks)
{
   //we are going to take things out of the array.  But first deref them since we ref them when we put them in.
   for(unsigned int i=0;i<mBlockFiles.size();i++)
      mBlockFiles[i]->Deref();
   mBlockFiles.clear();
   //TODO:order the blockfiles into our queue in a fancy convenient way.  (this could be user-prefs)
   //for now just put them in linear.  We start the order from the first block that includes the ondemand sample
   //(which the user sets by clicking.)   note that this code is pretty hacky - it assumes that the array is sorted in time.
   
   //find the startpoint
   sampleCount processStartSample = GetDemandSample(); 
   for(int i= ((int)unorderedBlocks.size())-1;i>= 0;i--)
   {
      //check to see if the refcount is at least two before we add it to the list.
      //There should be one Ref() from the one added by this ODTask, and one from the track.  
      //If there isn't, then the block was deleted for some reason and we should ignore it.
      if(unorderedBlocks[i]->RefCount()>=2)
      {
         if(mBlockFiles.size() && (unorderedBlocks[i]->GetStart()+unorderedBlocks[i]->GetClipOffset()) + unorderedBlocks[i]->GetLength() >=processStartSample && 
                ( (mBlockFiles[0]->GetStart()+mBlockFiles[0]->GetClipOffset()) +  mBlockFiles[0]->GetLength() < processStartSample || 
                  (unorderedBlocks[i]->GetStart()+unorderedBlocks[i]->GetClipOffset()) <= (mBlockFiles[0]->GetStart() +mBlockFiles[0]->GetClipOffset()))
            )
         {
            //insert at the front of the list if we get blockfiles that are after the demand sample
            mBlockFiles.insert(mBlockFiles.begin()+0,unorderedBlocks[i]);
         }
         else
         {
            //otherwise no priority
            mBlockFiles.push_back(unorderedBlocks[i]);
         }
         if(mMaxBlockFiles< (int) mBlockFiles.size())
            mMaxBlockFiles = mBlockFiles.size();
      }
      else
      {
         //Otherwise, let it be deleted and forget about it.
         unorderedBlocks[i]->Deref();
      }
   }
   
}  

void ODDecodeTask::ODUpdate()
{
   //clear old blockFiles and do something smarter.
   
}   



///there could be the ODBlockFiles of several FLACs in one track (after copy and pasting)
///so we keep a list of decoders that keep track of the file names, etc, and check the blocks against them.
///Blocks that have IsDataAvailable()==false are blockfiles to be decoded.  if BlockFile::GetDecodeType()==ODDecodeTask::GetDecodeType() then
///this decoder should handle it.  Decoders are accessible with the methods below.  These aren't thread-safe and should only
///be called from the decoding thread.
ODFileDecoder* ODDecodeTask::GetOrCreateMatchingFileDecoder(ODDecodeBlockFile* blockFile)
{
   ODFileDecoder* ret=NULL;
   //see if the filename matches any of our decoders, if so, return it.
   for(int i=0;i<(int)mDecoders.size();i++)
   {
      if(strcmp(mDecoders[i]->GetFileName(),blockFile->GetAudioFileName().GetFullPath().mb_str()) ==0)
      {
         ret = mDecoders[i];
         break;
      }
   }
   
   //otherwise, create and add one, and return it.
   if(!ret)
   {
      ret=CreateFileDecoder(blockFile->GetFileName().GetFullPath().mb_str());
      mDecoders.push_back(ret);
   }
   return ret;
}
int ODDecodeTask::GetNumFileDecoders()
{
   return mDecoders.size();
}



///This should handle unicode converted to UTF-8 on mac/linux, but OD TODO:check on windows
ODFileDecoder::ODFileDecoder(const char* fName)
{
   if(fName)
   {
      mFName = new char[strlen(fName)];
      strcpy(mFName,fName);
   }
   else
      mFName=NULL;
}

ODFileDecoder::~ODFileDecoder()
{
  if(mFName)
      delete [] mFName;
}

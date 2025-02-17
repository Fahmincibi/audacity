/**********************************************************************

  Audacity: A Digital Audio Editor

  SimpleBlockFile.cpp

  Joshua Haberman
  Markus Meyer

*******************************************************************//**

\file SimpleBlockFile.cpp
\brief Implements SimpleBlockFile and auHeader.

*//****************************************************************//**

\class SimpleBlockFile
\brief A BlockFile that reads and writes uncompressed data using 
libsndfile

A block file that writes the audio data to an .au file and reads
it back using libsndfile.

There are two ways to construct a simple block file.  One is to
supply data and have the constructor write the file.  The other
is for when the file already exists and we simply want to create
the data structure to refer to it.

The block file can be cached in two ways. Caching is enabled if the
preference "/Directories/CacheBlockFiles" is set, otherwise disabled. The
default is to disable caching.

* Read-caching: If caching is enabled, all block files will always be
  read-cached. Block files on disk will be read as soon as they are created
  and hold in memory. New block files will be written to disk, but hold in
  memory, so they are never read from disk in the current session.

* Write-caching: If caching is enabled and the parameter allowDeferredWrite
  is enabled at the block file constructor, new block files are hold in memory
  and only written to disk when WriteCacheToDisk() is called. This is used
  during recording to prevent disk access. After recording, WriteCacheToDisk()
  will be called on all block files and they will be written to disk. During
  normal editing, no write cache is active, that is, any block files will be
  written to disk instantly.
  
  Even with write cache, auto recovery during normal editing will work as
  expected. However, auto recovery during recording will not work (not even
  manual auto recovery, because the files are never written physically to
  disk).

*//****************************************************************//**

\class auHeader
\brief The auHeader is a structure used by SimpleBlockFile for .au file 
format.  There probably is an 'official' header file we should include 
to get its definition, rather than rolling our own.

*//*******************************************************************/

#ifdef _WIN32
   #include <windows.h>
#endif

#include <wx/filefn.h>
#include <wx/ffile.h>
#include <wx/utils.h>
#include <wx/log.h>

#include "../Prefs.h"

#include "SimpleBlockFile.h"
#include "../FileFormats.h"

#include "sndfile.h"
#include "../Internat.h"

//#define DEBUG_OUTPUT(s) printf("[SimpleBlockFile] %s\n", s)
#define DEBUG_OUTPUT(s)



wxUint32 SwapUintEndianess(wxUint32 in)
{
  wxUint32 out;
  unsigned char *p_in = (unsigned char *) &in;
  unsigned char *p_out = (unsigned char *) &out;
  p_out[0] = p_in[3];
  p_out[1] = p_in[2];
  p_out[2] = p_in[1];
  p_out[3] = p_in[0];
  return out;
}

/// Constructs a SimpleBlockFile based on sample data and writes
/// it to disk.
///
/// @param baseFileName The filename to use, but without an extension.
///                     This constructor will add the appropriate
///                     extension (.au in this case).
/// @param sampleData   The sample data to be written to this block.
/// @param sampleLen    The number of samples to be written to this block.
/// @param format       The format of the given samples.
/// @param allowDeferredWrite    Allow deferred write-caching
SimpleBlockFile::SimpleBlockFile(wxFileName baseFileName,
                                 samplePtr sampleData, sampleCount sampleLen,
                                 sampleFormat format,
                                 bool allowDeferredWrite /* = false */,
                                 bool bypassCache /* = false */):
   BlockFile(wxFileName(baseFileName.GetFullPath() + wxT(".au")), sampleLen)
{
   mCache.active = false;
   
   DEBUG_OUTPUT("SimpleBlockFile created based on sample data");

   bool useCache = GetCache() && (!bypassCache);

   if (!(allowDeferredWrite && useCache) && !bypassCache)
      WriteSimpleBlockFile(sampleData, sampleLen, format, NULL);
      
   if (useCache) {
      DEBUG_OUTPUT("Caching block file data");
      mCache.active = true;
      mCache.needWrite = true;
      mCache.format = format;
      mCache.sampleData = new char[sampleLen * SAMPLE_SIZE(format)];
      memcpy(mCache.sampleData,
             sampleData, sampleLen * SAMPLE_SIZE(format));
      void* summaryData = BlockFile::CalcSummary(sampleData, sampleLen,
                                                format);
      mCache.summaryData = new char[mSummaryInfo.totalSummaryBytes];
      memcpy(mCache.summaryData, summaryData,
             (size_t)mSummaryInfo.totalSummaryBytes);
    }
}

/// Construct a SimpleBlockFile memory structure that will point to an
/// existing block file.  This file must exist and be a valid block file.
///
/// @param existingFile The disk file this SimpleBlockFile should use.
SimpleBlockFile::SimpleBlockFile(wxFileName existingFile, sampleCount len,
                                 float min, float max, float rms):
   BlockFile(existingFile, len)
{
   DEBUG_OUTPUT("SimpleBlockFile based on existing file created");
   
   mMin = min;
   mMax = max;
   mRMS = rms;

   mCache.active = false;
}

SimpleBlockFile::~SimpleBlockFile()
{
   if (mCache.active)
   {
      delete[] mCache.sampleData;
      delete[] (char *)mCache.summaryData;
   }
}

bool SimpleBlockFile::WriteSimpleBlockFile(
    samplePtr sampleData,
    sampleCount sampleLen,
    sampleFormat format,
    void* summaryData)
{
   // Now checked in the DirManager
   //wxASSERT( !wxFileExists(FILENAME(mFileName.GetFullPath())) );
   
   DEBUG_OUTPUT("Writing simple block file");

   // Open and write the file
   wxFFile file(mFileName.GetFullPath(), wxT("wb"));

   if( !file.IsOpened() ){
      // Can't do anything else.
      return false;
   }

   auHeader header;

   // AU files can be either big or little endian.  Which it is is
   // determined implicitly by the byte-order of the magic 0x2e736e64
   // (.snd).  We want it to be native-endian, so we write the magic
   // to memory and then let it write that to a file in native
   // endianness
   header.magic = 0x2e736e64;

   // We store the summary data at the end of the header, so the data
   // offset is the length of the summary data plus the length of the header
   header.dataOffset = sizeof(auHeader) + mSummaryInfo.totalSummaryBytes;

   // dataSize is optional, and we opt out
   header.dataSize = 0xffffffff;

   switch(format) {
      case int16Sample:
         header.encoding = AU_SAMPLE_FORMAT_16;
         break;

      case int24Sample:
         header.encoding = AU_SAMPLE_FORMAT_24;
         break;

      case floatSample:
         header.encoding = AU_SAMPLE_FORMAT_FLOAT;
         break;
   }

   // TODO: don't fabricate
   header.sampleRate = 44100;

   // BlockFiles are always mono
   header.channels = 1;

   // Write the file
   if (!summaryData)
      summaryData = /*BlockFile::*/CalcSummary(sampleData, sampleLen, format); //mchinen:allowing virtual override of calc summary for ODDecodeBlockFile.
   
   file.Write(&header, sizeof(header));
   file.Write(summaryData, mSummaryInfo.totalSummaryBytes);

   if( format == int24Sample )
   {
      // we can't write the buffer directly to disk, because 24-bit samples
      // on disk need to be packed, not padded to 32 bits like they are in
      // memory
      int *int24sampleData = (int*)sampleData;

      for( int i = 0; i < sampleLen; i++ )
#if wxBYTE_ORDER == wxBIG_ENDIAN
         file.Write((char*)&int24sampleData[i] + 1, 3);
#else
         file.Write((char*)&int24sampleData[i], 3);
#endif
   }
   else
   {
      // for all other sample formats we can write straight from the buffer
      // to disk
      file.Write(sampleData, sampleLen * SAMPLE_SIZE(format));
    }
    
    DEBUG_OUTPUT("Wrote simple block file");
    
    return true;
}

void SimpleBlockFile::FillCache()
{
   if (mCache.active)
      return; // cache is already filled

   DEBUG_OUTPUT("Reading simple block file into cache");
   
   // Check sample format
   wxFFile file(mFileName.GetFullPath(), wxT("rb"));
   if (!file.IsOpened())
   {
      // Don't read into cache if file not available
      return;
   }

   auHeader header;

   if (file.Read(&header, sizeof(header)) != sizeof(header))
   {
      // Corrupt file
      return;
   }
   
   wxUint32 encoding;
   
   if (header.magic == 0x2e736e64)
      encoding = header.encoding; // correct endianness
   else
      encoding = SwapUintEndianess(header.encoding);

   switch (encoding)
   {
   case AU_SAMPLE_FORMAT_16:
      mCache.format = int16Sample;
      break;
   case AU_SAMPLE_FORMAT_24:
      mCache.format = int24Sample;
      break;
   default:
      // floatSample is a safe default (we will never loose data)
      mCache.format = floatSample;
      break;
   }
   
   file.Close();
   
   // Read samples into cache
   mCache.sampleData = new char[mLen * SAMPLE_SIZE(mCache.format)];
   if (ReadData(mCache.sampleData, mCache.format, 0, mLen) != mLen)
   {
      // Could not read all samples
      delete mCache.sampleData;
      return;
   }

   // Read summary data into cache
   mCache.summaryData = new char[mSummaryInfo.totalSummaryBytes];
   if (!ReadSummary(mCache.summaryData))
      memset(mCache.summaryData, 0, mSummaryInfo.totalSummaryBytes);

   // Cache is active but already on disk
   mCache.active = true;
   mCache.needWrite = false;
   
   DEBUG_OUTPUT("Succesfully read simple blockfile into cache");
}

/// Read the summary section of the disk file.
///
/// @param *data The buffer to write the data to.  It must be at least
/// mSummaryinfo.totalSummaryBytes long.
bool SimpleBlockFile::ReadSummary(void *data)
{
   if (mCache.active)
   {
      DEBUG_OUTPUT("ReadSummary: Summary is already in cache");
      memcpy(data, mCache.summaryData, (size_t)mSummaryInfo.totalSummaryBytes);
      return true;
   } else
   {
      DEBUG_OUTPUT("ReadSummary: Reading summary from disk");
      
      wxFFile file(mFileName.GetFullPath(), wxT("rb"));

      wxLogNull *silence=0;
      if(mSilentLog)silence= new wxLogNull();
   
      if(!file.IsOpened() ){
      
         memset(data,0,(size_t)mSummaryInfo.totalSummaryBytes);

         if(silence) delete silence;
         mSilentLog=TRUE;

         return true;
      
      }

      if(silence) delete silence;
      mSilentLog=FALSE;
   
      // The offset is just past the au header
      if( !file.Seek(sizeof(auHeader)) )
         return false;
   
      int read = (int)file.Read(data, (size_t)mSummaryInfo.totalSummaryBytes);

      FixSummary(data);

      return (read == mSummaryInfo.totalSummaryBytes);
   }
}

/// Read the data portion of the block file using libsndfile.  Convert it
/// to the given format if it is not already.
///
/// @param data   The buffer where the data will be stored
/// @param format The format the data will be stored in
/// @param start  The offset in this block file
/// @param len    The number of samples to read
int SimpleBlockFile::ReadData(samplePtr data, sampleFormat format,
                        sampleCount start, sampleCount len)
{
   if (mCache.active)
   {
      DEBUG_OUTPUT("ReadData: Data is already in cache");
      
      if (len > mLen - start)
         len = mLen - start;
      CopySamples(
         (samplePtr)(((char*)mCache.sampleData) +
            start * SAMPLE_SIZE(mCache.format)),
         mCache.format, data, format, len);
      return len;
   } else
   {
      DEBUG_OUTPUT("ReadData: Reading data from disk");
      
      SF_INFO info;
      wxLogNull *silence=0;
      if(mSilentLog)silence= new wxLogNull();
   
      memset(&info, 0, sizeof(info));

      wxFile f;   // will be closed when it goes out of scope
      SNDFILE *sf = NULL;

      if (f.Open(mFileName.GetFullPath())) {
         // Even though there is an sf_open() that takes a filename, use the one that
         // takes a file descriptor since wxWidgets can open a file with a Unicode name and
         // libsndfile can't (under Windows).
         sf = sf_open_fd(f.fd(), SFM_READ, &info, FALSE);
      }

      if (!sf) {
      
         memset(data,0,SAMPLE_SIZE(format)*len);

         if(silence) delete silence;
         mSilentLog=TRUE;

         return len;
      }
      if(silence) delete silence;
      mSilentLog=FALSE;
   
      sf_seek(sf, start, SEEK_SET);
      samplePtr buffer = NewSamples(len, floatSample);

      int framesRead = 0;

      // If both the src and dest formats are integer formats,
      // read integers from the file (otherwise we would be
      // converting to float and back, which is unneccesary)
      if (format == int16Sample &&
          sf_subtype_is_integer(info.format)) {
         framesRead = sf_readf_short(sf, (short *)data, len);
      }
      else
      if (format == int24Sample &&
          sf_subtype_is_integer(info.format))
      {
         framesRead = sf_readf_int(sf, (int *)data, len);

         // libsndfile gave us the 3 byte sample in the 3 most
         // significant bytes -- we want it in the 3 least
         // significant bytes.
         int *intPtr = (int *)data;
         for( int i = 0; i < framesRead; i++ )
            intPtr[i] = intPtr[i] >> 8;
      }
      else {
         // Otherwise, let libsndfile handle the conversion and
         // scaling, and pass us normalized data as floats.  We can
         // then convert to whatever format we want.
         framesRead = sf_readf_float(sf, (float *)buffer, len);
         CopySamples(buffer, floatSample,
                     (samplePtr)data, format, framesRead);
      }

      DeleteSamples(buffer);
   
      sf_close(sf);

      return framesRead;
   }
}

void SimpleBlockFile::SaveXML(XMLWriter &xmlFile)
{
   xmlFile.StartTag(wxT("simpleblockfile"));

   xmlFile.WriteAttr(wxT("filename"), mFileName.GetFullName());
   xmlFile.WriteAttr(wxT("len"), mLen);
   xmlFile.WriteAttr(wxT("min"), mMin);
   xmlFile.WriteAttr(wxT("max"), mMax);
   xmlFile.WriteAttr(wxT("rms"), mRMS);

   xmlFile.EndTag(wxT("simpleblockfile"));
}

/// static
BlockFile *SimpleBlockFile::BuildFromXML(DirManager &dm, const wxChar **attrs)
{
   wxFileName fileName;
   float min=0, max=0, rms=0;
   sampleCount len = 0;
   double dblValue;
   long nValue;

   while(*attrs)
   {
      const wxChar *attr =  *attrs++;
      const wxChar *value = *attrs++;

      if (!value)
         break;

      const wxString strValue = value;
      if( !wxStrcmp(attr, wxT("filename")) )
      {
         // Can't use XMLValueChecker::IsGoodFileName here, but do part of its test.
         if (!XMLValueChecker::IsGoodFileString(strValue))
            return NULL;

         #ifdef _WIN32
            if (strValue.Length() + 1 + dm.GetProjectDataDir().Length() > MAX_PATH)
               return NULL;
         #endif

         dm.AssignFile(fileName,value,FALSE);
      }
      else if( !wxStrcmp(attr, wxT("len")) && XMLValueChecker::IsGoodInt(strValue) && strValue.ToLong(&nValue)) 
         len = nValue;
      else if( !wxStrcmp(attr, wxT("min")) && 
               XMLValueChecker::IsGoodString(strValue) && Internat::CompatibleToDouble(strValue, &dblValue))
         min = dblValue;
      else if( !wxStrcmp(attr, wxT("max")) && 
               XMLValueChecker::IsGoodString(strValue) && Internat::CompatibleToDouble(strValue, &dblValue))
         max = dblValue;
      else if( !wxStrcmp(attr, wxT("rms")) && 
               XMLValueChecker::IsGoodString(strValue) && Internat::CompatibleToDouble(strValue, &dblValue))
         rms = dblValue;
   }

   if (!XMLValueChecker::IsGoodFileString(fileName.GetFullName()) || 
         (len <= 0) || (rms < 0.0))
      return NULL;

   return new SimpleBlockFile(fileName, len, min, max, rms);
}

/// Create a copy of this BlockFile, but using a different disk file.
///
/// @param newFileName The name of the new file to use.
BlockFile *SimpleBlockFile::Copy(wxFileName newFileName)
{
   BlockFile *newBlockFile = new SimpleBlockFile(newFileName, mLen,
                                                 mMin, mMax, mRMS);

   return newBlockFile;
}

wxLongLong SimpleBlockFile::GetSpaceUsage()
{
   if (mCache.active && mCache.needWrite)
   {
      // We don't know space usage yet
      return 0;
   } else
   {
      wxFFile dataFile(mFileName.GetFullPath());
      return dataFile.Length();
   }
}

void SimpleBlockFile::Recover(){
   wxFFile file(mFileName.GetFullPath(), wxT("wb"));
   int i;

   if( !file.IsOpened() ){
      // Can't do anything else.
      return;
   }

   auHeader header;
   header.magic = 0x2e736e64;
   header.dataOffset = sizeof(auHeader) + mSummaryInfo.totalSummaryBytes;

   // dataSize is optional, and we opt out
   header.dataSize = 0xffffffff;
   header.encoding = AU_SAMPLE_FORMAT_16;
   header.sampleRate = 44100;
   header.channels = 1;
   file.Write(&header, sizeof(header));

   for(i=0;i<mSummaryInfo.totalSummaryBytes;i++)
      file.Write(wxT("\0"),1);

   for(i=0;i<mLen*2;i++)
      file.Write(wxT("\0"),1);

}

void SimpleBlockFile::WriteCacheToDisk()
{
   if (!GetNeedWriteCacheToDisk())
      return;
   
   DEBUG_OUTPUT("WriteCacheToDisk");

   if (WriteSimpleBlockFile(mCache.sampleData, mLen, mCache.format,
                            mCache.summaryData))
      mCache.needWrite = false;
}

bool SimpleBlockFile::GetNeedWriteCacheToDisk()
{
   return mCache.active && mCache.needWrite;
}

bool SimpleBlockFile::GetCache()
{  
   bool cacheBlockFiles = false;
   gPrefs->Read(wxT("/Directories/CacheBlockFiles"), &cacheBlockFiles);

   int lowMem = gPrefs->Read(wxT("/Directories/CacheLowMem"), 16l);
   if (lowMem < 16) {
      lowMem = 16;
   }
   lowMem <<= 20;

   return cacheBlockFiles && (GetFreeMemory() > lowMem);
}

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 606f2b69-1fda-40a3-88e6-06ff91d708c2



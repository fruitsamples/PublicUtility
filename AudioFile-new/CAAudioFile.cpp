/*	Copyright: 	© Copyright 2004 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	CAAudioFile.cpp
	
=============================================================================*/

#include "CAAudioFile.h"
#include "CAXException.h"
#include <libgen.h>
#include <algorithm>
#include "CAHostTimeBase.h"
#include "CADebugMacros.h"

#if DEBUG
	//#define VERBOSE_IO 1
	//#define VERBOSE_CONVERTER 1
	//#define VERBOSE_CHANNELMAP 1
#endif

#if VERBOSE_CHANNELMAP
	//#include "CAChannelLayouts.h"
#endif

static const UInt32 kDefaultIOBufferSize = 0x8000;

#if CAAUDIOFILE_PROFILE
	#define StartTiming(af, starttime) UInt64 starttime = af->mProfiling ? CAHostTimeBase::GetCurrentTime() : NULL
	#define ElapsedTime(af, starttime, counter) if (af->mProfiling) counter += (CAHostTimeBase::GetCurrentTime() - starttime)
#else
	#define StartTiming(af, starttime)
	#define ElapsedTime(af, starttime, counter)
#endif

#define kNoMoreInputRightNow 'nein'

// _______________________________________________________________________________________
//
CAAudioFile::CAAudioFile() :
	mAudioFile(0),
	mUseCache(false),
	mFinishingEncoding(false),
	mMode(kClosed),
	mFramesToSkipFollowingSeek(0),
	
	mClientOwnsIOBuffer(false),
	mPacketDescs(NULL),
	mConverter(NULL),
	mMagicCookie(NULL),
	mWriteBufferList(NULL)
#if CAAUDIOFILE_PROFILE
    ,
	mProfiling(false),
	mTicksInConverter(0),
	mTicksInIO(0)
#endif
{
	mIOBufferList.mBuffers[0].mData = NULL;
	mClientMaxPacketSize = 0;
}

// _______________________________________________________________________________________
//
CAAudioFile::~CAAudioFile()
{
	Close();
}

// _______________________________________________________________________________________
//
void	CAAudioFile::Close()
{
	if (mMode == kClosed)
		return;
	if (mMode == kWriting)
		FlushEncoder();
	CloseConverter();
	if (mAudioFile != 0 && mOwnOpenFile) {
		AudioFileClose(mAudioFile);
		mAudioFile = 0;
	}
	if (!mClientOwnsIOBuffer) {
		delete[] (Byte *)mIOBufferList.mBuffers[0].mData;
		mIOBufferList.mBuffers[0].mData = NULL;
	}
	delete[] mPacketDescs;	mPacketDescs = NULL;
	delete[] mMagicCookie;	mMagicCookie = NULL;
	delete mWriteBufferList;	mWriteBufferList = NULL;
	mMode = kClosed;
}

// _______________________________________________________________________________________
//
void	CAAudioFile::Delete()
{
	bool willDelete = (mMode != kPreparingToCreate);
	mMode = kDeleting;
	Close();
	if (willDelete)
		XThrowIfError(FSDeleteObject(&mFSRef), "delete audio file");
}

// _______________________________________________________________________________________
//
void	CAAudioFile::CloseConverter()
{
	if (mConverter) {
#if VERBOSE_CONVERTER
		printf("CAAudioFile %p : CloseConverter\n", this);
#endif
		AudioConverterDispose(mConverter);
		mConverter = NULL;
	}
}

// =======================================================================================

// _______________________________________________________________________________________
//
void	CAAudioFile::Open(const char *filePath)
{
	FSRef fsref;
	XThrowIfError(FSPathMakeRef((UInt8 *)filePath, &fsref, NULL), "locate audio file");
	Open(fsref);
}

// _______________________________________________________________________________________
//
void	CAAudioFile::Open(const FSRef &fsref)
{
	XThrowIf(mMode != kClosed, kExtAudioFileError_InvalidOperationOrder, "file already open");
	mFSRef = fsref;
	XThrowIfError(AudioFileOpen(&mFSRef, fsRdPerm, 0, &mAudioFile), "open audio file");
	mOwnOpenFile = true;
	mMode = kReading;
	GetExistingFileInfo();
}

// _______________________________________________________________________________________
//
void	CAAudioFile::Wrap(AudioFileID fileID, bool forWriting)
{
	XThrowIf(mMode != kClosed, kExtAudioFileError_InvalidOperationOrder, "file already open");
	UInt64 packetCount;
	UInt32 size = sizeof(packetCount);
	XThrowIfError(AudioFileGetProperty(fileID, kAudioFilePropertyAudioDataPacketCount, &size, &packetCount), "could not get file's packet count");

	mAudioFile = fileID;
	mOwnOpenFile = false;
	mMode = forWriting ? kPreparingToWrite : kReading;
	GetExistingFileInfo();
	if (forWriting)
		FileFormatChanged();
}

// _______________________________________________________________________________________
//
void	CAAudioFile::PrepareNew(const CAStreamBasicDescription &dataFormat, 
								const CAAudioChannelLayout *layout)
{
	XThrowIf(mMode != kClosed, kExtAudioFileError_InvalidOperationOrder, "file already open");
	
	mFileDataFormat = dataFormat;
	if (layout) {
		mFileChannelLayout = *layout;
#if VERBOSE_CHANNELMAP
		printf("PrepareNew passed channel layout: %s\n", CAChannelLayouts::ConstantToString(mFileChannelLayout.Tag()));
#endif
	}
	mMode = kPreparingToCreate;
}

// _______________________________________________________________________________________
//
static OSStatus	FullPathToParentFSRefAndName(const char *path, FSRef &outParentDir, CFStringRef &outFileName)
{
	outFileName = NULL;
	OSStatus err = FSPathMakeRef((UInt8 *)dirname(path), &outParentDir, NULL);
	if (err) return err;
	outFileName = CFStringCreateWithCString(NULL, basename(path), kCFStringEncodingUTF8);
	return noErr;
}

// _______________________________________________________________________________________
//
void	CAAudioFile::Create(const char *filePath, AudioFileTypeID fileType)
{
	// get FSRef/CFStringRef for output file
	FSRef outFolderFSRef;
	CFStringRef outFileName;
	XThrowIfError(FullPathToParentFSRefAndName(filePath, outFolderFSRef, outFileName), "locate audio file");
	
	Create(outFolderFSRef, outFileName, fileType);
	CFRelease(outFileName);
}

// _______________________________________________________________________________________
//
void	CAAudioFile::Create(const FSRef &parentDir, CFStringRef filename, AudioFileTypeID filetype)
{
	FileFormatChanged(&parentDir, filename, filetype);
}

// _______________________________________________________________________________________
//
// called to create the file -- or update its format/channel layout/properties based on an encoder 
// setting change
void	CAAudioFile::FileFormatChanged(const FSRef *parentDir, CFStringRef filename, AudioFileTypeID filetype)
{
	XThrowIf(mMode != kPreparingToCreate && mMode != kPreparingToWrite, kExtAudioFileError_InvalidOperationOrder, "new file not prepared");
	
	UInt32 propertySize;
	OSStatus err;
	AudioStreamBasicDescription saveFileDataFormat = mFileDataFormat;
	
#if VERBOSE_CONVERTER
	mFileDataFormat.PrintFormat(stdout, "", "Specified file data format");
#endif
	
	// Find out the actual format the converter will produce. This is necessary in
	// case the bitrate has forced a lower sample rate, which needs to be set correctly
	// in the stream description passed to AudioFileCreate.
	if (mConverter != NULL) {
		UInt32 propertySize = sizeof(AudioStreamBasicDescription);
		Float64 origSampleRate = mFileDataFormat.mSampleRate;
		XThrowIfError(AudioConverterGetProperty(mConverter, 
			kAudioConverterCurrentOutputStreamDescription, &propertySize, &mFileDataFormat),
			"get audio converter's output stream description");
		// do the same for the channel layout being output by the converter
#if VERBOSE_CONVERTER
		mFileDataFormat.PrintFormat(stdout, "", "Converter output");
#endif
		if (fiszero(mFileDataFormat.mSampleRate))
			mFileDataFormat.mSampleRate = origSampleRate;
		err = AudioConverterGetPropertyInfo(mConverter, kAudioConverterOutputChannelLayout, 
				&propertySize, NULL);
		if (err == noErr && propertySize > 0) {
			AudioChannelLayout *layout = static_cast<AudioChannelLayout *>(malloc(propertySize));
			err = AudioConverterGetProperty(mConverter, kAudioConverterOutputChannelLayout, &propertySize, layout);
			if (err) {
				free(layout);
				XThrow(err, "couldn't get audio converter's output channel layout");
			}
			mFileChannelLayout = layout;
#if VERBOSE_CHANNELMAP
			printf("got new file's channel layout: %s\n", CAChannelLayouts::ConstantToString(mFileChannelLayout.Tag()));
#endif
			free(layout);
		}
	}
	
	if (fiszero(mFileDataFormat.mSampleRate))
		mFileDataFormat.mSampleRate = mClientDataFormat.mSampleRate;
#if VERBOSE_CONVERTER
	mFileDataFormat.PrintFormat(stdout, "", "Applied to new file");
#endif
	XThrowIf(fiszero(mFileDataFormat.mSampleRate), kExtAudioFileError_InvalidDataFormat, "file's sample rate is 0");

	// create the output file
	if (mMode == kPreparingToCreate) {
		XThrowIfError(AudioFileCreate(parentDir, filename, filetype, &mFileDataFormat, 0, 
				&mFSRef, &mAudioFile), "create audio file");
		mMode = kPreparingToWrite;
		mOwnOpenFile = true;
	} else if (saveFileDataFormat != mFileDataFormat) {
		XThrowIfError(AudioFileSetProperty(mAudioFile, kAudioFilePropertyDataFormat, sizeof(AudioStreamBasicDescription), &mFileDataFormat), "couldn't update file's data format");
	}

	UInt32 deferSizeUpdates = 1;
	XThrowIfError(AudioFileSetProperty(mAudioFile, kAudioFilePropertyDeferSizeUpdates, sizeof(UInt32), &deferSizeUpdates),
		"couldn't defer size updates");	// make this optional?		

	if (mConverter != NULL) {
		// encoder
		// get the magic cookie, if any, from the converter		
		err = AudioConverterGetPropertyInfo(mConverter, 
					kAudioConverterCompressionMagicCookie, &propertySize, NULL);
		
		// we can get a noErr result and also a propertySize == 0
		// -- if the file format does support magic cookies, but this file doesn't have one.
		if (err == noErr && propertySize > 0) {
			mMagicCookie = new Byte[propertySize];
			mMagicCookieSize = propertySize;
			XThrowIfError(AudioConverterGetProperty(mConverter, kAudioConverterCompressionMagicCookie,
				&propertySize, mMagicCookie), "get audio converter's magic cookie");
			// now set the magic cookie on the output file
			UInt32 willEatTheCookie = false;
			// the converter wants to give us one; will the file take it?
			err = AudioFileGetPropertyInfo(mAudioFile, kAudioFilePropertyMagicCookieData,
					NULL, &willEatTheCookie);
			if (err == noErr && willEatTheCookie)
				XThrowIfError(AudioFileSetProperty(mAudioFile, kAudioFilePropertyMagicCookieData,
					mMagicCookieSize, mMagicCookie), "set audio file's magic cookie");
		}
		
		// get maximum packet size
		propertySize = sizeof(UInt32);
		XThrowIfError(AudioConverterGetProperty(mConverter, 
			kAudioConverterPropertyMaximumOutputPacketSize, 
			&propertySize, &mFileMaxPacketSize), "get audio converter's maximum output packet size");
	} else {
		InitFileMaxPacketSize();
	}
	XThrowIf(mFileMaxPacketSize == 0, kExtAudioFileError_MaxPacketSizeUnknown, "file's maximum packet size is 0");
	
	if (mFileChannelLayout.IsValid() && mFileChannelLayout.NumberChannels() > 2) {
		// don't bother tagging mono/stereo files
		UInt32 isWritable;
		err = AudioFileGetPropertyInfo(mAudioFile, kAudioFilePropertyChannelLayout, NULL, &isWritable);
		if (!err && isWritable) {
#if VERBOSE_CHANNELMAP
			printf("writing file's channel layout: %s\n", CAChannelLayouts::ConstantToString(mFileChannelLayout.Tag()));
#endif
			err = AudioFileSetProperty(mAudioFile, kAudioFilePropertyChannelLayout, 
				mFileChannelLayout.Size(), &mFileChannelLayout.Layout());
			if (err)
				CAXException::Warning("could not set the file's channel layout", err);
		} else {
#if VERBOSE_CHANNELMAP
			printf("file won't accept a channel layout\n");
#endif
			// forget that we have a channel layout, we can't store it in the file!
			mFileChannelLayout = CAAudioChannelLayout();
		}
	}
	
	UpdateClientMaxPacketSize();	// also sets mFrame0Offset
	mPacketMark = 0;
	mFrameMark = 0;
	mNumberPackets = 0;
	SetIOBufferSize(kDefaultIOBufferSize);
}

// _______________________________________________________________________________________
//
void	CAAudioFile::InitFileMaxPacketSize()
{
	UInt32 propertySize = sizeof(UInt32);
	OSStatus err = AudioFileGetProperty(mAudioFile, kAudioFilePropertyMaximumPacketSize, 
		&propertySize, &mFileMaxPacketSize);
	if (err) {
		// workaround for 3361377: not all file formats' maximum packet sizes are supported
		if (!mFileDataFormat.IsPCM())
			XThrowIfError(err, "get audio file's maximum packet size");
		mFileMaxPacketSize = mFileDataFormat.mBytesPerFrame;
	}
	XThrowIf(mFileMaxPacketSize == 0, kExtAudioFileError_MaxPacketSizeUnknown, "file's maximum packet size is 0");
}

// _______________________________________________________________________________________
//
// call for existing file, NOT new one - from Open() or Wrap()
void	CAAudioFile::GetExistingFileInfo()
{
	UInt32 propertySize;
	OSStatus err;
	
	// get mFileDataFormat
	propertySize = sizeof(AudioStreamBasicDescription);
	XThrowIfError(AudioFileGetProperty(mAudioFile, kAudioFilePropertyDataFormat, 
		&propertySize, &mFileDataFormat), "get audio file's data format");
	
	// get mFileChannelLayout
	err = AudioFileGetPropertyInfo(mAudioFile, kAudioFilePropertyChannelLayout,
			&propertySize, NULL);
	if (err == noErr && propertySize > 0) {
		AudioChannelLayout *layout = static_cast<AudioChannelLayout *>(malloc(propertySize));
		err = AudioFileGetProperty(mAudioFile, kAudioFilePropertyChannelLayout,
				&propertySize, layout);
		if (err == noErr) {
			mFileChannelLayout = layout;
#if VERBOSE_CHANNELMAP
			printf("existing file's channel layout: %s\n", CAChannelLayouts::ConstantToString(mFileChannelLayout.Tag()));
#endif
		}
		free(layout);
		XThrowIfError(err, "get audio file's channel layout");
	}
	if (mMode != kReading)
		return;
	
	// get mNumberPackets
	propertySize = sizeof(mNumberPackets);
	XThrowIfError(AudioFileGetProperty(mAudioFile, kAudioFilePropertyAudioDataPacketCount, 
		&propertySize, &mNumberPackets), "get audio file's packet count");
#if VERBOSE_IO
	printf("CAAudioFile::GetExistingFileInfo: %qd packets\n", mNumberPackets);
#endif
	
	// get mMagicCookie
	err = AudioFileGetPropertyInfo(mAudioFile, kAudioFilePropertyMagicCookieData,
			&propertySize, NULL);
	if (err == noErr && propertySize > 0) {
		mMagicCookie = new Byte[propertySize];
		mMagicCookieSize = propertySize;
		XThrowIfError(AudioFileGetProperty(mAudioFile, kAudioFilePropertyMagicCookieData,
			&propertySize, mMagicCookie), "get audio file's magic cookie");
	}
	InitFileMaxPacketSize();
	mPacketMark = 0;
	mFrameMark = 0;
	
	UpdateClientMaxPacketSize();
	SetIOBufferSize(kDefaultIOBufferSize);
}

// =======================================================================================

// _______________________________________________________________________________________
//
void	CAAudioFile::SetFileChannelLayout(const CAAudioChannelLayout &layout)
{
	mFileChannelLayout = layout;
#if VERBOSE_CHANNELMAP
	printf("file channel layout set explicitly: %s\n", CAChannelLayouts::ConstantToString(mFileChannelLayout.Tag()));
#endif
	FileFormatChanged();
}

// _______________________________________________________________________________________
//
void	CAAudioFile::SetClientDataFormat(const CAStreamBasicDescription &dataFormat)
{
	SetClientFormat(dataFormat, NULL);
}

// _______________________________________________________________________________________
//
void	CAAudioFile::SetClientChannelLayout(const CAAudioChannelLayout &layout)
{
	SetClientFormat(mClientDataFormat, &layout);
}

// _______________________________________________________________________________________
//
void	CAAudioFile::SetClientFormat(const CAStreamBasicDescription &dataFormat, const CAAudioChannelLayout *layout)
{
	XThrowIf(!dataFormat.IsPCM(), kExtAudioFileError_NonPCMClientFormat, "non-PCM client format on audio file");
	
	bool dataFormatChanging = (mClientDataFormat.mFormatID == 0 || mClientDataFormat != dataFormat);
	
	if (dataFormatChanging) {
		CloseConverter();
		if (mWriteBufferList) {
			delete mWriteBufferList;
			mWriteBufferList = NULL;
		}
		mClientDataFormat = dataFormat;
	}
	
	if (layout && layout->IsValid()) {
		XThrowIf(layout->NumberChannels() != mClientDataFormat.NumberChannels(), kExtAudioFileError_InvalidChannelMap, "inappropriate channel map");
		mClientChannelLayout = *layout;
	}
	
	bool differentLayouts;
	if (mClientChannelLayout.IsValid()) {
		if (mFileChannelLayout.IsValid()) {
			differentLayouts = mClientChannelLayout.Tag() != mFileChannelLayout.Tag();
#if VERBOSE_CHANNELMAP
			printf("two valid layouts, %s\n", differentLayouts ? "different" : "same");
#endif
		} else {
			differentLayouts = false;
#if VERBOSE_CHANNELMAP
			printf("valid client layout, unknown file layout\n");
#endif
		}
	} else {
		differentLayouts = false;
#if VERBOSE_CHANNELMAP
		if (mFileChannelLayout.IsValid())
			printf("valid file layout, unknown client layout\n");
		else
			printf("two invalid layouts\n");
#endif
	}
	
	if (mClientDataFormat != mFileDataFormat || differentLayouts) {
		// We need an AudioConverter.
		if (mMode == kReading) {
			// file -> client
//mFileDataFormat.PrintFormat(  stdout, "", "File:   ");
//mClientDataFormat.PrintFormat(stdout, "", "Client: ");

			if (mConverter == NULL)
				XThrowIfError(AudioConverterNew(&mFileDataFormat, &mClientDataFormat, &mConverter),
				"create audio converter");
			
#if VERBOSE_CONVERTER
			printf("CAAudioFile %p -- created converter\n", this);
			CAShow(mConverter);
#endif
			// set the magic cookie, if any (for decode)
			if (mMagicCookie)
				SetConverterProperty(kAudioConverterDecompressionMagicCookie,
					mMagicCookieSize, mMagicCookie, mFileDataFormat.IsPCM());
					// we get cookies from some AIFF's but the converter barfs on them,
					// so we set canFail to true for PCM

			SetConverterChannelLayout(false, mFileChannelLayout);
			SetConverterChannelLayout(true, mClientChannelLayout);
		} else if (mMode == kPreparingToCreate || mMode == kPreparingToWrite) {
			// client -> file
			if (mConverter == NULL)
				XThrowIfError(AudioConverterNew(&mClientDataFormat, &mFileDataFormat, &mConverter),
				"create audio converter");
			mWriteBufferList = CABufferList::New("", mClientDataFormat);
			SetConverterChannelLayout(false, mClientChannelLayout);
			SetConverterChannelLayout(true, mFileChannelLayout);
		} else
			XThrowIfError(kExtAudioFileError_InvalidOperationOrder, "audio file format not yet known");
	}
	UpdateClientMaxPacketSize();
}

// _______________________________________________________________________________________
//
OSStatus	CAAudioFile::SetConverterProperty(	
											AudioConverterPropertyID	inPropertyID,
											UInt32						inPropertyDataSize,
											const void*					inPropertyData,
											bool						inCanFail)
{
	OSStatus err = AudioConverterSetProperty(mConverter, inPropertyID, inPropertyDataSize, inPropertyData);
	if (!inCanFail) {
		XThrowIfError(err, "set audio converter property");
	}
	UpdateClientMaxPacketSize();
	if (mMode == kPreparingToWrite)
		FileFormatChanged();
	return err;
}

// _______________________________________________________________________________________
//
void	CAAudioFile::SetConverterChannelLayout(bool output, const CAAudioChannelLayout &layout)
{
	OSStatus err;
	
	if (layout.IsValid()) {
		if (output) {
			err = AudioConverterSetProperty(mConverter, kAudioConverterOutputChannelLayout,
				layout.Size(), &layout.Layout());
			XThrowIf(err && err != kAudioConverterErr_OperationNotSupported, err, "couldn't set converter's output channel layout");
		} else {
			err = AudioConverterSetProperty(mConverter, kAudioConverterInputChannelLayout,
				layout.Size(), &layout.Layout());
			XThrowIf(err && err != kAudioConverterErr_OperationNotSupported, err, "couldn't set converter's input channel layout");
		}
		if (mMode == kPreparingToWrite)
			FileFormatChanged();
	}
}

// _______________________________________________________________________________________
//
CFArrayRef  CAAudioFile::GetConverterConfig()
{
	CFArrayRef plist;
	UInt32 propertySize = sizeof(plist);
	XThrowIfError(AudioConverterGetProperty(mConverter, kAudioConverterPropertySettings, &propertySize, &plist), "get converter property settings");
	return plist;
}

// _______________________________________________________________________________________
//
void	CAAudioFile::UpdateClientMaxPacketSize()
{
	mFrame0Offset = 0;
	if (mConverter != NULL) {
		AudioConverterPropertyID property = (mMode == kReading) ? 
			kAudioConverterPropertyMaximumOutputPacketSize :
			kAudioConverterPropertyMaximumInputPacketSize;
			
		UInt32 propertySize = sizeof(UInt32);
		XThrowIfError(AudioConverterGetProperty(mConverter, property, &propertySize, &mClientMaxPacketSize),
			"get audio converter's maximum packet size");
		
		AudioConverterPrimeInfo primeInfo;
		propertySize = sizeof(primeInfo);
		OSStatus err = AudioConverterGetProperty(mConverter, kAudioConverterPrimeInfo, &propertySize, &primeInfo);
		if (err == noErr)
			mFrame0Offset = primeInfo.leadingFrames;
#if VERBOSE_CONVERTER
		printf("kAudioConverterPrimeInfo: err = %ld, leadingFrames = %ld\n", err, mFrame0Offset);
#endif
	} else {
		mClientMaxPacketSize = mFileMaxPacketSize;
	}
	XThrowIf(mClientMaxPacketSize == 0, kExtAudioFileError_MaxPacketSizeUnknown, "client maximum packet size is 0");
}

// _______________________________________________________________________________________
// Allocates the I/O buffers, so it must be called from the second-stage initializers.
void	CAAudioFile::SetIOBufferSize(UInt32 bufferSizeBytes)
{
	XThrowIf(mFileMaxPacketSize == 0, kExtAudioFileError_MaxPacketSizeUnknown, "file's maximum packet size is 0");
	bufferSizeBytes = std::max(bufferSizeBytes, mFileMaxPacketSize);	// must be big enough for one maximum size packet
	mIOBufferSizeBytes = bufferSizeBytes;

	mIOBufferList.mNumberBuffers = 1;
	mIOBufferList.mBuffers[0].mNumberChannels = mFileDataFormat.mChannelsPerFrame;
	if (!mClientOwnsIOBuffer) {
		delete[] (Byte *)mIOBufferList.mBuffers[0].mData;
		mIOBufferList.mBuffers[0].mData = new Byte[bufferSizeBytes];
	}
	mIOBufferList.mBuffers[0].mDataByteSize = bufferSizeBytes;
	mIOBufferSizePackets = bufferSizeBytes / mFileMaxPacketSize;

	UInt32 propertySize = sizeof(UInt32);
	UInt32 externallyFramed;
	XThrowIfError(AudioFormatGetProperty(kAudioFormatProperty_FormatIsExternallyFramed,
			sizeof(AudioStreamBasicDescription), &mFileDataFormat, &propertySize, &externallyFramed),
			"is format externally framed");
	if (externallyFramed)
		mPacketDescs = new AudioStreamPacketDescription[mIOBufferSizePackets];
}

// _______________________________________________________________________________________
//
void	CAAudioFile::SetIOBuffer(void *buf)
{
	if (buf == NULL) {
		mClientOwnsIOBuffer = false;
		SetIOBufferSize(mIOBufferSizeBytes);
	} else {
		mClientOwnsIOBuffer = true;
		mIOBufferList.mBuffers[0].mData = buf;
	}
}

// ===============================================================================

/*
For Tiger:
added kAudioFilePropertyPacketToFrame and kAudioFilePropertyFrameToPacket.
You pass in an AudioFramePacketTranslation struct, with the appropriate field filled in, to AudioFileGetProperty.
*/

SInt64  CAAudioFile::PacketToFrame(SInt64 packet) const
{
	switch (mFileDataFormat.mFramesPerPacket) {
	case 1:
		return packet;
	case 0:
#warning use new AudioFile property for this
		XThrowIfError(unimpErr, "packet <-> frame translation unimplemented for format with variable frames/packet");
	}
	return packet * mFileDataFormat.mFramesPerPacket;
}

SInt64	CAAudioFile::FrameToPacket(SInt64 inFrame) const
{
	switch (mFileDataFormat.mFramesPerPacket) {
	case 1:
		return inFrame;
	case 0:
		XThrowIfError(unimpErr, "packet <-> frame translation unimplemented for format with variable frames/packet");
	}
	return inFrame / mFileDataFormat.mFramesPerPacket;
}

// _______________________________________________________________________________________
//

void	CAAudioFile::SeekToPacket(SInt64 packetNumber)
{
#if VERBOSE_IO
	printf("CAAudioFile::SeekToPacket: %qd\n", packetNumber);
#endif
	XThrowIf(mMode != kReading || packetNumber < 0 || packetNumber >= mNumberPackets, kExtAudioFileError_InvalidSeek, "seek to packet in audio file");
	if (mPacketMark == packetNumber)
		return; // already there! don't reset converter
	mPacketMark = packetNumber;
	
	mFrameMark = PacketToFrame(packetNumber) - mFrame0Offset;
	mFramesToSkipFollowingSeek = 0;
	if (mConverter)
		AudioConverterReset(mConverter);
}

SInt64  CAAudioFile::TellFrame() const
{
	return mFrameMark - mFrame0Offset;
}

/*
	Example: AAC, 1024 frames/packet, 2112 frame offset
	
                                           2112
                                             |
    Absolute frames:  0       1024      2048 |    3072						mFrameMark
                      +---------+---------+--|------+---------+---------+
    Packets:          |    0    |    1    |  | 2    |    3    |    4    |
                      +---------+---------+--|------+---------+---------+
    Client frames:  -2112   -1088       -64  |     960						SeekToFrame, TellFrame
                                             |
                                             0

	*   Offset between absolute and client frames is mFrame0Offset.
	*   mFrameMark is in client frames.
	
	Example:
		frame = 1000
		packet = 0
		subFrame = 1000
		(SeekToPacket)
		mFrameMark = -2112
		mFramesToSkipFollowingSeek = 1000 - (-2112) = 3112
*/
void	CAAudioFile::SeekToFrame(SInt64 clientFrameNumber)
{
	SInt64 frame = clientFrameNumber + mFrame0Offset;
	XThrowIf(mMode != kReading || frame < 0 || !mClientDataFormat.IsPCM(), kExtAudioFileError_InvalidSeek, "seek to frame in audio file");

	if (frame == mFrameMark)
		return; // already there! don't reset converter
#if VERBOSE_IO
	SInt64 prevFrameMark = mFrameMark;
#endif
	
	SInt64 packet;
	packet = FrameToPacket(frame);
	if (packet < 0)
		packet = 0;
	SeekToPacket(packet);
	// this will have set mFrameMark to match the beginning of the packet
	mFramesToSkipFollowingSeek = std::max(UInt32(frame - mFrameMark), UInt32(0));
	mFrameMark = clientFrameNumber;
	
#if VERBOSE_IO
	printf("CAAudioFile::SeekToFrame: frame %qd (from %qd), packet %qd, skip %ld frames\n", mFrameMark, prevFrameMark, packet, mFramesToSkipFollowingSeek);
#endif
}

// _______________________________________________________________________________________
//
void	CAAudioFile::ReadPackets(UInt32 &ioNumPackets, AudioBufferList *ioData)
			// May read fewer packets than requested if:
			//		buffer is not big enough
			//		file does not contain that many more packets
			// Note that eofErr is not fatal, just results in 0 packets returned
			// ioData's buffer sizes may be shortened
{
	UInt32 bufferSizeBytes = ioData->mBuffers[0].mDataByteSize;
	UInt32 maxNumPackets = bufferSizeBytes / mClientMaxPacketSize;	
	// older versions of AudioConverterFillComplexBuffer don't do this, so do our own sanity check
	UInt32 nPackets = std::min(ioNumPackets, maxNumPackets);
	
	mMaxPacketsToRead = ~0;
	
	if (mClientDataFormat.mFramesPerPacket == 1) {  // PCM or equivalent
		while (mFramesToSkipFollowingSeek > 0) {
			UInt32 skipFrames = std::min(mFramesToSkipFollowingSeek, maxNumPackets);
			if (mFileDataFormat.mFramesPerPacket > 0)
				mMaxPacketsToRead = skipFrames / mFileDataFormat.mFramesPerPacket + 1;

			if (mConverter == NULL) {
				XThrowIfError(ReadInputProc(NULL, &skipFrames, ioData, NULL, this), "read audio file");
			} else {
				StartTiming(this, fill);
				XThrowIfError(AudioConverterFillComplexBuffer(mConverter, ReadInputProc, this, &skipFrames, ioData, NULL), "convert audio packets");
				ElapsedTime(this, fill, mTicksInConverter);
			}
			if (skipFrames == 0) {	// hit EOF
				ioNumPackets = 0;
				return;
			}
#if VERBOSE_IO
			printf("CAAudioFile::ReadPackets: skipped %ld frames\n", skipFrames);
#endif

			mFramesToSkipFollowingSeek -= skipFrames;

			// restore mDataByteSize
			for (int i = ioData->mNumberBuffers; --i >= 0 ; )
				ioData->mBuffers[i].mDataByteSize = bufferSizeBytes;
		}
	}
	
	if (mFileDataFormat.mFramesPerPacket > 0)
		mMaxPacketsToRead = nPackets / mFileDataFormat.mFramesPerPacket + 1;
	if (mConverter == NULL) {
		XThrowIfError(ReadInputProc(NULL, &nPackets, ioData, NULL, this), "read audio file");
	} else {
		StartTiming(this, fill);
		XThrowIfError(AudioConverterFillComplexBuffer(mConverter, ReadInputProc, this, &nPackets, ioData, NULL),
			"convert audio packets");
		ElapsedTime(this, fill, mTicksInConverter);
	}
	
	ioNumPackets = nPackets;
}

// _______________________________________________________________________________________
//
OSStatus CAAudioFile::ReadInputProc(	AudioConverterRef				inAudioConverter,
										UInt32*							ioNumberDataPackets,
										AudioBufferList*				ioData,
										AudioStreamPacketDescription**	outDataPacketDescription,
										void*							inUserData)
{
	CAAudioFile *This = static_cast<CAAudioFile *>(inUserData);

	SInt64 remainingPacketsInFile = This->mNumberPackets - This->mPacketMark;
	if (remainingPacketsInFile <= 0) {
		*ioNumberDataPackets = 0;
		ioData->mBuffers[0].mDataByteSize = 0;
		if (outDataPacketDescription)
			*outDataPacketDescription = This->mPacketDescs;
#if VERBOSE_IO
		printf("CAAudioFile::ReadInputProc: EOF\n");
#endif
		return noErr;	// not eofErr; EOF is signified by 0 packets/0 bytes
	}
	AudioBufferList *readBuffer;
	UInt32 ioPackets;
	if (inAudioConverter != NULL) {
		// getting called from converter, need to use our I/O buffer
		readBuffer = &This->mIOBufferList;
		ioPackets = This->mIOBufferSizePackets;
	} else {
		// getting called directly from ReadPackets, use supplied buffer
		readBuffer = ioData;
		ioPackets = std::min(*ioNumberDataPackets, readBuffer->mBuffers[0].mDataByteSize / This->mFileMaxPacketSize);
			// don't attempt to read more packets than will fit in the buffer
	}
	// don't try to read past EOF
	if (ioPackets > remainingPacketsInFile)
		ioPackets = remainingPacketsInFile;
	if (ioPackets > This->mMaxPacketsToRead) {
#if VERBOSE_IO
		printf("CAAudioFile::ReadInputProc: limiting read to %ld packets (from %ld)\n", This->mMaxPacketsToRead, ioPackets);
#endif
		ioPackets = This->mMaxPacketsToRead;
	}

	UInt32 bytesRead;
	OSStatus err;
	
	StartTiming(This, read);
	err = AudioFileReadPackets(This->mAudioFile, This->mUseCache, &bytesRead, This->mPacketDescs,
				This->mPacketMark, &ioPackets, readBuffer->mBuffers[0].mData);
	ElapsedTime(This, read, This->mTicksInIO);

	if (err) {
		DebugMessageN1("Error %ld from AudioFileReadPackets!!!\n", err);
		return err;
	}
	
#if VERBOSE_IO
	printf("CAAudioFile::ReadInputProc: read %ld packets (%qd-%qd), %ld bytes\n", ioPackets, This->mPacketMark, This->mPacketMark + ioPackets, bytesRead);
#if VERBOSE_IO >= 2
	if (This->mPacketDescs) {
		for (UInt32 i = 0; i < ioPackets; ++i) {
			printf("  read packet %qd : offset %qd, length %ld\n", This->mPacketMark + i, This->mPacketDescs[i].mStartOffset, This->mPacketDescs[i].mDataByteSize);
		}
	}
	printf("  read buffer:"); CAShowAudioBufferList(readBuffer, 0, 4);
#endif
#endif
	if (outDataPacketDescription)
		*outDataPacketDescription = This->mPacketDescs;
	ioData->mBuffers[0].mDataByteSize = bytesRead;
	ioData->mBuffers[0].mData = readBuffer->mBuffers[0].mData;
	This->mPacketMark += ioPackets;
	if (This->mFileDataFormat.mFramesPerPacket > 0)
		This->mFrameMark += ioPackets * This->mFileDataFormat.mFramesPerPacket;
	else {
		for (UInt32 i = 0; i < ioPackets; ++i)
			This->mFrameMark += This->mPacketDescs[i].mVariableFramesInPacket;
	}

	*ioNumberDataPackets = ioPackets;
	return noErr;
}

// _______________________________________________________________________________________
//
void	CAAudioFile::WritePackets(UInt32 numPackets, const AudioBufferList *data)
{
	if (mMode == kPreparingToWrite)
		mMode = kWriting;
	else
		XThrowIf(mMode != kWriting, kExtAudioFileError_InvalidOperationOrder, "can't write to this file");
	if (mConverter != NULL) {
		mWritePackets = numPackets;
		mWriteBufferList->SetFrom(data);
		WritePacketsFromCallback(WriteInputProc, this);
	} else {
		StartTiming(this, write);
		XThrowIfError(AudioFileWritePackets(mAudioFile, mUseCache, data->mBuffers[0].mDataByteSize, 
						NULL, mPacketMark, &numPackets, data->mBuffers[0].mData),
						"write audio file");
		ElapsedTime(this, write, mTicksInIO);
#if VERBOSE_IO
		printf("CAAudioFile::WritePackets: wrote %ld packets at %qd, %ld bytes\n", numPackets, mPacketMark, data->mBuffers[0].mDataByteSize);
#endif
		mNumberPackets = (mPacketMark += numPackets);
		if (mFileDataFormat.mFramesPerPacket > 0)
			mFrameMark += numPackets * mFileDataFormat.mFramesPerPacket;
		// else: shouldn't happen since we're only called when there's no converter
	}
}

// _______________________________________________________________________________________
//
void	CAAudioFile::FlushEncoder()
{
	if (mConverter != NULL) {
		mFinishingEncoding = true;
		WritePacketsFromCallback(WriteInputProc, this);
		mFinishingEncoding = false;
	}
}

// _______________________________________________________________________________________
//
OSStatus CAAudioFile::WriteInputProc(	AudioConverterRef				inAudioConverter,
										UInt32 *						ioNumberDataPackets,
										AudioBufferList*				ioData,
										AudioStreamPacketDescription **	outDataPacketDescription,
										void*							inUserData)
{
	CAAudioFile *This = static_cast<CAAudioFile *>(inUserData);
	if (This->mFinishingEncoding) {
		*ioNumberDataPackets = 0;
		ioData->mBuffers[0].mDataByteSize = 0;
		ioData->mBuffers[0].mData = NULL;
		if (outDataPacketDescription)
			*outDataPacketDescription = NULL;
		return noErr;
	}
	UInt32 numPackets = This->mWritePackets;
	if (numPackets == 0) {
		return kNoMoreInputRightNow;
	}
	This->mWriteBufferList->ToAudioBufferList(ioData);
	This->mWriteBufferList->BytesConsumed(numPackets * This->mClientDataFormat.mBytesPerFrame);
	*ioNumberDataPackets = numPackets;
	if (outDataPacketDescription)
		*outDataPacketDescription = NULL;
	This->mWritePackets -= numPackets;
	return noErr;
}

// _______________________________________________________________________________________
//
#if VERBOSE_IO
static void	hexdump(const void *addr, long len)
{
	const Byte *p = (Byte *)addr;
	UInt32 offset = 0;
	
	if (len > 0x400) len = 0x400;
	
	while (len > 0) {
		int n = len > 16 ? 16 : len;
		printf("%08lX:  ", offset);
		for (int i = 0; i < 16; ++i)
			if (i < n)
				printf("%02X ", p[i]);
			else printf("   ");
		for (int i = 0; i < 16; ++i)
			if (i < n)
				putchar(p[i] >= ' ' && p[i] < 127 ? p[i] : '.');
			else putchar(' ');
		putchar('\n');
		p += 16;
		len -= 16;
		offset += 16;
	}
}
#endif

// _______________________________________________________________________________________
//
void	CAAudioFile::WritePacketsFromCallback(
								AudioConverterComplexInputDataProc	inInputDataProc,
								void *								inInputDataProcUserData)
{
	while (true) {
		// keep writing until we exhaust the input (temporary stop), or produce no output (EOF)
		UInt32 numEncodedPackets = mIOBufferSizePackets;
		mIOBufferList.mBuffers[0].mDataByteSize = mIOBufferSizeBytes;
		StartTiming(this, fill);
		OSStatus err = AudioConverterFillComplexBuffer(mConverter, inInputDataProc, inInputDataProcUserData, 
					&numEncodedPackets, &mIOBufferList, mPacketDescs);
		ElapsedTime(this, fill, mTicksInConverter);
		XThrowIf(err != 0 && err != kNoMoreInputRightNow, err, "convert audio packets");
		if (numEncodedPackets == 0)
			break;
		Byte *buf = (Byte *)mIOBufferList.mBuffers[0].mData;
#if VERBOSE_IO
		printf("CAAudioFile::WritePacketsFromCallback: wrote %ld packets, %ld bytes\n", numEncodedPackets, mIOBufferList.mBuffers[0].mDataByteSize);
		if (mPacketDescs) {
			for (UInt32 i = 0; i < numEncodedPackets; ++i) {
				printf("  write packet %qd : offset %qd, length %ld\n", mPacketMark + i, mPacketDescs[i].mStartOffset, mPacketDescs[i].mDataByteSize);
				hexdump(buf + mPacketDescs[i].mStartOffset, mPacketDescs[i].mDataByteSize);
			}
		}
#endif
		StartTiming(this, write);
		XThrowIfError(AudioFileWritePackets(mAudioFile, mUseCache, mIOBufferList.mBuffers[0].mDataByteSize, 
						mPacketDescs, mPacketMark, &numEncodedPackets, buf),
						"write audio file");
		ElapsedTime(this, write, mTicksInIO);
		mPacketMark += numEncodedPackets;
		mNumberPackets += numEncodedPackets;
		if (mFileDataFormat.mFramesPerPacket > 0)
			mFrameMark += numEncodedPackets * mFileDataFormat.mFramesPerPacket;
		else {
			for (UInt32 i = 0; i < numEncodedPackets; ++i)
				mFrameMark += mPacketDescs[i].mVariableFramesInPacket;
		}
		if (err == kNoMoreInputRightNow)
			break;
	}
}


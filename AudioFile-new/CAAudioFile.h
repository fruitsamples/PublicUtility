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
	CAAudioFile.h
	
=============================================================================*/

#ifndef __CAAudioFile_h__
#define __CAAudioFile_h__

#if 0
#include <AudioToolbox/AudioFile.h> // avoid AT framework master include - why?
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFormat.h>
#endif
#include <AudioToolbox/AudioToolbox.h>

#include "CAStreamBasicDescription.h"
#include "CABufferList.h"
#include "CAAudioChannelLayout.h"
#include "CAXException.h"
#include "CAMath.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_4
	typedef UInt32 AudioFileTypeID;
	enum {
		kExtAudioFileError_InvalidProperty			= -66561,
		kExtAudioFileError_InvalidPropertySize		= -66562,
		kExtAudioFileError_NonPCMClientFormat		= -66563,
		kExtAudioFileError_InvalidChannelMap,		= -66564	// number of channels doesn't match format
		kExtAudioFileError_InvalidOperationOrder	= -66565,
		kExtAudioFileError_InvalidDataFormat		= -66566,
		kExtAudioFileError_MaxPacketSizeUnknown		= -66567
	};
#else
	#include <AudioToolbox/ExtendedAudioFile.h>
#endif

// _______________________________________________________________________________________
// Wrapper class for an AudioFile, supporting encode/decode to/from a PCM client format
class CAAudioFile {
public:
	CAAudioFile();
	virtual ~CAAudioFile();
	
	// --- second-stage initializers ---
	// Use exactly one of the following:
	//		- Open
	//		- PrepareNew followed by Create
	//		- Wrap
	
	void	Open(const char *filePath);
	void	Open(const FSRef &fsref);
				// open an existing file

	void	PrepareNew(const CAStreamBasicDescription &dataFormat, const CAAudioChannelLayout *layout=NULL);
				// for encoding: call PrepareNew, SetClientFormat, [SetConverterProperty...], Create
				// The dataFormat may have a sample rate of 0, in which case sampleRate must
				//	specify the real sample rate.
	void	Create(const char *filePath, AudioFileTypeID fileType);
	void	Create(const FSRef &parentDir, CFStringRef filename, AudioFileTypeID fileType);

	void	Wrap(AudioFileID fileID, bool forWriting);
				// use this to wrap an AudioFileID opened externally

	// ---

	void	Close();
				// In case you want to close the file before the destructor executes
	void	Delete();
	
	// --- Data formats ---

	// Allow specifying the file's channel layout. Must be called before SetClientFormat.
	// When writing, the specified channel layout is written to the file (if the file format supports
	// the channel layout). When reading, the specified layout overrides the one read from the file,
	// if any.
	void	SetFileChannelLayout(const CAAudioChannelLayout &layout);
	
	// This specifies the data format which the client will use for reading/writing the file,
	// which may be different from the file's format. An AudioConverter is created if necessary.
	// The client format must be linear PCM.
	void	SetClientFormat(const CAStreamBasicDescription &dataFormat, const CAAudioChannelLayout *layout=NULL);
	void	SetClientDataFormat(const CAStreamBasicDescription &dataFormat);
	void	SetClientChannelLayout(const CAAudioChannelLayout &layout);
	
	// Wrapping the underlying converter, if there is one
	OSStatus	SetConverterProperty(AudioConverterPropertyID	inPropertyID,
									UInt32						inPropertyDataSize,
									const void *				inPropertyData,
									bool						inCanFail = false);
	void		SetConverterConfig(CFArrayRef config) {
					SetConverterProperty(kAudioConverterPropertySettings, sizeof(config), &config); }
	CFArrayRef  GetConverterConfig();
	
	// --- I/O ---
	// All I/O is sequential, but you can seek to an arbitrary position when reading.
	// SeekToPacket and TellPacket's packet numbers are in the file's data format, not the client's.
	// However, ReadPackets/WritePackets use packet counts in the client data format.

	void	SeekToPacket(SInt64 packetNumber);
	SInt64	TellPacket() const { return mPacketMark; }  // will be imprecise if SeekToFrame was called
	
	void	ReadPackets(UInt32 &ioNumPackets, AudioBufferList *ioData);
	void	WritePackets(UInt32 numPackets, const AudioBufferList *data);

	// These can fail for files without a constant mFramesPerPacket
	void	SeekToFrame(SInt64 frameNumber);
	SInt64  TellFrame() const;
	
	// --- Accessors ---
	// note: client parameters only valid if SetClientFormat has been called
	AudioFileID						GetAudioFileID() const { return mAudioFile; }
	operator AudioFileID () const { return mAudioFile; }
	const CAStreamBasicDescription &GetFileDataFormat() const { return mFileDataFormat; }
	const CAStreamBasicDescription &GetClientDataFormat() const { return mClientDataFormat; }
	const CAAudioChannelLayout &	GetFileChannelLayout() const { return mFileChannelLayout; }
	const CAAudioChannelLayout &	GetClientChannelLayout() const { return mClientChannelLayout; }
	bool							HasConverter() const { return (mConverter != NULL); }
	AudioConverterRef				GetConverter() const { return mConverter; }

	UInt32	GetFileMaxPacketSize() const { return mFileMaxPacketSize; }
	UInt32	GetClientMaxPacketSize() const { return mClientMaxPacketSize; }
	SInt64	GetNumberPackets() const { return mNumberPackets; }
	SInt64  GetNumberFrames() const { return mFileDataFormat.mFramesPerPacket * mNumberPackets; }
				// will be 0 if the file's frames/packet is 0 (variable)
	double  GetDurationSeconds() const { return fnonzero(mFileDataFormat.mSampleRate) ? GetNumberFrames() / mFileDataFormat.mSampleRate : 0; }
				// will be 0 if the file's frames/packet is 0 (variable)
				// or the file's sample rate is 0 (unknown)
	
	// --- Tunable performance parameters ---
	void	SetUseCache(bool b) { mUseCache = b; }
	void	SetIOBufferSize(UInt32 bufferSizeBytes);
	UInt32  GetIOBufferSize() { return mIOBufferSizeBytes; }
	void *  GetIOBuffer() { return mIOBufferList.mBuffers[0].mData; }
	void	SetIOBuffer(void *buf);
	
	// -- Profiling ---
#if CAAUDIOFILE_PROFILE
	void	EnableProfiling(bool b) { mProfiling = b; }
	UInt64	TicksInConverter() const { return (mTicksInConverter > 0) ? (mTicksInConverter - mTicksInIO) : 0; }
	UInt64	TicksInIO() const { return mTicksInIO; }
#endif
	
// _______________________________________________________________________________________
private:
	void	SetConverterChannelLayout(bool output, const CAAudioChannelLayout &layout);
	void	WritePacketsFromCallback(
									AudioConverterComplexInputDataProc	inInputDataProc,
									void *								inInputDataProcUserData);
				// will use I/O buffer size
	void	InitFileMaxPacketSize();
	void	FileFormatChanged(const FSRef *parentDir=0, CFStringRef filename=0, AudioFileTypeID filetype=0);

// _______________________________________________________________________________________
private:
	void	GetExistingFileInfo();
	void	FlushEncoder();
	void	CloseConverter();
	void	UpdateClientMaxPacketSize();
	SInt64  PacketToFrame(SInt64 packet) const;
	SInt64	FrameToPacket(SInt64 inFrame) const;

	static OSStatus ReadInputProc(		AudioConverterRef				inAudioConverter,
										UInt32*							ioNumberDataPackets,
										AudioBufferList*				ioData,
										AudioStreamPacketDescription**	outDataPacketDescription,
										void*							inUserData);	

	static OSStatus WriteInputProc(		AudioConverterRef				inAudioConverter,
										UInt32*							ioNumberDataPackets,
										AudioBufferList*				ioData,
										AudioStreamPacketDescription**	outDataPacketDescription,
										void*							inUserData);	
	// the file
	FSRef						mFSRef;
	AudioFileID					mAudioFile;
	bool						mOwnOpenFile;
	bool						mUseCache;
	bool						mFinishingEncoding;
	enum { kClosed, kReading, kPreparingToCreate, kPreparingToWrite, kWriting, kDeleting } mMode;
	
	SInt64						mNumberPackets;		// in file's format
	SInt64						mPacketMark;		// in file's format
	SInt64						mFrameMark;			// this may be offset from the start of the file
													// by the codec's latency; i.e. our frame 0 could
													// lie at frame 2112 of a decoded AAC file
	SInt32						mFrame0Offset;
	UInt32						mFramesToSkipFollowingSeek;
	
	// buffers
	UInt32						mIOBufferSizeBytes;
	UInt32						mIOBufferSizePackets;
	AudioBufferList				mIOBufferList;
	bool						mClientOwnsIOBuffer;
	AudioStreamPacketDescription *mPacketDescs;
	
	// formats/conversion
	AudioConverterRef			mConverter;
	CAStreamBasicDescription	mFileDataFormat;
	CAStreamBasicDescription	mClientDataFormat;
	CAAudioChannelLayout		mFileChannelLayout;
	CAAudioChannelLayout		mClientChannelLayout;
	UInt32						mFileMaxPacketSize;
	UInt32						mClientMaxPacketSize;
	
	// cookie
	Byte *						mMagicCookie;
	UInt32						mMagicCookieSize;
	
	// for ReadPackets
	UInt32						mMaxPacketsToRead;
	
	// for WritePackets
	UInt32						mWritePackets;
	CABufferList *				mWriteBufferList;
	
#if CAAUDIOFILE_PROFILE
	// performance
	bool						mProfiling;
	UInt64						mTicksInConverter;
	UInt64						mTicksInIO;
#endif
};

#endif // __CAAudioFile_h__

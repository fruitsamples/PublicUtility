/*	Copyright: 	© Copyright 2003 Apple Computer, Inc. All rights reserved.

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
	CAAudioUnit.h
 
=============================================================================*/

#ifndef __CAAudioUnit_h__
#define __CAAudioUnit_h__

#include <CoreServices/CoreServices.h>

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AUComponent.h>
#include <AudioToolbox/AUGraph.h>
#include <vector>
#include "CAStreamBasicDescription.h"
#include "CAComponent.h"
#include "CAAudioChannelLayout.h"

// These constructors will NOT throw exceptions - so "check" after creation if AU IsValid()
// The destructor will NOT automatically close the AU down
// This state should be managed by the Caller
// once closed, the unit represented by this object is no longer valid
// it is up to the user of this object to ensure its validity is in sync 
// if it is removed from a graph

// methods that can significantly change the state of the AU (like its format) are
// NOT const whereas those that don't change the externally related state of the AU are not const

class CAAudioUnit {
public:
	typedef std::vector<AudioChannelLayoutTag> 	ChannelTagVector;
	typedef ChannelTagVector::iterator 			ChannelTagVectorIter;

public:
							CAAudioUnit () 
								: mDataPtr(0) {}

							CAAudioUnit (const AudioUnit& inUnit);

							CAAudioUnit (const AUNode &inNode, const AudioUnit& inUnit);

							CAAudioUnit (const CAAudioUnit& y)
								: mDataPtr(0) { *this = y; }

	static OSStatus			Open (const CAComponent& inComp, CAAudioUnit &outUnit);

							~CAAudioUnit ();

	
	CAAudioUnit& operator= (const CAAudioUnit& y);

#pragma mark __State Management	
	bool					IsValid () const;
	
	AudioUnit				AU() const;
	operator AudioUnit () const { return AU(); }

	const CAComponent&		Comp() const { return mComp; }
	
	bool					FromAUGraph () const { return GetAUNode() != 0 || GetAUNode() != -1; }
	
	AUNode					GetAUNode () const;
	
#pragma mark __API Wrapper
	OSStatus				Initialize() { return AudioUnitInitialize(AU()); }
	OSStatus				Uninitialize() { return AudioUnitUninitialize(AU()); }
	OSStatus				GetPropertyInfo(AudioUnitPropertyID propID, AudioUnitScope scope, AudioUnitElement element,
											UInt32 *outDataSize, Boolean *outWritable)
							{
								return AudioUnitGetPropertyInfo(AU(), propID, scope, element, outDataSize, outWritable);
							}
	OSStatus				GetProperty(AudioUnitPropertyID propID, AudioUnitScope scope, AudioUnitElement element,
											void *outData, UInt32 *ioDataSize)
							{
								return AudioUnitGetProperty(AU(), propID, scope, element, outData, ioDataSize);
							}
	OSStatus				SetProperty(AudioUnitPropertyID propID, AudioUnitScope scope, AudioUnitElement element,
											const void *inData, UInt32 inDataSize)
							{
								return AudioUnitSetProperty(AU(), propID, scope, element, inData, inDataSize);
							}
	OSStatus				SetParameter(AudioUnitParameterID inID, AudioUnitScope scope, AudioUnitElement element,
											Float32 value, UInt32 bufferOffsetFrames=0)
							{
								return AudioUnitSetParameter(AU(), inID, scope, element, value, bufferOffsetFrames);
							}
	OSStatus				GetParameter(AudioUnitParameterID inID, AudioUnitScope scope, AudioUnitElement element,
											Float32 &outValue)
							{
								return AudioUnitGetParameter(AU(), inID, scope, element, &outValue);
							}
	OSStatus				Reset (AudioUnitScope scope, AudioUnitElement element)
							{
								return AudioUnitReset (AU(), scope, element);
							}
	OSStatus				GlobalReset ()
							{
								return AudioUnitReset (AU(), kAudioUnitScope_Global, 0);
							}
							
#pragma mark __Format Utilities
		// typically you ask this about an AU
		// These Questions are asking about Input and Output...
	bool					CanDo (int inChannelsInOut) const
							{
								return CanDo (inChannelsInOut, inChannelsInOut);
							}
							
	bool					CanDo (		int 				inChannelsIn, 
										int 				inChannelsOut) const;
	
	bool					HasChannelLayouts (AudioUnitScope 		inScope, 
											AudioUnitElement 		inEl) const;
		
	bool					GetChannelLayouts (AudioUnitScope 		inScope,
									AudioUnitElement 				inEl,
									ChannelTagVector				&outChannelVector) const;
	
	OSStatus				GetChannelLayout (AudioUnitScope 		inScope,
											AudioUnitElement 		inEl,
											CAAudioChannelLayout	&outLayout) const;	

	OSStatus				SetChannelLayout (AudioUnitScope 		inScope, 
											AudioUnitElement 		inEl,
											CAAudioChannelLayout	&inLayout);

	OSStatus				SetChannelLayout (AudioUnitScope 		inScope, 
											AudioUnitElement 		inEl,
											AudioChannelLayout		&inLayout,
											UInt32					inSize);
											
	OSStatus				ClearChannelLayout (AudioUnitScope		inScope,
											AudioUnitElement		inEl);
												
	OSStatus				GetFormat (AudioUnitScope					inScope,
											AudioUnitElement			inEl,
											AudioStreamBasicDescription	&outFormat) const;
	// if an AudioChannelLayout is either required or set, this call can fail
	// and the SetChannelLayout call should be used to set the format
	OSStatus				SetFormat (AudioUnitScope							inScope,
											AudioUnitElement					inEl,
											const AudioStreamBasicDescription	&inFormat);

	OSStatus				GetSampleRate (AudioUnitScope		inScope,
											AudioUnitElement	inEl,
											Float64				&outRate) const;
	OSStatus				SetSampleRate (AudioUnitScope		inScope,
											AudioUnitElement	inEl,
											Float64				inRate);

	OSStatus				GetNumChannels (AudioUnitScope		inScope,
											AudioUnitElement	inEl,
											UInt32				&outChans) const;

	bool					CanBypass 		() const;

	bool					GetBypass 		() const;

	OSStatus				SetBypass 		(bool				inBypass) const;
	
		// these calls just deal with the global preset state
		// you could rescope them to deal with presets on the part scope
	OSStatus				GetAUPreset (CFPropertyListRef &outData) const;

	OSStatus				SetAUPreset (CFPropertyListRef &inData);
	
	OSStatus				GetPresentPreset (AUPreset &outData) const;
	
	OSStatus				SetPresentPreset (AUPreset &inData);
	
#pragma mark __Print	
	void					Print () const { Print (stdout); }
	void					Print (FILE* file) const;
	
private:
	CAComponent				mComp;
	
	class AUState;
	AUState*		mDataPtr;
		
		// this can throw - so wrap this up in a static that returns a result code...
	CAAudioUnit (const CAComponent& inComp);
};
#endif

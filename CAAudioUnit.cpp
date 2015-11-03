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
	CAAudioUnit.cpp
 
=============================================================================*/

#include "CAAudioUnit.h"
#include "CAReferenceCounted.h"

class CAAudioUnit::AUState : public CAReferenceCounted  {
public:
	AUState (Component inComp)
						: mUnit(0), mNode (0)
						{ 
							OSStatus result = ::OpenAComponent (inComp, &mUnit); 
							if (result)
								throw result;
						}

	AUState (const AUNode &inNode, const AudioUnit& inUnit)
						: mUnit (inUnit), mNode (inNode) 
						{}
	~AUState();
											
	AudioUnit			mUnit;
	AUNode				mNode;

private:
		// get the compiler to tell us when we do a bad thing!!!
	AUState () {}
	AUState (const AUState&) {}
	AUState& operator= (const AUState&) { return *this; } 
};

CAAudioUnit::AUState::~AUState ()
{
	if (mUnit && (mNode == 0)) {
		::CloseComponent (mUnit);
		mUnit = 0;
	}
	mNode = 0;
}

OSStatus		CAAudioUnit::Open (const CAComponent& inComp, CAAudioUnit &outUnit)
{
	try {
		outUnit = inComp; 
		return noErr;
	} catch (OSStatus res) {
		return res;
	} catch (...) {
		return -1;
	}
}

CAAudioUnit::CAAudioUnit (const AudioUnit& inUnit)
	: mComp (inUnit), mDataPtr (new AUState (-1, inUnit))
{
}

CAAudioUnit::CAAudioUnit (const CAComponent& inComp)
	: mComp (inComp), mDataPtr (0)
{
	if (mComp.Desc().IsAU()) {
		mDataPtr = new AUState (mComp.Comp());
	} else {
		throw static_cast<OSStatus>(paramErr);
	}
}

CAAudioUnit::CAAudioUnit (const AUNode &inNode, const AudioUnit& inUnit)
	: mComp (inUnit), mDataPtr(new AUState (inNode, inUnit)) 
{
}

CAAudioUnit::~CAAudioUnit ()
{
	if (mDataPtr) mDataPtr->release();
}

CAAudioUnit&	CAAudioUnit::operator= (const CAAudioUnit &a)
{
	if (mDataPtr != a.mDataPtr) {
		if (mDataPtr)
			mDataPtr->release();
	
		if ((mDataPtr = a.mDataPtr) != NULL)
			mDataPtr->retain();
	}
	mComp = a.mComp;
	
	return *this;
}

#pragma mark __State Management	

bool			CAAudioUnit::IsValid () const 
{ 
	return mDataPtr ? mDataPtr->mUnit != 0 : false; 
}
	
AudioUnit		CAAudioUnit::AU() const 
{ 
	return mDataPtr ? mDataPtr->mUnit : 0; 
}

AUNode			CAAudioUnit::GetAUNode () const
{
	return mDataPtr ? mDataPtr->mNode : 0; 
}

#pragma mark __Format Handling
	
bool		CAAudioUnit::CanDo (	int 				inChannelsIn, 
									int 				inChannelsOut) const
{		
	// this is the default assumption of an audio effect unit
	Boolean* isWritable = 0;
	UInt32	dataSize = 0;
		// lets see if the unit has any channel restrictions
	OSStatus result = AudioUnitGetPropertyInfo (AU(),
									kAudioUnitProperty_SupportedNumChannels,
									kAudioUnitScope_Global, 0,
									&dataSize, isWritable); //don't care if this is writable
		
		// if this property is NOT implemented an FX unit
		// is expected to deal with same channel valance in and out
	if (result == kAudioUnitErr_InvalidProperty) {
		if (Comp().Desc().IsEffect() && (inChannelsIn == inChannelsOut)
			|| Comp().Desc().IsOffline() && (inChannelsIn == inChannelsOut))
			return true;
						
			// the au should either really tell us about this
			// of we will assume the worst
		else {
			return false;
		}
	}
	
	bool canDo = false;
		// ok so, we have a channel map lets see what it can do
	AUChannelInfo* info = (AUChannelInfo*)malloc (dataSize);
	result = AudioUnitGetProperty (AU(),
							kAudioUnitProperty_SupportedNumChannels,
							kAudioUnitScope_Global, 0,
							info, &dataSize);
	if (result) { goto home; }

	//now chan layout can contain -1 for either scope (ie. doesn't care)
	unsigned int numChans;
	numChans = (dataSize / sizeof (AUChannelInfo));
	for (unsigned int i = 0; i < numChans; ++i)
	{
			//check wild cards on both scopes
		if ((info[i].inChannels < 0) && (info[i].outChannels < 0))
		{
				// matches as long as channels in/out are the same
			if (info[i].inChannels == info[i].outChannels) {
				if (inChannelsOut == inChannelsIn) {
					canDo = true;
					break;
				}
			} else {
					//matches for any channels in/out
				canDo = true;
				break;
			}
		}
			// wild card on input
		else if ((info[i].inChannels < 0) && (info[i].outChannels == inChannelsOut)) {
			canDo = true;
			break;
		}
			// wild card on output
		else if ((info[i].outChannels < 0) && (info[i].inChannels == inChannelsIn)) {
			canDo = true;
			break;
		}
			// both chans in struct >= 0 - thus has to explicitly match
		else if ((info[i].inChannels == inChannelsIn) && (info[i].outChannels == inChannelsOut)) {
			canDo = true;
			break;
		} 
			// now check to see if a wild card on the args (in or out chans is zero) is found 
			// to match just one side of the scopes
		else if (inChannelsIn == 0) {
			if (info[i].outChannels == inChannelsOut) {
				canDo = true;
				break;
			}
		}
		else if (inChannelsOut == 0) {
			if (info[i].inChannels == inChannelsIn) {
				canDo = true;
				break;
			}
		}
	}
home:
	free (info);
	return canDo;
}

bool		CAAudioUnit::GetChannelLayouts (AudioUnitScope 			inScope,
										AudioUnitElement 			inEl,
										ChannelTagVector			&outChannelVector) const
{
	if (HasChannelLayouts (inScope, inEl) == false) return false; 

	UInt32 dataSize;
	OSStatus result = AudioUnitGetPropertyInfo (AU(),
								kAudioUnitProperty_SupportedChannelLayoutTags,
								inScope, inEl,
								&dataSize, NULL);

	if (result == kAudioUnitErr_InvalidProperty) {
		// if we get here we can do layouts but we've got the speaker config property
		outChannelVector.erase (outChannelVector.begin(), outChannelVector.end());
		outChannelVector.push_back (kAudioChannelLayoutTag_Stereo);
		outChannelVector.push_back (kAudioChannelLayoutTag_StereoHeadphones);
		outChannelVector.push_back (kAudioChannelLayoutTag_Quadraphonic);
		outChannelVector.push_back (kAudioChannelLayoutTag_AudioUnit_5_0);
		return true;
	}

	if (result) return false;
	
	bool canDo = false;
		// OK lets get our channel layouts and see if the one we want is present
	AudioChannelLayoutTag* info = (AudioChannelLayoutTag*)malloc (dataSize);
	result = AudioUnitGetProperty (AU(),
							kAudioUnitProperty_SupportedChannelLayoutTags,
							inScope, inEl,
							info, &dataSize);
	if (result) goto home;
	
	outChannelVector.erase (outChannelVector.begin(), outChannelVector.end());
	for (unsigned int i = 0; i < (dataSize / sizeof (AudioChannelLayoutTag)); ++i)
		outChannelVector.push_back (info[i]);

home:
	free (info);
	return canDo;
}

bool		CAAudioUnit::HasChannelLayouts (AudioUnitScope 		inScope, 
										AudioUnitElement 		inEl) const
{
	OSStatus result = AudioUnitGetPropertyInfo (AU(),
									kAudioUnitProperty_SupportedChannelLayoutTags,
									inScope, inEl,
									NULL, NULL);
	return !result;
}

OSStatus	CAAudioUnit::GetChannelLayout (AudioUnitScope 		inScope,
										AudioUnitElement 		inEl,
										CAAudioChannelLayout	&outLayout) const
{
	UInt32 size;
	OSStatus result = AudioUnitGetPropertyInfo (AU(), kAudioUnitProperty_AudioChannelLayout,
									inScope, inEl, &size, NULL);
	if (result) return result;
	
	AudioChannelLayout *layout = (AudioChannelLayout*)malloc (size);

	require_noerr (result = AudioUnitGetProperty (AU(), kAudioUnitProperty_AudioChannelLayout,
									inScope, inEl, layout, &size), home);

	outLayout = CAAudioChannelLayout (layout);
	
home:
	free (layout);
	return result;
}

OSStatus	CAAudioUnit::SetChannelLayout (AudioUnitScope 		inScope,
									AudioUnitElement 			inEl,
									CAAudioChannelLayout 		&inLayout)
{
	OSStatus result = AudioUnitSetProperty (AU(),
									kAudioUnitProperty_AudioChannelLayout,
									inScope, inEl,
									inLayout, inLayout.Size());
	return result;
}

OSStatus	CAAudioUnit::SetChannelLayout (AudioUnitScope 			inScope, 
											AudioUnitElement 		inEl,
											AudioChannelLayout		&inLayout,
											UInt32					inSize)
{
	OSStatus result = AudioUnitSetProperty (AU(),
									kAudioUnitProperty_AudioChannelLayout,
									inScope, inEl,
									&inLayout, inSize);
	return result;
}

OSStatus		CAAudioUnit::ClearChannelLayout (AudioUnitScope	inScope,
											AudioUnitElement	inEl)
{
	return AudioUnitSetProperty (AU(),
							kAudioUnitProperty_AudioChannelLayout,
							inScope, inEl, NULL, 0);
}

OSStatus	CAAudioUnit::GetFormat (AudioUnitScope				inScope,
									AudioUnitElement			inEl,
									AudioStreamBasicDescription	&outFormat) const
{
	UInt32 dataSize = sizeof (AudioStreamBasicDescription);
	return AudioUnitGetProperty (AU(), kAudioUnitProperty_StreamFormat,
								inScope, inEl, 
								&outFormat, &dataSize);
}

OSStatus	CAAudioUnit::SetFormat (AudioUnitScope						inScope,
									AudioUnitElement					inEl,
									const AudioStreamBasicDescription	&inFormat)
{
	return AudioUnitSetProperty (AU(), kAudioUnitProperty_StreamFormat,
								inScope, inEl,
								const_cast<AudioStreamBasicDescription*>(&inFormat), 
								sizeof (AudioStreamBasicDescription));
}

OSStatus	CAAudioUnit::GetSampleRate (AudioUnitScope		inScope,
										AudioUnitElement	inEl,
										Float64				&outRate) const
{
	UInt32 dataSize = sizeof (Float64);
	return AudioUnitGetProperty (AU(), kAudioUnitProperty_SampleRate,
								inScope, inEl, 
								&outRate, &dataSize);
}

OSStatus	CAAudioUnit::SetSampleRate (AudioUnitScope		inScope,
										AudioUnitElement	inEl,
										Float64				inRate)
{
	return AudioUnitSetProperty (AU(), kAudioUnitProperty_SampleRate,
								inScope, inEl,
								&inRate, sizeof (Float64));
}

OSStatus	CAAudioUnit::GetNumChannels (AudioUnitScope		inScope,
										AudioUnitElement	inEl,
										UInt32				&outChans) const
{
	AudioStreamBasicDescription desc;
	OSStatus result = GetFormat (inScope, inEl, desc);
	if (!result)
		outChans = desc.mChannelsPerFrame;
	return result;
}


bool		CAAudioUnit::CanBypass () const
{
	Boolean outWritable;
	OSStatus result = AudioUnitGetPropertyInfo (AU(), kAudioUnitProperty_BypassEffect,
									kAudioUnitScope_Global, 0,
									NULL, &outWritable);
	return (!result && outWritable);
}

bool		CAAudioUnit::GetBypass 		() const
{
	UInt32 dataSize = sizeof (UInt32);
	UInt32 outBypass;
	OSStatus result = AudioUnitGetProperty (AU(), kAudioUnitProperty_BypassEffect,
								kAudioUnitScope_Global, 0,
								&outBypass, &dataSize);
	return (result ? false : outBypass);
}

OSStatus	CAAudioUnit::SetBypass 		(bool	inBypass) const
{	
	UInt32 bypass = inBypass ? 1 : 0;
	return AudioUnitSetProperty (AU(), kAudioUnitProperty_BypassEffect,
								kAudioUnitScope_Global, 0,
								&bypass, sizeof (UInt32));
}


OSStatus	CAAudioUnit::GetAUPreset (CFPropertyListRef &outData) const
{
	UInt32 dataSize = sizeof(outData);
	return AudioUnitGetProperty (AU(), kAudioUnitProperty_ClassInfo,
								kAudioUnitScope_Global, 0,
								&outData, &dataSize);
}

OSStatus	CAAudioUnit::SetAUPreset (CFPropertyListRef &inData)
{
	return AudioUnitSetProperty (AU(), kAudioUnitProperty_ClassInfo,
								kAudioUnitScope_Global, 0,
								&inData, sizeof (CFPropertyListRef));
}

OSStatus	CAAudioUnit::GetPresentPreset (AUPreset &outData) const
{
	UInt32 dataSize = sizeof(outData);
	OSStatus result = AudioUnitGetProperty (AU(), kAudioUnitProperty_PresentPreset,
								kAudioUnitScope_Global, 0,
								&outData, &dataSize);
	if (result == kAudioUnitErr_InvalidProperty) {
		dataSize = sizeof(outData);
		result = AudioUnitGetProperty (AU(), kAudioUnitProperty_CurrentPreset,
									kAudioUnitScope_Global, 0,
									&outData, &dataSize);
		if (result == noErr) {
			// we now retain the CFString in the preset so for the client of this API
			// it is consistent (ie. the string should be released when done)
			if (outData.presetName)
				CFRetain (outData.presetName);
		}
	}
	return result;
}
	
OSStatus	CAAudioUnit::SetPresentPreset (AUPreset &inData)
{
	OSStatus result = AudioUnitSetProperty (AU(), kAudioUnitProperty_PresentPreset,
								kAudioUnitScope_Global, 0,
								&inData, sizeof (AUPreset));
	if (result == kAudioUnitErr_InvalidProperty) {
		result = AudioUnitSetProperty (AU(), kAudioUnitProperty_CurrentPreset,
								kAudioUnitScope_Global, 0,
								&inData, sizeof (AUPreset));
	}
	return result;
}

#pragma __Print Utilities

void		CAAudioUnit::Print (FILE* file) const
{
	fprintf (file, "AudioUnit:0x%X\n", int(AU()));
	if (IsValid()) { 
		fprintf (file, "\tnode=%ld\t", GetAUNode()); Comp().Print (file);
	}
}

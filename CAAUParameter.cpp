/*	Copyright: 	© Copyright 2004 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
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
	CAAUParameter.cpp
	
=============================================================================*/

#include "CAAUParameter.h"

CAAUParameter::CAAUParameter() 
	: mParamName(NULL),
	  mParamTag (0),
	  mNumIndexedParams (0),
	  mNamedParams (0)
{
}

CAAUParameter::CAAUParameter(AudioUnit au, AudioUnitParameterID param, AudioUnitScope scope, AudioUnitElement element)
	: mParamTag (0),
	  mNumIndexedParams (0),
	  mNamedParams (0)
{
	mAudioUnit = au;
	mParameterID = param;
	mScope = scope;
	mElement = element;
	
	UInt32 propertySize = sizeof(mParamInfo);
	OSStatus err = AudioUnitGetProperty(au, kAudioUnitProperty_ParameterInfo,
			scope, param, &mParamInfo, &propertySize);
	if (err)
		memset(&mParamInfo, 0, sizeof(mParamInfo));
	if (mParamInfo.flags & kAudioUnitParameterFlag_HasCFNameString) {
		mParamName = mParamInfo.cfNameString;
		if (!(mParamInfo.flags & kAudioUnitParameterFlag_CFNameRelease)) 
			CFRetain (mParamName);
	} else
		mParamName = CFStringCreateWithCString(NULL, mParamInfo.name, kCFStringEncodingUTF8);
	
	char* str = 0;
	switch (mParamInfo.unit)
	{
		case kAudioUnitParameterUnit_Boolean:
			str = "T/F";
			break;
		case kAudioUnitParameterUnit_Percent:
		case kAudioUnitParameterUnit_EqualPowerCrossfade:
			str = "%";
			break;
		case kAudioUnitParameterUnit_Seconds:
			str = "Secs";
			break;
		case kAudioUnitParameterUnit_SampleFrames:
			str = "Samps";
			break;
		case kAudioUnitParameterUnit_Phase:
		case kAudioUnitParameterUnit_Degrees:
			str = "Degr.";
			break;
		case kAudioUnitParameterUnit_Hertz:
			str = "Hz";
			break;
		case kAudioUnitParameterUnit_Cents:
		case kAudioUnitParameterUnit_AbsoluteCents:
			str = "Cents";
			break;
		case kAudioUnitParameterUnit_RelativeSemiTones:
			str = "S-T";
			break;
		case kAudioUnitParameterUnit_MIDINoteNumber:
		case kAudioUnitParameterUnit_MIDIController:
			str = "MIDI";
				//these are inclusive, so add one value here
			mNumIndexedParams = short(mParamInfo.maxValue+1 - mParamInfo.minValue);
			break;
		case kAudioUnitParameterUnit_Decibels:
			str = "dB";
			break;
		case kAudioUnitParameterUnit_MixerFaderCurve1:
		case kAudioUnitParameterUnit_LinearGain:
			str = "Gain";
			break;
		case kAudioUnitParameterUnit_Pan:
			str = "L/R";
			break;
		case kAudioUnitParameterUnit_Meters:
			str = "Mtrs";
			break;
		case kAudioUnitParameterUnit_Octaves:
			str = "8ve";
			break;
		case kAudioUnitParameterUnit_BPM:
			str = "BPM";
			break;
		case kAudioUnitParameterUnit_Beats:
			str = "Beats";
			break;
		case kAudioUnitParameterUnit_Milliseconds:
			str = "msecs";
			break;
		case kAudioUnitParameterUnit_Indexed:
			{
				propertySize = sizeof(mNamedParams);
				err = AudioUnitGetProperty (au, 
									kAudioUnitProperty_ParameterValueStrings,
									scope, 
									param, 
									&mNamedParams, 
									&propertySize);
				if (!err && mNamedParams) {
					mNumIndexedParams = CFArrayGetCount(mNamedParams);
				} else {
						//these are inclusive, so add one value here
					mNumIndexedParams = short(mParamInfo.maxValue+1 - mParamInfo.minValue);
				}
				str = NULL;
			}
			break;
		case kAudioUnitParameterUnit_CustomUnit:
		{
			CFStringRef unitName = mParamInfo.unitName;
			static char paramStr[256];
			CFStringGetCString (unitName, paramStr, 256, kCFStringEncodingUTF8);
			if (mParamInfo.flags & kAudioUnitParameterFlag_CFNameRelease)
				CFRelease (unitName);
			str = paramStr;
			break;
		}
		case kAudioUnitParameterUnit_Generic:
		case kAudioUnitParameterUnit_Rate:
		default:
			str = NULL;
			break;
	}
	
	if (str)
		mParamTag = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
	else
		mParamTag = NULL;
}

CAAUParameter::CAAUParameter(const CAAUParameter &a) :
	mParamName(NULL),
	mParamTag (0),
	mNumIndexedParams (0),
	mNamedParams (0)
{
	*this = a;
}

CAAUParameter &	CAAUParameter::operator = (const CAAUParameter &a)
{
	if (mParamName) CFRelease(mParamName);
	if (mParamTag) CFRelease(mParamTag);
	if (mNamedParams) CFRelease(mNamedParams);
	
	memcpy(this, &a, sizeof(CAAUParameter));

	if (mParamName) CFRetain(mParamName);
	if (mParamTag) CFRetain(mParamTag);
	if (mNamedParams) CFRetain(mNamedParams);
	
	return *this;
}

CAAUParameter::~CAAUParameter()
{
	if (mParamName)
		CFRelease(mParamName);
	if (mParamTag)
		CFRelease(mParamTag);
	if (mNamedParams)
		CFRelease (mNamedParams);
}

Float32		CAAUParameter::GetValue()
{
	Float32 value = 0.;
	//OSStatus err = 
	AudioUnitGetParameter(mAudioUnit, mParameterID, mScope, mElement, &value);
	return value;
}

CFStringRef CAAUParameter::GetStringFromValueCopy(const Float32 *value) 
{
	if (ValuesHaveStrings()) 
	{
		AudioUnitParameterStringFromValue stringValue;
		stringValue.inParamID = mParameterID;
		stringValue.inValue = value;
		stringValue.outString = NULL;
		UInt32 propertySize = sizeof(stringValue);
		
		OSStatus err = AudioUnitGetProperty (mAudioUnit, 
											kAudioUnitProperty_ParameterStringFromValue,
											mScope, 
											mParameterID, 
											&stringValue, 
											&propertySize);
		
		if (err == noErr && stringValue.outString != NULL)
			return stringValue.outString;
	}
	
	Float32 val = (value == NULL ? GetValue() : *value);
	char valstr[32];
	AUParameterFormatValue (val, this, valstr, (HasDisplayTransformation() ? 4 : 3));
	return CFStringCreateWithCString(NULL, valstr, kCFStringEncodingUTF8);
}

Float32 CAAUParameter::GetValueFromString(CFStringRef str)
{
	if (ValuesHaveStrings()) 
	{
		AudioUnitParameterValueFromString valueString;
		valueString.inParamID = mParameterID;
		valueString.inString = str;
		UInt32 propertySize = sizeof(valueString);
		
		OSStatus err = AudioUnitGetProperty (mAudioUnit, 
										kAudioUnitProperty_ParameterValueFromString,
										mScope, 
										mParameterID, 
										&valueString, 
										&propertySize);
										
		if (err == noErr) {
			return valueString.outValue;
		}
	}
	
	Float32 paramValue = mParamInfo.defaultValue;
	char valstr[32];
	CFStringGetCString(str, valstr, sizeof(valstr), kCFStringEncodingUTF8);
	sscanf(valstr, "%f", &paramValue);
	return paramValue;
}

void		CAAUParameter::SetValue(	AUParameterListenerRef			inListener, 
									void *							inObject,
									Float32							inValue)
{
    // clip inValue as: maxValue >= inValue >= minValue before setting
    Float32 valueToSet = inValue;
    if (valueToSet > mParamInfo.maxValue)
        valueToSet = mParamInfo.maxValue;
    if (valueToSet < mParamInfo.minValue)
        valueToSet = mParamInfo.minValue;
    
	AUParameterSet(inListener, inObject, this, valueToSet, 0);
}



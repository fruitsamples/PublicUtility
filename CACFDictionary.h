/*	Copyright: 	� Copyright 2003 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
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
	CACFDictionary.h
 
=============================================================================*/
#if !defined(__CACFDictionary_h__)
#define __CACFDictionary_h__

//=============================================================================
//	Includes
//=============================================================================

//	System Includes
#include <CoreFoundation/CoreFoundation.h>

//=============================================================================
//	CACFDictionary
//=============================================================================

class CACFDictionary 
{

//	Construction/Destruction
public:
							CACFDictionary(bool inRelease) : mCFDictionary(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)), mRelease(inRelease), mMutable(true) {}
							CACFDictionary(const CFDictionaryRef inCFDictionary, bool inRelease) : mCFDictionary(const_cast<CFMutableDictionaryRef>(inCFDictionary)), mRelease(inRelease), mMutable(true) {}
							CACFDictionary(const CFMutableDictionaryRef inCFDictionary, bool inRelease) : mCFDictionary(inCFDictionary), mRelease(inRelease), mMutable(true) {}
							CACFDictionary(const CACFDictionary& inDictionary) : mCFDictionary(inDictionary.mCFDictionary), mRelease(inDictionary.mRelease), mMutable(inDictionary.mMutable) { if(mRelease && (mCFDictionary != NULL)) { CFRetain(mCFDictionary); } }
	CACFDictionary&			operator=(const CACFDictionary& inDictionary) { mCFDictionary = inDictionary.mCFDictionary; mRelease = inDictionary.mRelease; mMutable = inDictionary.mMutable; if(mRelease && (mCFDictionary != NULL)) { CFRetain(mCFDictionary); } return *this; } 
							~CACFDictionary() { if(mRelease && (mCFDictionary != NULL)) { CFRelease(mCFDictionary); } }
		
//	Attributes
public:
	bool					IsValid() const { return mCFDictionary != NULL; }
	bool					IsMutable() const { return mMutable;}
	
	bool					WillRelease() const { return mRelease; }
	void					ShouldRelease(bool inRelease) { mRelease = inRelease; }
	
	CFDictionaryRef			GetDict() const { return mCFDictionary; }
	CFDictionaryRef			GetCFDictionary() const { return mCFDictionary; }
	CFDictionaryRef			CopyCFDictionary() const { if(mCFDictionary != NULL) { CFRetain(mCFDictionary); } return mCFDictionary; }

	CFMutableDictionaryRef	GetMutableDict() { return mCFDictionary; }
	CFMutableDictionaryRef	GetCFMutableDictionary() const { return mCFDictionary; }
	CFMutableDictionaryRef	CopyCFMutableDictionary() const { if(mCFDictionary != NULL) { CFRetain(mCFDictionary); } return mCFDictionary; }

	CFPropertyListRef		AsPropertyList() const { return mCFDictionary; }
	OSStatus				GetDictIfMutable(CFMutableDictionaryRef& outDict) const { OSStatus theAnswer = -1; if(mMutable) { outDict = mCFDictionary; theAnswer = 0; } return theAnswer; }

//	Item Operations
public:
	bool					HasKey(const CFStringRef inKey) const;
	
	bool					GetSInt32(const CFStringRef inKey, SInt32& outValue) const;
	bool					GetUInt32(const CFStringRef inKey, UInt32& outValue) const;
	bool					GetSInt64(const CFStringRef inKey, SInt64& outValue) const;
	bool					GetUInt64(const CFStringRef inKey, UInt64& outValue) const;
	bool					GetFloat32(const CFStringRef inKey, Float32& outValue) const;
	bool					GetFloat64(const CFStringRef inKey, Float64& outValue) const;
	bool					GetString(const CFStringRef inKey, CFStringRef& outValue) const;	
	bool					GetArray(const CFStringRef inKey, CFArrayRef& outValue) const;	
	bool					GetDictionary(const CFStringRef inKey, CFDictionaryRef& outValue) const;	
	bool					GetCFType(const CFStringRef inKey, CFTypeRef& outValue) const;
	
	bool					GetCFTypeWithCStringKey(const char* inKey, CFTypeRef& outValue) const;

	bool					AddSInt32(const CFStringRef inKey, SInt32 inValue);
	bool					AddUInt32(const CFStringRef inKey, UInt32 inValue);
	bool					AddSInt64(const CFStringRef inKey, SInt64 inValue);
	bool					AddUInt64(const CFStringRef inKey, UInt64 inValue);
	bool					AddFloat32(const CFStringRef inKey, Float32 inValue);
	bool					AddFloat64(const CFStringRef inKey, Float64 inValue);
	bool					AddNumber(const CFStringRef inKey, const CFNumberRef inValue);
	bool					AddString(const CFStringRef inKey, const CFStringRef inValue);
	bool					AddArray(const CFStringRef inKey, const CFArrayRef inValue);
	bool					AddDictionary(const CFStringRef inKey, const CFDictionaryRef inValue);
	bool					AddCFType(const CFStringRef inKey, const CFTypeRef inValue);
	
	bool					AddCFTypeWithCStringKey(const char* inKey, const CFTypeRef inValue);
	bool					AddCString(const CFStringRef inKey, const char* inValue);

	void					Show () { CFShow (mCFDictionary); }
	
//	Implementation
private:
	CFMutableDictionaryRef 	mCFDictionary;
	bool					mRelease;
	bool					mMutable;
};

#endif //__CACFDictionary_h__

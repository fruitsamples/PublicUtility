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
	CAPresetFile.cpp

=============================================================================*/

#include "CAPresetFile.h"

static const CFStringRef partString = CFSTR("part");

CAPresetFile::CAPresetFile (CFStringRef inPresetFileName) {
    mPresetFileName = inPresetFileName;
    
    CFRetain(mPresetFileName);
}

CAPresetFile::~CAPresetFile () {
    CFRelease(mPresetFileName);
}

Boolean CAPresetFile::SavePreset (CFPropertyListRef inData) {
	CFURLRef fileURL = CFURLCreateWithFileSystemPath(	kCFAllocatorDefault,    
                                                        mPresetFileName,		// file path name
                                                        kCFURLPOSIXPathStyle,	// interpret as POSIX path        
                                                        false );				// is it a directory?
    if (fileURL == NULL) return false;
    
    // Convert the property list into XML data.
	CFDataRef xmlData = CFPropertyListCreateXMLData (kCFAllocatorDefault, inData);
    if (xmlData == NULL) return false;
    
	// Write the XML data to the file.
    OSStatus result = noErr;
	Boolean status = CFURLWriteDataAndPropertiesToResource(	fileURL,			// URL to use
                                                            xmlData,			// data to write
                                                            NULL,
                                                            &result);
    
	CFRelease(xmlData);
	CFRelease(fileURL);
    
    return status;
}

Boolean CAPresetFile::RestorePreset (CFPropertyListRef *outPreset, Boolean *outIsPartPreset) {
	CFURLRef fileURL = CFURLCreateWithFileSystemPath(	kCFAllocatorDefault,    
                                                        mPresetFileName,		// file path name
                                                        kCFURLPOSIXPathStyle,	// interpret as POSIX path        
                                                        false );
    if (fileURL == NULL) return false;
	
	CFDataRef         resourceData = NULL;
	SInt32            errorCode;
    
   // Read the XML file.
   Boolean status = CFURLCreateDataAndPropertiesFromResource (	kCFAllocatorDefault,
                                                                fileURL,
                                                                &resourceData,	// place to put file data
                                                                NULL,
                                                                NULL,
                                                                &errorCode	);
        if (!status || errorCode) {
            CFRelease (fileURL);
            if (resourceData) CFRelease (resourceData);
            return false;
        }
    
	CFStringRef errStr = NULL;
	*outPreset = CFPropertyListCreateFromXMLData (	kCFAllocatorDefault,
                                                    resourceData,
                                                    kCFPropertyListImmutable,
                                                    &errStr	);
        if (*outPreset == NULL) {
            CFRelease (fileURL);
            if (resourceData) CFRelease (resourceData);
            return false;
        }
    
    if (errStr) {
        CFShow (errStr);
        CFRelease (errStr);
    }
    
    // is this part or global data?
    *outIsPartPreset = false;
    if (CFGetTypeID(*outPreset) == CFDictionaryGetTypeID()) {
        *outIsPartPreset = CFDictionaryContainsKey ((CFDictionaryRef)*outPreset, partString);
    }
    
	CFRelease (fileURL);
	CFRelease (resourceData);
    
	return true;
}

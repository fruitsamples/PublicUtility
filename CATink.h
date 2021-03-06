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
	CATink.h

=============================================================================*/
#if !defined(__CATink_h__)
#define __CATink_h__

//=============================================================================
//	Includes
//=============================================================================

//	System Includes
#include <CoreServices/CoreServices.h>

//=============================================================================
//	CATink
//
//	A Tink is a small jump island that can make one function appear as if it
//	has many addresses.
//=============================================================================

template <class F>
class CATink
{

public:
	CATink(F proc)
	{
		/* thanks to CodeWarrior for the nice disassembler
			lis		r11,0x1234
		00000000: 3D601234  lis        r11,4660
			ori		r11,r11,0x5678
		00000004: 616B5678  ori        r11,r11,$5678
			mtctr	r11
		00000008: 7D6903A6  mtctr      r11
			bctr
		0000000C: 4E800420  bctr
		*/
		UInt32 p = UInt32(proc);
		mCode[0] = 0x3D600000 | (p >> 16);
		mCode[1] = 0x616B0000 | (p & 0xFFFF);
		mCode[2] = 0x7D6903A6;
		mCode[3] = 0x4E800420;
		
		MakeDataExecutable(mCode, sizeof(mCode));
	}
	
	operator F () { return F(mCode); }

private:
	UInt32		mCode[4];

};

#endif

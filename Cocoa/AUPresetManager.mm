/*	Copyright © 2007 Apple Inc. All Rights Reserved.
	
	Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
			Apple Inc. ("Apple") in consideration of your agreement to the
			following terms, and your use, installation, modification or
			redistribution of this Apple software constitutes acceptance of these
			terms.  If you do not agree with these terms, please do not use,
			install, modify or redistribute this Apple software.
			
			In consideration of your agreement to abide by the following terms, and
			subject to these terms, Apple grants you a personal, non-exclusive
			license, under Apple's copyrights in this original Apple software (the
			"Apple Software"), to use, reproduce, modify and redistribute the Apple
			Software, with or without modifications, in source and/or binary forms;
			provided that if you redistribute the Apple Software in its entirety and
			without modifications, you must retain this notice and the following
			text and disclaimers in all such redistributions of the Apple Software. 
			Neither the name, trademarks, service marks or logos of Apple Inc. 
			may be used to endorse or promote products derived from the Apple
			Software without specific prior written permission from Apple.  Except
			as expressly stated in this notice, no other rights or licenses, express
			or implied, are granted by Apple herein, including but not limited to
			any patent rights that may be infringed by your derivative works or by
			other works in which the Apple Software may be incorporated.
			
			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
			MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
			THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
			FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
			OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
			
			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
			OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
			SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
			INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
			MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
			AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
			STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
			POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	AUPresetManager.mm
	PlayPen
 
=============================================================================*/

#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>

#import "CAComponent.h"
#import "CAAUPresetFile.h"

#import "AUPresetManager.h"
#import "CAFileBrowser_Protected.h"

@implementation AUPresetManager

#pragma mark ____ PRIVATE FUNCTIONS ____
- (void)privGetFactoryPresets {
    // dump old presets
    [mFactoryPresets removeAllObjects];
    
	// can't do anything if no AU selected
 	if (mAudioUnit == NULL) return;
	
    NSMutableArray *presetArray = [NSMutableArray array];
	[mFactoryPresets setObject:presetArray forKey:NSLocalizedStringFromTable(@"Factory", @"AUInspector", @"Factory Preset group heading")];
    
    // get new presets    
    CFArrayRef factoryPresets = NULL;
    UInt32 dataSize = sizeof(factoryPresets);
    
	ComponentResult result = AudioUnitGetProperty (	mAudioUnit,
													kAudioUnitProperty_FactoryPresets,
													kAudioUnitScope_Global,
													0,
													&factoryPresets,
													&dataSize	);
	
    if ((result == noErr) && (factoryPresets != NULL)) {
        int count = CFArrayGetCount(factoryPresets);
        AUPreset *		currentPreset;
        
        for (int i = 0; i < count; ++i) {
            currentPreset = (AUPreset*) CFArrayGetValueAtIndex (factoryPresets, i);
            [presetArray addObject:[NSDictionary dictionaryWithObject:[NSNumber numberWithLong:currentPreset->presetNumber] forKey:(NSString *)(currentPreset->presetName)]];
        }
        
        CFRelease(factoryPresets);
    }
}

- (BOOL)privHasFactoryPresets {
	NSArray *valueArray = [mFactoryPresets allValues];
	if ([valueArray count] <= 0) return NO;
	NSArray *factoryPresetArray = (NSArray *)[valueArray objectAtIndex:0];
	return [factoryPresetArray count] > 0;
}

#pragma mark ____ INIT / DEALLOC ____
- (id)initWithFrame:(NSRect)frame shouldScanNetworkForFiles:(BOOL)shouldScanNetworkForFiles {
	[super initWithFrame:frame shouldScanNetworkForFiles:shouldScanNetworkForFiles];
	
	[self setTitle:NSLocalizedStringFromTable(@"AudioUnit Presets", @"AUInspector", @"Audio Unit Preset manager title")];
    
    mFactoryPresets = [[NSMutableDictionary alloc] init];
        
    return self;
}

- (id)initWithFrame:(NSRect)frame {
    return [self initWithFrame:frame shouldScanNetworkForFiles:NO];
}

- (void)dealloc {
    [mFactoryPresets release];
	delete mPresetFile; // can call delete on NULL
	[super dealloc];
}

#pragma mark ____ PUBLIC FUNCTIONS ____
- (void)setAU:(AudioUnit)inAU {
    mAudioUnit = inAU;
    
	[self unsetCAFileHandlingObject];
    if (mPresetFile) delete mPresetFile;
    mPresetFile = new CAAUPresetFile (CAComponent(mAudioUnit), [self scansNetworkForFiles]);
    
    [self privGetFactoryPresets];
	[self setCAFileHandlingObject:mPresetFile];
	[self reloadData];
}

- (BOOL)savePresetWithName:(NSString *)inPresetName asChildOf:(id)item {
    AUPreset presetToSet;
    
    presetToSet.presetName = (CFStringRef)inPresetName;
    presetToSet.presetNumber = -1;
    
    ComponentResult result = AudioUnitSetProperty (	mAudioUnit,
                                                    kAudioUnitProperty_PresentPreset,
                                                    kAudioUnitScope_Global,
                                                    0,
                                                    &presetToSet,
                                                    sizeof(AUPreset)	);
    if (result != noErr) 
		return NO;
    
    // now get classInfo & write to file
    CFPropertyListRef classInfo = NULL;
    UInt32 dataSize = sizeof(CFPropertyListRef);
    result = AudioUnitGetProperty (	mAudioUnit,
                                    kAudioUnitProperty_ClassInfo,
                                    kAudioUnitScope_Global,
                                    0,
                                    &classInfo,
                                    &dataSize);
    if (result != noErr)
		return NO;
    
    BOOL retVal = [self writePropertyList:classInfo withName:inPresetName asChildOfItem:item];
	
    if (classInfo) CFRelease (classInfo);
	
	[self rescanFiles];
	
	return retVal;
}

- (void)rescanFiles {
    [self setAU:mAudioUnit];
}

#pragma mark ____ PROTECTED FUNCTIONS ____
- (BOOL)itemWasActivated:(id)object 
{
    if ([object isKindOfClass:[NSDictionary class]])
	{
		// FACTORY PRESETS
        id obj = [[(NSDictionary *)object allValues] objectAtIndex:0];
        if (![obj isKindOfClass:[NSNumber class]]) return NO;
		
		NSString *presetName = (NSString *)[[(NSDictionary *)object allKeys] objectAtIndex:0];
		UInt32 presetNumber = [(NSNumber *)obj longValue];
		
		AUPreset presetToSet;
		presetToSet.presetName = (CFStringRef)presetName;
		presetToSet.presetNumber = presetNumber;
		
		ComponentResult result = AudioUnitSetProperty (	mAudioUnit,
														kAudioUnitProperty_PresentPreset,
														kAudioUnitScope_Global,
														0,
														&presetToSet,
														sizeof(AUPreset)	);
		if (result != noErr)
			return NO;
    }
	else
	{
		// LOCAL/USER/NETWORK PRESETS
		CFPropertyListRef preset = [self readPropertyListForItem:object];
		if (preset == NULL) return NO;
		
		// set preset in AU
		AUPreset presetToSet;
		
		CFTreeRef tree = (CFTreeRef)[(NSValue *)object pointerValue];
		mPresetFile->GetNameCopy(tree, presetToSet.presetName);
		presetToSet.presetNumber = -1;
		
		ComponentResult result = AudioUnitSetProperty (	mAudioUnit,
														kAudioUnitProperty_ClassInfo,
														kAudioUnitScope_Global,
														0,
														&preset,
														sizeof(CFPropertyListRef));
		CFRelease(preset);
		CFRelease(presetToSet.presetName);
		if (result != noErr)
			return NO;
	}
	
	// notify parameter listeners that they should re-scan unit
	AudioUnitParameter changedUnit;
	changedUnit.mAudioUnit = mAudioUnit;
	changedUnit.mParameterID = kAUParameterListener_AnyParameter;
	if (AUParameterListenerNotify (NULL, NULL, &changedUnit) != noErr)
		return NO;
	
	return YES;
}

- (NSString *)fileExtension
{
	return (NSString *)CAAUPresetFile::kAUPresetFileExtension;
}

- (NSString *)nameKeyString
{
	return (NSString *)CAAUPresetFile::kAUPresetNameKeyString;
}

- (BOOL)shouldAllowItemRenaming:(id)inItem
{
	return (![inItem isKindOfClass:[NSDictionary class]]);
}

#pragma mark ____NSOutlineView.DataSource overrides____
// override outlineView dataSource methods so we can integrate factory presets
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item {
    // TOP LEVEL?
    if (item == nil) {
		if ([self privHasFactoryPresets]) {
			if (index == 0) return mFactoryPresets;
			return [super outlineView:outlineView child:(index - 1) ofItem:item];
		} else {
			return [super outlineView:outlineView child:index ofItem:item];
		}
    }
    
    // FACTORY PRESET?
    if ([item isKindOfClass:[NSDictionary class]]) {
		NSArray *array = [(NSDictionary*)item allValues];
		if ([array count] <= 0) return nil;
        id obj = [array objectAtIndex:0];
        
        if ([obj isKindOfClass:[NSArray class]])
            return [(NSArray *)obj objectAtIndex:index];
        else
            return obj;	// NSDictionary *
    }
    
    // ELSE SUPERCLASS HANDLING
    return [super outlineView:outlineView child:index ofItem:item];
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item {
    // TOP LEVEL?
    if (item == nil) {
		int factoryOffset = ([self privHasFactoryPresets] > 0) ? 1 : 0;
        return [super outlineView:outlineView numberOfChildrenOfItem:item] + factoryOffset;
    }
    
    // FACTORY PRESET?
    if ([item isKindOfClass:[NSDictionary class]]) {
		NSArray *array = [(NSDictionary*)item allValues];
		if ([array count] <= 0) return 0;
        id obj = [array objectAtIndex:0];
        
        if ([obj isKindOfClass:[NSArray class]])
            return [(NSArray *)obj count];
        else 
            return 0;
    }
    
    // ELSE SUPERCLASS HANDLING
    return [super outlineView:outlineView numberOfChildrenOfItem:item];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    // TOP LEVEL?
    if (item == nil) return NO;
    
    // FACTORY PRESET?
    if ([item isKindOfClass:[NSDictionary class]]) {
		NSArray *array = [(NSDictionary*)item allValues];
		if ([array count] <= 0) return NO;
        id obj = [array objectAtIndex:0];
        
        return [obj isKindOfClass:[NSArray class]];
    }
    
    // ELSE SUPERCLASS HANDLING
    return [super outlineView:outlineView isItemExpandable:item];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	NSString *title = nil;
	if ([item isKindOfClass:[NSArray class]]) {
		title = NSLocalizedStringFromTable(@"Factory", @"AUInspector", @"Factory Preset group heading");
	} else if ([item isKindOfClass:[NSDictionary class]]) {
        title = (NSString *)[[(NSDictionary *)item allKeys] objectAtIndex:0];
	}
	
	if (title != nil) {
        NSCell *cell = [self cell];
		float fontSize = [[cell font] pointSize];
		
        if ([self outlineView:outlineView isItemExpandable:item]) {
            [cell setFont:[NSFont boldSystemFontOfSize:fontSize]];
        } else {
            [cell setFont:[NSFont systemFontOfSize:fontSize]];
        }
        
        [cell setTitle:title];
        
        return cell;
    }
    
	// ELSE SUPERCLASS HANDLING
    return [super outlineView:outlineView objectValueForTableColumn:tableColumn byItem:item];
}


@end

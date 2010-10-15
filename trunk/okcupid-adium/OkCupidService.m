//
//  OkCupidService.m
//  okcupid-adium
//
//  Created by MyMacSpace on 16/08/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import "OkCupidService.h"
#import "OkCupidAccountViewController.h"
#import "AdiumOkCupidAccount.h"


@implementation OkCupidService

- (Class)accountClass{
	return [AdiumOkCupidAccount class];
}

- (AIAccountViewController *)accountViewController {
	return [OkCupidAccountViewController accountViewController];
}

- (BOOL)supportsProxySettings{
	return NO;
}

- (BOOL)supportsPassword
{
	return YES;
}

- (BOOL)requiresPassword
{
	return YES;
}

- (NSString *)UIDPlaceholder
{
	return @"OkCupid";
}

//Service Description
- (NSString *)serviceCodeUniqueID{
	return @"prpl-bigbrownchunx-okcupid";
}
- (NSString *)serviceID{
	return @"OkCupid";
}
- (NSString *)serviceClass{
	return @"OkCupid";
}
- (NSString *)shortDescription{
	return @"OkCupid";
}
- (NSString *)longDescription{
	return @"OkCupid";
}

- (BOOL)isSocialNetworkingService
{
	return YES;
}


- (NSCharacterSet *)allowedCharacters{
	return [[NSCharacterSet illegalCharacterSet] invertedSet];
}
- (NSCharacterSet *)ignoredCharacters{
	return [NSCharacterSet characterSetWithCharactersInString:@""];
}
- (BOOL)caseSensitive{
	return NO;
}
- (AIServiceImportance)serviceImportance{
	return AIServiceSecondary;
}
- (NSImage *)defaultServiceIconOfType:(AIServiceIconType)iconType
{
	NSImage *image;
	NSString *imagename;
	NSSize imagesize;
	
	if (iconType == AIServiceIconLarge)
	{
		imagename = @"okcupid";
		imagesize = NSMakeSize(48,48);
	} else {
		imagename = @"okcupid-small";
		imagesize = NSMakeSize(16,16);
	}
	
	image = [NSImage imageNamed:(imagename)
					   forClass:[self class] loadLazily:YES];
	[image setSize:imagesize];
	return image;
}

@end

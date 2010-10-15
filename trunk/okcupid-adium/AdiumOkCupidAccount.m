//
//  AdiumOkCupidAccount.m
//  okcupid-adium
//
//  Created by MyMacSpace on 16/08/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import <Adium/AIHTMLDecoder.h>
#import "AdiumOkCupidAccount.h"


@implementation AdiumOkCupidAccount

- (const char*)protocolPlugin
{
	return "prpl-bigbrownchunx-okcupid";
}

- (BOOL)connectivityBasedOnNetworkReachability
{
	return YES;
}

- (NSString *)host
{
	return @"www.okcupid.com";
}

- (int)port
{
	return 80;
}

- (NSString *)encodedAttributedString:(NSAttributedString *)inAttributedString forListObject:(AIListObject *)inListObject
{
	NSString *temp = [AIHTMLDecoder encodeHTML:inAttributedString
									   headers:YES
									  fontTags:NO
							includingColorTags:NO
								 closeFontTags:NO
									 styleTags:NO
					closeStyleTagsOnFontChange:NO
								encodeNonASCII:NO
								  encodeSpaces:NO
									imagesPath:nil
							 attachmentsAsText:YES
					 onlyIncludeOutgoingImages:NO
								simpleTagsOnly:YES
								bodyBackground:NO
						   allowJavascriptURLs:NO];
	return temp;
}


- (BOOL)canSendOfflineMessageToContact:(AIListContact *)inContact
{
	//shortcut parent method
	return NO;
}

@end



#import "OkCupidPlugin.h"
#import "OkCupidService.h"


@implementation OkCupidPlugin

- (void)installPlugin
{
    purple_init_okcupid_plugin();
	service = [[OkCupidService alloc] init];
}

- (void)uninstallPlugin
{
	[service release]; service = nil;
}

@end

#ifdef __cplusplus
#import "vision-camera-plugin-anpr.h"
#endif

#ifdef RCT_NEW_ARCH_ENABLED
#import "RNVisionCameraPluginAnprSpec.h"

@interface VisionCameraPluginAnpr : NSObject <NativeVisionCameraPluginAnprSpec>
#else
#import <React/RCTBridgeModule.h>

@interface VisionCameraPluginAnpr : NSObject <RCTBridgeModule>
#endif

@end

// Stand-in for UIKit and Foundation, only for preflight/check_ios_sources.sh (syntax check of
// host_ios.mm without Xcode). Declares just what host_ios.mm uses.
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef signed char BOOL;
#define YES ((BOOL)1)
#define NO ((BOOL)0)
#define nil nullptr
typedef double CGFloat;
struct CGPoint { CGFloat x, y; }; struct CGSize { CGFloat width, height; };
struct CGRect { CGPoint origin; CGSize size; };
extern "C" CGRect CGRectInset(CGRect, CGFloat, CGFloat);
typedef long NSInteger; typedef unsigned long NSUInteger;
typedef uint64_t dispatch_time_t; typedef struct dispatch_queue_s* dispatch_queue_t;
#define DISPATCH_TIME_NOW (0ull)
#define NSEC_PER_MSEC 1000000ull
extern "C" dispatch_time_t dispatch_time(dispatch_time_t, int64_t);
extern "C" dispatch_queue_t dispatch_get_main_queue(void);
extern "C" void dispatch_after(dispatch_time_t, dispatch_queue_t, void (^)(void));
@class NSString;
@protocol NSObject @end
__attribute__((objc_root_class)) @interface NSObject <NSObject>
+ (instancetype)alloc; - (instancetype)init; + (Class)class;
@end
@interface NSString : NSObject
+ (instancetype)stringWithUTF8String:(const char*)s; @property(readonly) const char* UTF8String;
@end
@interface NSArray<T> : NSObject @property(readonly) T firstObject; @end
@interface NSBundle : NSObject + (NSBundle*)mainBundle; @property(readonly) NSString* privateFrameworksPath; @end
typedef NSInteger NSProcessInfoThermalState;
@interface NSProcessInfo : NSObject + (NSProcessInfo*)processInfo; @property(readonly) NSProcessInfoThermalState thermalState;
@property(readonly, getter=isLowPowerModeEnabled) BOOL lowPowerModeEnabled; @property(readonly) unsigned long long physicalMemory; @end
enum { NSDocumentDirectory = 9 }; enum { NSUserDomainMask = 1 };
extern "C" NSArray<NSString*>* NSSearchPathForDirectoriesInDomains(NSUInteger, NSUInteger, BOOL);
extern "C" NSString* NSStringFromClass(Class);
@interface UIColor : NSObject @property(class, readonly) UIColor* systemBackgroundColor; @property(class, readonly) UIColor* labelColor; @end
@interface UIResponder : NSObject @end
@interface UICoordinateSpaceObj : NSObject @property(readonly) CGRect bounds; @end
@interface UIScene : UIResponder @end
@interface UIWindowScene : UIScene @property(readonly) UICoordinateSpaceObj* coordinateSpace; @end
@interface UISceneSession : NSObject @property(readonly) NSString* role; @end
@interface UISceneConnectionOptions : NSObject @end
@interface UISceneConfiguration : NSObject - (instancetype)initWithName:(NSString*)n sessionRole:(NSString*)r; @property Class delegateClass; @end
@interface UIView : UIResponder - (instancetype)initWithFrame:(CGRect)f; @property(strong) UIColor* backgroundColor; - (void)addSubview:(UIView*)v; @end
@interface UILabel : UIView @property NSInteger numberOfLines; @property NSInteger textAlignment; @property(strong) UIColor* textColor; @property(copy) NSString* text; @end
enum { NSTextAlignmentCenter = 1 };
@interface UIViewController : UIResponder @property(strong) UIView* view; @end
@interface UIWindow : UIView - (instancetype)initWithWindowScene:(UIWindowScene*)s; @property(strong) UIViewController* rootViewController; - (void)makeKeyAndVisible; @end
@interface UIDevice : NSObject + (UIDevice*)currentDevice; @property(readonly) NSString* model; @property(readonly) NSString* systemName; @property(readonly) NSString* systemVersion; @end
@interface UIApplication : UIResponder + (UIApplication*)sharedApplication; @property BOOL idleTimerDisabled; @end
@protocol UIApplicationDelegate <NSObject> @end
@protocol UIWindowSceneDelegate <NSObject> @end
extern "C" int UIApplicationMain(int, char*[], NSString*, NSString*);

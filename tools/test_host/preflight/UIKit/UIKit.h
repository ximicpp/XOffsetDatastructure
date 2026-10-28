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
@interface NSArray<T> : NSObject @property(readonly) T firstObject;
+ (instancetype)arrayWithObjects:(const id[])objects count:(NSUInteger)cnt; @end
typedef double NSTimeInterval;
@interface NSTimer : NSObject
+ (NSTimer*)scheduledTimerWithTimeInterval:(NSTimeInterval)ti target:(id)t selector:(SEL)s userInfo:(id)u repeats:(BOOL)r; @end
@interface NSBundle : NSObject + (NSBundle*)mainBundle; @property(readonly) NSString* privateFrameworksPath; @end
typedef NSInteger NSProcessInfoThermalState;
@interface NSProcessInfo : NSObject + (NSProcessInfo*)processInfo; @property(readonly) NSProcessInfoThermalState thermalState;
@property(readonly, getter=isLowPowerModeEnabled) BOOL lowPowerModeEnabled; @property(readonly) unsigned long long physicalMemory; @end
enum { NSDocumentDirectory = 9 }; enum { NSUserDomainMask = 1 };
extern "C" NSArray<NSString*>* NSSearchPathForDirectoriesInDomains(NSUInteger, NSUInteger, BOOL);
extern "C" NSString* NSStringFromClass(Class);
@interface UIColor : NSObject @property(class, readonly) UIColor* systemBackgroundColor; @property(class, readonly) UIColor* labelColor;
@property(class, readonly) UIColor* secondaryLabelColor; @property(class, readonly) UIColor* systemOrangeColor;
@property(class, readonly) UIColor* systemBlueColor; @end
typedef CGFloat UIFontWeight;
extern "C" const UIFontWeight UIFontWeightRegular, UIFontWeightSemibold, UIFontWeightBold;
@interface UIFont : NSObject + (UIFont*)monospacedDigitSystemFontOfSize:(CGFloat)s weight:(UIFontWeight)w;
+ (UIFont*)monospacedSystemFontOfSize:(CGFloat)s weight:(UIFontWeight)w; @end
@interface NSLayoutConstraint : NSObject + (void)activateConstraints:(NSArray*)a; @end
@interface NSLayoutAnchor : NSObject - (NSLayoutConstraint*)constraintEqualToAnchor:(NSLayoutAnchor*)a constant:(CGFloat)c; @end
@interface UILayoutGuide : NSObject @property(readonly) NSLayoutAnchor* leadingAnchor; @property(readonly) NSLayoutAnchor* trailingAnchor;
@property(readonly) NSLayoutAnchor* topAnchor; @end
@interface UIResponder : NSObject @end
@interface UICoordinateSpaceObj : NSObject @property(readonly) CGRect bounds; @end
@interface UIScene : UIResponder @end
@interface UIWindowScene : UIScene @property(readonly) UICoordinateSpaceObj* coordinateSpace; @end
@interface UISceneSession : NSObject @property(readonly) NSString* role; @end
@interface UISceneConnectionOptions : NSObject @end
@interface UISceneConfiguration : NSObject - (instancetype)initWithName:(NSString*)n sessionRole:(NSString*)r; @property Class delegateClass; @end
@interface UIView : UIResponder - (instancetype)initWithFrame:(CGRect)f; @property(strong) UIColor* backgroundColor; - (void)addSubview:(UIView*)v;
@property BOOL translatesAutoresizingMaskIntoConstraints; @property(readonly) UILayoutGuide* safeAreaLayoutGuide;
@property(readonly) NSLayoutAnchor* leadingAnchor; @property(readonly) NSLayoutAnchor* trailingAnchor; @property(readonly) NSLayoutAnchor* topAnchor; @end
@interface UILabel : UIView @property NSInteger numberOfLines; @property NSInteger textAlignment; @property(strong) UIColor* textColor; @property(copy) NSString* text;
@property(strong) UIFont* font; @property BOOL adjustsFontSizeToFitWidth; @property CGFloat minimumScaleFactor; @end
enum { UILayoutConstraintAxisVertical = 1 };
@interface UIStackView : UIView - (instancetype)initWithArrangedSubviews:(NSArray*)v; @property NSInteger axis; @property CGFloat spacing;
- (void)setCustomSpacing:(CGFloat)s afterView:(UIView*)v; @end
enum { NSTextAlignmentCenter = 1 };
@interface UIViewController : UIResponder @property(strong) UIView* view; - (void)viewDidLoad; @end
@interface UIWindow : UIView - (instancetype)initWithWindowScene:(UIWindowScene*)s; @property(strong) UIViewController* rootViewController; - (void)makeKeyAndVisible; @end
@interface UIDevice : NSObject + (UIDevice*)currentDevice; @property(readonly) NSString* model; @property(readonly) NSString* systemName; @property(readonly) NSString* systemVersion; @end
@interface UIApplication : UIResponder + (UIApplication*)sharedApplication; @property BOOL idleTimerDisabled; @end
@protocol UIApplicationDelegate <NSObject> @end
@protocol UIWindowSceneDelegate <NSObject> @end
extern "C" int UIApplicationMain(int, char*[], NSString*, NSString*);

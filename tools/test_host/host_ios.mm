// iOS test host app: a UIKit shell around host_core (see host_core.hpp). Launched by
// xo_host_run.py (devicectl on a phone, simctl on the simulator) with the arguments
// "xorun ID" or "xopack ID REASON"; launched from the home screen it only shows that
// it is idle. The command runs on its own thread and ends the process.
#include "host_core.hpp"

#import <UIKit/UIKit.h>

#include <string>
#include <vector>

namespace {
std::vector<std::string> g_args;
}

@interface XOSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation XOSceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    UIWindowScene* windowScene = (UIWindowScene*)scene;
    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
    UIViewController* vc = [[UIViewController alloc] init];
    vc.view.backgroundColor = UIColor.systemBackgroundColor;
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectInset(windowScene.coordinateSpace.bounds, 24, 120)];
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    label.textColor = UIColor.labelColor;
    std::string text = "XOffset test host\n\n";
    if (g_args.size() >= 2) text += g_args[0] + " " + g_args[1] + "\n\nrunning; the app closes by itself when done";
    else text += "idle (started without a command)";
    label.text = [NSString stringWithUTF8String:text.c_str()];
    [vc.view addSubview:label];
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    if (g_args.size() < 2) return;
    // keep the screen on while a run lasts: a locked phone suspends the app
    UIApplication.sharedApplication.idleTimerDisabled = YES;

    xoffset_test_host::Options o;
    NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    o.root = std::string(docs.UTF8String) + "/xo";
    o.modules_dir = NSBundle.mainBundle.privateFrameworksPath.UTF8String;
    o.module_suffix = ".dylib";
    UIDevice* d = UIDevice.currentDevice;
    NSProcessInfo* p = NSProcessInfo.processInfo;
    o.platform_info = std::string("ui_device ") + d.model.UTF8String + " " + d.systemName.UTF8String + " " +
                      d.systemVersion.UTF8String + "\nthermal_state " + std::to_string((long)p.thermalState) +
                      "\nlow_power_mode " + (p.lowPowerModeEnabled ? "1" : "0") +
                      "\nphysical_memory " + std::to_string((unsigned long long)p.physicalMemory) + "\n";
    static std::vector<char*> argv;
    for (auto& a : g_args) argv.push_back(a.data());
    argv.push_back(nullptr);
    xoffset_test_host::start_command_thread((int)g_args.size(), argv.data(), o);
}
@end

@interface XOAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation XOAppDelegate
- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:(UISceneSession*)session
                                   options:(UISceneConnectionOptions*)options {
    UISceneConfiguration* c = [[UISceneConfiguration alloc] initWithName:@"Default" sessionRole:session.role];
    c.delegateClass = XOSceneDelegate.class;
    return c;
}
@end

int main(int argc, char* argv[]) {
    // devicectl and simctl pass the launch arguments after the program name; the
    // system may add its own (e.g. -NSDocumentRevisionsDebugMode YES) before them
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (g_args.empty() && a != "xorun" && a != "xopack") continue;
        g_args.push_back(a);
    }
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(XOAppDelegate.class));
    }
}

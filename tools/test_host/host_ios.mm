// iOS test host app: a UIKit shell around host_core (see host_core.hpp). Launched by
// xo_host_run.py (devicectl on a phone, simctl on the simulator) with the arguments
// "xoserve" (stay up and run the driver's requests one after another), "xorun ID" or
// "xopack ID REASON" (one run, then the process ends); launched from the home screen it
// serves too. While serving it shows the totals of the ledger: data received, runs,
// the demo clients' snapshots. During a timed run (slg bench, memory probe) the screen
// does not change.
#include "host_core.hpp"

#import <UIKit/UIKit.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {
std::vector<std::string> g_args;

std::string human_bytes(unsigned long long n) {
    char b[32];
    if (n >= (1ull << 30)) std::snprintf(b, sizeof b, "%.2f GB", n / double(1ull << 30));
    else if (n >= (1ull << 20)) std::snprintf(b, sizeof b, "%.1f MB", n / double(1ull << 20));
    else if (n >= 1024) std::snprintf(b, sizeof b, "%.1f KB", n / 1024.0);
    else std::snprintf(b, sizeof b, "%llu B", n);
    return b;
}

std::string with_commas(unsigned long long n) {
    std::string s = std::to_string(n);
    for (int i = int(s.size()) - 3; i > 0; i -= 3) s.insert(std::size_t(i), ",");
    return s;
}

NSString* ns(const std::string& s) { return [NSString stringWithUTF8String:s.c_str()]; }
}  // namespace

// The serving screen: a large total of the data received, then runs and the demo.
@interface XODashboardController : UIViewController
@end

@implementation XODashboardController {
    UILabel* _state;
    UILabel* _received;
    UILabel* _receivedDetail;
    UILabel* _runs;
    UILabel* _demo;
    UILabel* _recent;
    NSTimer* _timer;
    BOOL _showingPaused;
}

- (UILabel*)label:(CGFloat)size weight:(UIFontWeight)weight {
    UILabel* l = [[UILabel alloc] init];
    l.numberOfLines = 0;
    l.textColor = UIColor.labelColor;
    l.font = [UIFont monospacedDigitSystemFontOfSize:size weight:weight];
    l.adjustsFontSizeToFitWidth = YES;
    l.minimumScaleFactor = 0.5;
    return l;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    UILabel* title = [self label:17 weight:UIFontWeightSemibold];
    title.text = @"XOffset 测试主机";
    title.textColor = UIColor.secondaryLabelColor;
    _state = [self label:17 weight:UIFontWeightRegular];
    UILabel* receivedTitle = [self label:15 weight:UIFontWeightRegular];
    receivedTitle.text = @"累计接收数据";
    receivedTitle.textColor = UIColor.secondaryLabelColor;
    _received = [self label:44 weight:UIFontWeightBold];
    _receivedDetail = [self label:15 weight:UIFontWeightRegular];
    _receivedDetail.textColor = UIColor.secondaryLabelColor;
    _runs = [self label:17 weight:UIFontWeightRegular];
    _demo = [self label:17 weight:UIFontWeightRegular];
    _recent = [self label:13 weight:UIFontWeightRegular];
    _recent.font = [UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];
    _recent.textColor = UIColor.secondaryLabelColor;
    UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
        title, _state, receivedTitle, _received, _receivedDetail, _runs, _demo, _recent
    ]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 10;
    [stack setCustomSpacing:28 afterView:_state];
    [stack setCustomSpacing:2 afterView:receivedTitle];
    [stack setCustomSpacing:24 afterView:_receivedDetail];
    [stack setCustomSpacing:24 afterView:_demo];
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:stack];
    UILayoutGuide* g = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:g.leadingAnchor constant:24],
        [stack.trailingAnchor constraintEqualToAnchor:g.trailingAnchor constant:-24],
        [stack.topAnchor constraintEqualToAnchor:g.topAnchor constant:24],
    ]];
    [self refresh];
    _timer = [NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector:@selector(refresh) userInfo:nil repeats:YES];
}

- (void)refresh {
    // a timed run: draw the paused state once, then leave the screen alone
    if (xoffset_test_host::timed_run() && _showingPaused) return;
    const xoffset_test_host::Dashboard d = xoffset_test_host::dashboard();
    _showingPaused = d.quiet;
    if (d.quiet) {
        _state.text = ns("计时中：" + d.current + "\n界面暂停刷新，结束后更新");
        _state.textColor = UIColor.systemOrangeColor;
        return;
    }
    char line[256];
    if (!d.current.empty()) {
        std::snprintf(line, sizeof line, "运行中：%s（%.0f 秒）", d.current.c_str(), d.current_seconds);
        _state.text = ns(line);
        _state.textColor = UIColor.systemBlueColor;
    } else {
        _state.text = d.serving ? @"空闲，等待电脑发来任务" : @"启动中";
        _state.textColor = UIColor.secondaryLabelColor;
    }
    const unsigned long long demo_bytes = d.demo_bytes + d.live_bytes;
    _received.text = ns(human_bytes(d.bytes_in + demo_bytes));
    _receivedDetail.text = ns("任务包 " + human_bytes(d.bytes_in) + " · demo 快照 " + human_bytes(demo_bytes) +
                              " · 发回结果 " + human_bytes(d.bytes_out));
    std::snprintf(line, sizeof line, "已运行 %llu 个（通过 %llu，失败 %llu）%s", d.runs, d.passed, d.failed,
                  d.restarts ? (" · app 重启 " + std::to_string(d.restarts) + " 次").c_str() : "");
    _runs.text = ns(line);
    std::string demo = "demo 快照 " + with_commas(d.demo_snapshots + d.live_snapshots) + " 帧";
    if (d.demo_runs) demo += "（" + std::to_string(d.demo_runs) + " 次客户端运行）";
    if (!d.current.empty() && d.live_snapshots) demo += "\n本次 " + with_commas(d.live_snapshots) + " 帧 " + human_bytes(d.live_bytes);
    _demo.text = ns(demo);
    std::string recent;
    for (const auto& r : d.recent) {
        const std::size_t a = r.find('\t'), b = r.find('\t', a + 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        const std::string status = r.substr(a + 1, b - a - 1);
        const double sec = std::atof(r.c_str() + b + 1) / 1000.0;
        std::snprintf(line, sizeof line, "%s %-28.28s %7.1fs%s\n", status == "exit 0" ? "✓" : "✗", r.substr(0, a).c_str(), sec,
                      status == "exit 0" ? "" : ("  " + status).c_str());
        recent += line;
    }
    _recent.text = ns(recent);
}
@end

@interface XOSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation XOSceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    UIWindowScene* windowScene = (UIWindowScene*)scene;
    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
    // started from the home screen: serve
    if (g_args.empty()) g_args.push_back("xoserve");
    const bool serving = g_args[0] == "xoserve";
    if (serving) {
        self.window.rootViewController = [[XODashboardController alloc] init];
    } else {
        UIViewController* vc = [[UIViewController alloc] init];
        vc.view.backgroundColor = UIColor.systemBackgroundColor;
        UILabel* label = [[UILabel alloc] initWithFrame:CGRectInset(windowScene.coordinateSpace.bounds, 24, 120)];
        label.numberOfLines = 0;
        label.textAlignment = NSTextAlignmentCenter;
        label.textColor = UIColor.labelColor;
        std::string text = "XOffset test host\n\n" + g_args[0] + (g_args.size() >= 2 ? " " + g_args[1] : std::string()) +
                           "\n\nrunning; the app closes by itself when done";
        label.text = [NSString stringWithUTF8String:text.c_str()];
        [vc.view addSubview:label];
        self.window.rootViewController = vc;
    }
    [self.window makeKeyAndVisible];
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
    // start once the launch has settled (the first frames, the system's own work on its
    // queues): a program that counts allocations sees a quiet process
    static xoffset_test_host::Options host_options;
    host_options = o;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        xoffset_test_host::start_command_thread((int)g_args.size(), argv.data(), host_options);
    });
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
    xoffset_test_host::keep_host_allocations_on_this_thread();  // UIKit's thread
    // devicectl and simctl pass the launch arguments after the program name; the
    // system may add its own (e.g. -NSDocumentRevisionsDebugMode YES) before them
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (g_args.empty() && a != "xorun" && a != "xopack" && a != "xoserve") continue;
        g_args.push_back(a);
    }
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(XOAppDelegate.class));
    }
}

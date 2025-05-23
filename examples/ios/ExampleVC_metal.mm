//
//  ExampleVC_metal.mm
//  ccapDemo
//
//  Created by wy on 2025/05
//

#import "ExampleVC_metal.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <ccap/ccap.h>

#pragma GCC diagnostic ignored "-Wimplicit-retain-self"

@interface ExampleVC_metal ()<MTKViewDelegate>
@property (nonatomic, strong) MTKView* mtkView;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (weak, nonatomic) IBOutlet UIButton* backBtn;
@end

@implementation ExampleVC_metal
{
    ccap::Provider _provider;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    auto names = _provider.findDeviceNames();
    for (auto& name : names)
    {
        NSLog(@"Find Camera Device: %s", name.c_str());
    }

    _provider.set(ccap::PropertyName::PixelFormatInternal, ccap::PixelFormat::NV12f);
    _provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::NV12f);
    _provider.set(ccap::PropertyName::Width, 1920);
    _provider.set(ccap::PropertyName::Height, 1080);

    _provider.setNewFrameCallback([=](std::shared_ptr<ccap::Frame> newFrame) {
        NSLog(@"New frame received, resolution: %dx%d, format: %s",
              newFrame->width, newFrame->height, ccap::pixelFormatToString(newFrame->pixelFormat).data());
        return false;
    });

    _provider.open();
    
    [_backBtn setTitle:@"Will imp next version" forState:UIControlStateNormal];
    auto frame = _backBtn.frame;
    frame.size.width = 250;
    [_backBtn setFrame:frame];
}

- (IBAction)backClicked:(id)sender
{
    [self clear];
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)didReceiveWillResignActive:(NSNotification*)notification
{
    
}

- (void)didReceiveDidBecomeActive:(NSNotification*)notification
{
}

- (void)clear
{
}

- (void)displayLinkCallback
{
}

- (void)mtkView:(MTKView*)mtkView drawableSizeWillChange:(CGSize)size
{
}

- (void)drawInMTKView:(MTKView*)mtkView
{
}
@end

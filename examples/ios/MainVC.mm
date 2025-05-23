//
//  MainVC.m
//  ccapDemo
//
//  Created by wy on 2025/05
//

#import "MainVC.h"

#include <string>

@interface MainVC ()
@property (weak, nonatomic) IBOutlet UIScrollView* scrollView;
@end

@implementation MainVC
{
    BOOL _initCalled;
}

- (void)updateScrollViewLayout
{
    CGRect rt{};

    if (@available(macCatalyst 13.0, *))
    {
        UIWindow* window = self.view.window;
        if (window)
        {
            rt = window.bounds;
        }
    }
    
    if(CGRectIsEmpty(rt))
    {
        rt = [UIScreen mainScreen].bounds;
    }

    if (_initCalled && CGRectEqualToRect(self->_scrollView.frame, rt))
    {
        return;
    }

    if(!_initCalled)
    {
        [self->_scrollView setScrollEnabled:YES];
        [self->_scrollView setBackgroundColor:[UIColor blackColor]];
    }
    
    [self->_scrollView setFrame:rt];
    NSArray* subviews = [_scrollView subviews];
    CGFloat bottomOrginX = rt.size.width * 0.2f;
    CGFloat buttonWidth = rt.size.width * 0.65f;
    CGFloat buttonHeight = 40.0f;
    CGFloat verticalSpacing = 25.0f;
    CGFloat initialY = 40.0f;

    CGFloat totalContentHeight = initialY + subviews.count * (buttonHeight + verticalSpacing) - verticalSpacing;

    [UIView performWithoutAnimation:^{
        [subviews enumerateObjectsUsingBlock:^(UIView* view, NSUInteger idx, BOOL* stop) {
            CGRect frame = CGRectMake(bottomOrginX, initialY + idx * (buttonHeight + verticalSpacing), buttonWidth, buttonHeight);
            [view setFrame:frame];
            
            if(!_initCalled)
            {
                [self applyStyleToView:view];
            }            
        }];
    }];

    self->_scrollView.contentSize = CGSizeMake(rt.size.width, totalContentHeight);

    CGFloat verticalInset = MAX((rt.size.height - totalContentHeight) / 2, 0);
    self->_scrollView.contentInset = UIEdgeInsetsMake(verticalInset, 0, verticalInset, 0);

    _initCalled = YES;
}

- (void)applyStyleToView:(UIView*)view
{
    view.layer.shadowColor = [UIColor redColor].CGColor;
    view.layer.shadowOffset = CGSizeMake(2, 2);
    view.layer.borderWidth = 1.5;
    view.layer.borderColor = [UIColor blueColor].CGColor;
    view.layer.shadowOpacity = 1.0;
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    [self updateScrollViewLayout];
}

@end

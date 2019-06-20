//
//  ViewController.m
//  LogServerDemo
//
//  Created by tiny on 2019/6/17.
//  Copyright © 2019 gw. All rights reserved.
//  尝试将日志写入到文件中

#import "ViewController.h"
#import "LogServer.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    
    //开启日志读写功能
    [[LogServer shareInstance] writeLog];
}



@end

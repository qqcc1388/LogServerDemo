//
//  LogServer.h
//  LogServerDemo
//
//  Created by tiny on 2019/6/18.
//  Copyright © 2019 gw. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface LogServer : NSObject

+(instancetype)shareInstance;

///开启写日志
-(void)writeLog;

///上传日志
-(void)uploadLog;

///清空本地日志
-(void)clearLog;

@end

NS_ASSUME_NONNULL_END

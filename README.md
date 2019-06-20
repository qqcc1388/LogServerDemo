# LogServerDemo

日志系统主要包含两个部分
1. 本地保存
    我们知道NSLog打印的日志一般都是直接输出到控制台，开发人员可以在控制台直接看到实时打印的log，既然可以在控制台输出，那么能否将日志输出到其他地方呢，比如说自己定义的text文件？答案是肯定的 ，在iOS中可以通过一些方法将文件重定向到指定输出位置：
```
    freopen([filePath cStringUsingEncoding:NSASCIIStringEncoding],"a+", stdout);
    freopen([filePath cStringUsingEncoding:NSASCIIStringEncoding], "a+", stderr);
```
实现了上述方法，就可以将log重定向到指定的filePath中了，既然文件中保存到了本地，那么怎么处理，怎么加密完全看你们公司的业务需求咯

2. 日志上传
    日志上传首先需要将本地日志打包成zip格式的文件，这样可以减小上传的size，通过ZipArchive将日志文件打包好，通过afnetworking的 formData将文件上传了
```
    [formData appendPartWithFileURL:url name:@"zipFile" fileName:[NSString stringWithFormat:@"%@.zip",[self getCurrentTime]] mimeType:@"multipart/form-data" error:nil];

```

具体部分实现代码如下：
```
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

```
```

#import "LogServer.h"
#import <ZipArchive/ZipArchive.h>

#define MaxCount   30    //只保留最近的30条数据
#define MaxSize    (1024*4)   //4M

@interface LogServer (){
    NSString *filePath_;  //文件存放路径
}

@end

static LogServer * instance = nil;

@implementation LogServer

+(instancetype)shareInstance{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[LogServer alloc] init];
        [instance initLogServer];
    });
    return instance;
}

-(void)initLogServer{
    NSString *path = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    //初始化log路径
    filePath_ = [path stringByAppendingPathComponent:@"Log"];
    NSFileManager *manager = [NSFileManager defaultManager];
    if (![manager fileExistsAtPath:filePath_]) {
        //如果Log文件夹不存在，就创建文件夹
        [manager createDirectoryAtPath:filePath_ withIntermediateDirectories:YES attributes:nil error:nil];
    }
}

-(void)writeLog{
    // Do any additional setup after loading the view.
    //如果文件有zip格式则直接移除掉 可能是上次打包上传的
    [self removeZipFile];
    
    //如果文件夹中的文件超过最大限制 咋移除旧数据
    [self removeExpireFile];
    
    //加密文件
    [self encryptLastFile];

    //将日志写入到文件中
    NSString *filePath = [filePath_ stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.text",[self fileNameAtTime]]];
    freopen([filePath cStringUsingEncoding:NSASCIIStringEncoding],"a+", stdout);
    freopen([filePath cStringUsingEncoding:NSASCIIStringEncoding], "a+", stderr);
    
    NSLog(@"filePath:%@",filePath);
    //异常日志报告
    NSSetUncaughtExceptionHandler(&caughtExceptionHandler);
}

-(void)uploadLog{
    //将需要上传的文件加密zip格式上传到服务器
    NSArray *sortArray = [self sortFileAscending];
    if (sortArray.count > 0) {
        ZipArchive *zip = [[ZipArchive alloc] init];
        NSString *path = [filePath_ stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.zip",[self fileNameAtTime]]];
        BOOL ret = [zip CreateZipFile2:path];
        if (ret) {
            //便利所有文件内容
            for (int i = sortArray.count - 1; i>=0; i--) {
                NSString *fileName = sortArray[i];
                NSString *filePath = [filePath_ stringByAppendingPathComponent:fileName];
                [zip addFileToZip:filePath newname:fileName];
                //判断zip文件大小 如果文件大小大于最大限制 则停止剩余文件的压缩
                long long zipSize = [self getFileSize:path];
                if (zipSize > MaxSize) {
                    break;
                }
            }
            [zip CloseZipFile2];
            //将zip文件上传到服务器
    
//            afnetworking  文件上传到指定服务器即可
        }
    }
}

-(void)clearLog{
    NSFileManager *manager = [NSFileManager defaultManager];
    if ([manager fileExistsAtPath:filePath_]) {
        NSError *error = nil;
        [manager removeItemAtPath:filePath_ error:&error];
        if (error) {
            NSLog(@"日志清空失败：%@",error);
        }else{
            NSLog(@"日志清空成功");
        }
    }
}

#pragma mark - private
#pragma mark 异常处理日志
void caughtExceptionHandler(NSException *exception){
    /**
     *  获取异常崩溃信息
     */
    NSArray *callStack = [exception callStackSymbols];
    NSString *reason = [exception reason];
    NSString *name = [exception name];
    NSString *content = [NSString stringWithFormat:@"========异常错误报告========\nname:%@\nreason:\n%@\ncallStackSymbols:\n%@",name,reason,[callStack componentsJoinedByString:@"\n"]];
    NSLog(@"%@",content);
}

#pragma mark 获取文件大小
-(long long)getFileSize:(NSString *)path{
    unsigned long long fileLength = 0;
    NSNumber *fileSize;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if([fileManager fileExistsAtPath:path]){
        NSDictionary *fileAttributes = [fileManager attributesOfItemAtPath:path error:nil];
        
        if ((fileSize = [fileAttributes objectForKey:NSFileSize])) {
            fileLength = [fileSize unsignedLongLongValue]; //单位是 B
        }
        return fileLength / 1024;
    }
    return 0;
}

#pragma mark 用时间创建文件名
-(NSString *)fileNameAtTime{
    NSDate *date = [NSDate date];
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.dateFormat = @"yyyy-MM-dd-HH-mm-ss";
    NSString *timeStr = [formatter stringFromDate:date];
    
    return timeStr;
}

#pragma mark 加密文件内容
-(void)encryptLastFile{
    //这里根据自己的需要将文件内容进行加密
    NSFileManager *manager = [NSFileManager defaultManager];
    NSArray *sortArray = [self sortFileAscending];
    if (sortArray.count > 0) {
        NSString *lastPath = sortArray.lastObject;
        NSString *lastFilePath = [filePath_ stringByAppendingPathComponent:lastPath];
        NSData *data = [manager contentsAtPath:lastFilePath];
        NSString *base64String = [data base64EncodedStringWithOptions:0];
        //
        [base64String writeToFile:lastFilePath atomically:YES];
    }
}

#pragma mark 移除过期数据
-(void)removeExpireFile{
    NSFileManager *manager = [NSFileManager defaultManager];
    //将文件中数据排序
    NSArray *sortFileArray = [self sortFileAscending];
    if(sortFileArray.count > MaxCount){
        NSInteger count = sortFileArray.count - MaxCount;
        //因为是升序排在前面的都是比较老的数据 移除前面的count个数据即可
        for (int i = 0; i < count; i++) {
            NSString *path = sortFileArray[i];
            NSString *filePath = [filePath_ stringByAppendingPathComponent:path];
            [manager removeItemAtPath:filePath error:nil];
        }
    }
}

#pragma mark 移除zip格式文件
-(void)removeZipFile{
    NSFileManager *manager = [NSFileManager defaultManager];
    if ([manager fileExistsAtPath:filePath_]) {  //如果文件夹存在
        NSArray *filesArray = [manager contentsOfDirectoryAtPath:filePath_ error:nil];
        NSString *filePath = filePath_;
        [filesArray enumerateObjectsUsingBlock:^(NSString *  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            if (![obj containsString:@".text"]) {
                NSString *nPath = [filePath stringByAppendingPathComponent:obj];
                [manager removeItemAtPath:nPath error:nil];
            }
        }];
    }
}

#pragma mark 将文件夹中的文件按照创建时间进行排序
-(NSArray *)sortFileAscending{
    //拿到文件
    NSFileManager *manager = [NSFileManager defaultManager];
    if ([manager fileExistsAtPath:filePath_]) {  //如果文件夹存在
        NSArray *filesArray = [manager contentsOfDirectoryAtPath:filePath_ error:nil];
        NSString *filePath = filePath_;
        //便利文件夹中的所有文件
        NSArray *array = [filesArray sortedArrayUsingComparator:^NSComparisonResult(NSString*  _Nonnull file1, NSString *  _Nonnull file2) {
            //file1 file2全路径
            NSString *file1Path = [filePath stringByAppendingPathComponent:file1];
            NSString *file2Path = [filePath stringByAppendingPathComponent:file2];
            
            //file1 file2 Info
            NSDictionary *file1Dict = [[NSFileManager defaultManager] attributesOfItemAtPath:file1Path error:nil];
            NSDictionary *file2Dict = [[NSFileManager defaultManager] attributesOfItemAtPath:file2Path error:nil];
            
            //file1CreatTime file2CreatTime
            NSDate *file1Date = [file1Dict objectForKey:NSFileCreationDate];
            NSDate *file2Date = [file2Dict objectForKey:NSFileCreationDate];
            //升序
            return [file1Date compare:file2Date];
        }];
        return  array;
    }
    return nil;
}


@end

```

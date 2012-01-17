//
//  main.m
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "AdventureParserDelegate.h"



int main (int argc, char *argv[]) {
    @autoreleasepool {
        ssize_t readSize = 1<<12;
        ssize_t nRead = 0;
        ssize_t totalRead = 0;
        NSMutableData *inData = [[NSMutableData alloc] initWithLength:readSize];
        
        do {
            nRead = read(STDIN_FILENO, (void *)[inData bytes] + totalRead, readSize);
            totalRead += nRead;
            if (nRead == readSize) [inData increaseLengthBy:readSize];
        } while (nRead);
        
        AdventureParserDelegate *delegate = [[AdventureParserDelegate alloc] init];
        NSXMLParser *parser = [[NSXMLParser alloc] initWithData:inData];
        parser.delegate = delegate;
        [parser parse];
        
        printf("%s\n", [[delegate.topItem description] UTF8String]);
    }
    
    return 0;
}
//
//  Item.m
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import "Item.h"

@implementation Item
- (id) initWithParent:(Item *)parent {
    if ((self = [super init])) {
        self.parent = parent;
        self.dependencies = [[NSMutableArray alloc] init];
    }
    return self;
}

- (NSString *) _dependencyString {    
    NSMutableString *retval = [[NSMutableString alloc] initWithString:@"("];
    int n = 1;
    for (Item *curItem in self.dependencies) {
        if ([curItem.dependencies count]) {
            [retval appendFormat:@" %@ %@", curItem.name, [curItem _dependencyString]];
        } else {
            [retval appendFormat:@" %@", curItem.name];
        }
        if (n++ != [self.dependencies count]) [retval appendFormat:@","]; 
    }
    [retval appendString:@" )"];
    return retval;
}

- (NSString *) _descriptionAtDepth:(int)n {
    NSMutableString *retval = [[NSMutableString alloc] init];
    for (int i = 0; i < n; i++) [retval appendString:@"\t"];
    [retval appendFormat:@"Item \"%@\"", self.name];
    if ([self.dependencies count]) [retval appendFormat:@" depends on: %@", [self _dependencyString]];
    if (self.piledOn) [retval appendFormat:@", piled on:\n%@", [self.piledOn _descriptionAtDepth:n + 1]];
    return retval;
}

- (NSString *) description {
    return [self _descriptionAtDepth:0];
}

- (void) addDependency:(Item *)depends {
    [self.dependencies addObject:depends];
}

@end

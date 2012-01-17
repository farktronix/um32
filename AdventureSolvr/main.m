//
//  main.m
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "AdventureParserDelegate.h"
#import "Item.h"

@interface ItemMapper : NSObject {
    Item *_topItem;
    Item *_rootItem;
    NSMutableDictionary *_allItems;
    NSMutableArray *_necessaryItems;
}
- (id)initWithTopItem:(Item *)topItem;
@end

@implementation ItemMapper
+ (NSSet *)rootItems {
    return [NSSet setWithObjects:@"keypad", @"USB Cable", @"display", @"jumper shunt", @"progress bar", @"power cord", nil];
}

- (id)initWithTopItem:(Item *)topItem {
    if ((self = [super init])) {
        _topItem = [topItem retain];
        _necessaryItems = [[NSMutableArray alloc] init];
        _allItems = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void) _findRootItem {
    Item *curItem = _topItem;
    while (curItem.piledOn != nil) {
        [_allItems setObject:curItem forKey:curItem.name];
        if ([[[self class] rootItems] containsObject:curItem.name]) _rootItem = curItem;
        curItem = curItem.piledOn;
    }   
}

- (void) _collectNecessaryItemsFromItem:(Item *)item parentMissingItems:(NSArray *)parentItems {
    for (Item *dependItem in item.dependencies) {
        BOOL shouldAdd = YES;
        for (Item *missingItem in parentItems) {
            if ([missingItem.name isEqualToString:dependItem.name]) shouldAdd = NO;
        }
        if (shouldAdd) [_necessaryItems addObject:dependItem.name];
        [self _collectNecessaryItemsFromItem:[_allItems objectForKey:dependItem.name] parentMissingItems:dependItem.dependencies];
    }
}

- (void) createActions {
    [self _findRootItem];
    [_necessaryItems addObject:_rootItem.name];
    [self _collectNecessaryItemsFromItem:_rootItem parentMissingItems:nil];
    
    NSMutableArray *inventory = [[NSMutableArray alloc] init];
    Item *curItem = _topItem;
    while (curItem.parent != _rootItem) {
        printf("take %s %s\n", curItem.adjective ? [curItem.adjective UTF8String] : "", [curItem.name UTF8String]);
        [inventory addObject:curItem];
        if (![_necessaryItems containsObject:curItem.name]) {
            printf("incinerate %s %s\n", curItem.adjective ? [curItem.adjective UTF8String] : "", [curItem.name UTF8String]);
            [_necessaryItems removeObject:curItem.name];
            [inventory removeObject:curItem];
        } else {
            for (Item *inventoryItem in inventory) {
                for (Item *dependency in inventoryItem.dependencies) {
                    if ([dependency.name isEqualToString:curItem.name]) {
                        printf("combine %s %s and %s%s\n", curItem.adjective ? [curItem.adjective UTF8String] : "", [curItem.name UTF8String],
                               inventoryItem.adjective ? [inventoryItem.adjective UTF8String] : "", [inventoryItem.name UTF8String]);
                        [inventory removeObject:curItem];
                        goto done;
                    }
                }
            }
            for (Item *inventoryItem in [inventory copy]) {
                for (Item *dependency in curItem.dependencies) {
                    if ([dependency.name isEqualToString:inventoryItem.name]) {
                        printf("combine %s %s and %s%s\n", curItem.adjective ? [curItem.adjective UTF8String] : "", [curItem.name UTF8String],
                               inventoryItem.adjective ? [inventoryItem.adjective UTF8String] : "", [inventoryItem.name UTF8String]);
                        [inventory removeObject:inventoryItem];
                    }
                }
            }
        }
done:
        
        curItem = curItem.piledOn;
    }
}
@end

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
        
        ItemMapper *mapper = [[ItemMapper alloc] initWithTopItem:delegate.topItem];
        [mapper createActions];
    }
    
    return 0;
}
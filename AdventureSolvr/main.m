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

@interface ItemMapper : NSObject
@property (nonatomic, retain) Item *topItem;
@property (nonatomic, retain) Item *rootItem;
@property (nonatomic, retain) Item *treeRoot;

@property (nonatomic, retain) NSMutableDictionary *allItems;
@property (nonatomic, retain) NSMutableArray *necessaryItems;

- (id)initWithTopItem:(Item *)topItem;

@end

@implementation ItemMapper
+ (NSSet *)rootItems {
    return [NSSet setWithObjects:@"keypad", @"USB cable", @"display", @"jumper shunt", @"progress bar", @"power cord", @"MOSFET", @"status LED", @"RS232 adapter", @"EPROM burner", @"battery", nil];
}

- (id)initWithTopItem:(Item *)topItem {
    if ((self = [super init])) {
        self.topItem = topItem;
        self.necessaryItems = [[NSMutableArray alloc] init];
        self.allItems = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void) _findRootItem {
    NSSet *rootItems = [[self class] rootItems];
    Item *curItem = self.topItem;
    while (curItem.piledOn != nil) {
        [self.allItems setObject:curItem forKey:curItem.name];
        if ([rootItems containsObject:curItem.name]) {
            self.rootItem = curItem;
        }
        curItem = curItem.piledOn;
    }   
}

- (void) _removeMissingDependenciesForItem:(Item *)item fromTree:(Item *)tree {
    for (Item *dep in item.dependencies) {
        for (Item *treeDep in [tree.dependencies copy]) {
            if ([treeDep.name isEqualToString:dep.name]) [tree.dependencies removeObject:treeDep];
            [self _removeMissingDependenciesForItem:dep fromTree:treeDep];
        }
    }
}

- (Item *) _topoSortItems:(Item *)item withParent:(Item *)parent {    
    Item *root = [[Item alloc] initWithParent:parent];
    root.name = item.name;
    root.adjective = item.adjective;
    
    // Build up a tree of all dependencies
    for (Item *dep in item.dependencies) {
        if ([self.allItems objectForKey:dep.name] == nil) continue;
        [root addDependency:[self _topoSortItems:[self.allItems objectForKey:dep.name] withParent:root]];
    }
    
    // Go through all of the dependencies of dependencies and remove their items from the tree
    for (Item *dep in item.dependencies) {
        for (Item *treeDep in root.dependencies) {
            [self _removeMissingDependenciesForItem:dep fromTree:treeDep];
        }
    }
    
    return root;
}

- (void) _addItemsNamesInTree:(Item *)tree toArray:(NSMutableArray *)array {
    for (Item *dep in tree.dependencies) {
        [array addObject:dep.name];
        [self _addItemsNamesInTree:dep toArray:array];
    }
}

- (void) _printTree:(Item *)tree depth:(int)n {
    for (int i = 0; i < n; i++) printf(" ");
    printf("%s\n", [tree.fullName UTF8String]);
    for (Item *child in tree.dependencies) {
        [self _printTree:child depth:n+1];
    }
}

- (Item *) _itemWithName:(NSString *)name inTree:(Item *)tree {
    if (tree == nil || name == nil) return nil;
    if ([tree.name isEqualToString:name]) return tree;
    Item *result = nil;
    for (Item *child in tree.dependencies) {
        result = [self _itemWithName:name inTree:child];
        if (result) break;
    }
    return result;
}

- (void) createActions {
    printf("%s\n\n", [[self.topItem description] UTF8String]);
    
    [self _findRootItem];
    self.treeRoot = [self _topoSortItems:self.rootItem withParent:nil];
    [self _printTree:self.treeRoot depth:0];
    
    printf("\n\n");
    
    [self.necessaryItems addObject:self.treeRoot.name];
    [self _addItemsNamesInTree:self.treeRoot toArray:self.necessaryItems];
    
    NSMutableArray *inventory = [[NSMutableArray alloc] init];
    Item *curItem = self.topItem;
    BOOL rootItemAcquired = NO;
    while (!rootItemAcquired || [[self.treeRoot dependencies] count]) {
        if ([curItem.name isEqualToString:self.treeRoot.name]) rootItemAcquired = YES;
        printf("take %s\n", [curItem.fullName UTF8String]);
        [inventory addObject:curItem];
        if (![self.necessaryItems containsObject:curItem.name]) {
            printf("incinerate %s\n", [curItem.fullName UTF8String]);
            [self.necessaryItems removeObject:curItem.name];
            [inventory removeObject:curItem];
        } else {
            for (Item *invItem in [inventory copy]) {
                Item *treeItem = [self _itemWithName:invItem.name inTree:self.treeRoot];
                for (Item *treeChild in [treeItem.dependencies copy]) {
                    for (Item *curItem in [inventory copy]) {
                        if ([treeChild.dependencies count] == 0 &&
                            ![curItem.name isEqualToString:treeItem.name] &&
                             [curItem.name isEqualToString:treeChild.name]) {
                            printf("combine %s and %s\n", [curItem.name UTF8String], [treeItem.name UTF8String]);
                            [inventory removeObject:curItem];
                            [treeItem.dependencies removeObject:treeChild];
                        }
                    }
                }
            }
        }        
        curItem = curItem.piledOn;
    }
}
@end

int main (int argc, char *argv[]) {
    @autoreleasepool {
        NSMutableData *inData = nil;
        if (argc < 2) {
            ssize_t readSize = 1<<12;
            ssize_t nRead = 0;
            ssize_t totalRead = 0;
            inData = [[NSMutableData alloc] initWithLength:readSize];
            
            do {
                nRead = read(STDIN_FILENO, (void *)[inData bytes] + totalRead, readSize);
                totalRead += nRead;
                if (nRead == readSize) [inData increaseLengthBy:readSize];
            } while (nRead);
        } else {
            inData = [NSMutableData dataWithContentsOfFile:[[[NSProcessInfo processInfo] arguments] objectAtIndex:1]];
        }
        
        if (inData == nil) {
            printf("Couldn't load data. Try again.\n");
            return 1;
        }
        
        AdventureParserDelegate *delegate = [[AdventureParserDelegate alloc] init];
        NSXMLParser *parser = [[NSXMLParser alloc] initWithData:inData];
        parser.delegate = delegate;
        [parser parse];
        
        ItemMapper *mapper = [[ItemMapper alloc] initWithTopItem:delegate.topItem];
        [mapper createActions];
    }
    
    return 0;
}
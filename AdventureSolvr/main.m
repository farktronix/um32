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
    
    Item *_treeRoot;
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
    NSSet *rootItems = [[self class] rootItems];
    Item *curItem = _topItem;
    while (curItem.piledOn != nil) {
        [_allItems setObject:curItem forKey:curItem.name];
        if ([rootItems containsObject:curItem.name]) _rootItem = curItem;
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
        [root addDependency:[self _topoSortItems:[_allItems objectForKey:dep.name] withParent:root]];
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
    printf("%s\n\n", [[_topItem description] UTF8String]);
    
    [self _findRootItem];
    _treeRoot = [self _topoSortItems:_rootItem withParent:nil];
    [self _printTree:_treeRoot depth:0];
    
    printf("\n\n");
    
    [_necessaryItems addObject:_treeRoot.name];
    [self _addItemsNamesInTree:_treeRoot toArray:_necessaryItems];
    
    NSMutableArray *inventory = [[NSMutableArray alloc] init];
    Item *curItem = _topItem;
    while (curItem.parent != _rootItem) {
        printf("take %s\n", [curItem.fullName UTF8String]);
        [inventory addObject:curItem];
        if (![_necessaryItems containsObject:curItem.name]) {
            printf("incinerate %s\n", [curItem.fullName UTF8String]);
            [_necessaryItems removeObject:curItem.name];
            [inventory removeObject:curItem];
        } else {
            for (Item *invItem in [inventory copy]) {
                Item *treeItem = [self _itemWithName:invItem.name inTree:_treeRoot];
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
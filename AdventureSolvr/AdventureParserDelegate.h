//
//  AdventureParserDelegate.h
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef enum {
    psBaseState = 0,
    psItemState,
    psNameState,
    psKindState,
} parseState;

@interface Item : NSObject 
- (id) initWithParent:(Item *)parent;
@property (nonatomic, retain) Item *parent;

@property (nonatomic,retain) NSString *name;
@property (nonatomic,retain) NSMutableArray *dependencies;
@property (nonatomic, assign) Item *piledOn;

- (void) addDependency:(Item *)depends;

@end

@interface AdventureParserDelegate : NSObject <NSXMLParserDelegate>

@property (nonatomic, assign) int depth;
@property (nonatomic, assign) parseState state;
@property (nonatomic, retain) Item *topItem;
@property (nonatomic, retain) Item *currentItem;

@end

//
//  AdventureParserDelegate.h
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Item.h"

typedef enum {
    psBaseState = 0,
    psItemState,
    psNameState,
    psAdjectiveState,
    psKindState,
} parseState;

@interface AdventureParserDelegate : NSObject <NSXMLParserDelegate>

@property (nonatomic, assign) int depth;
@property (nonatomic, assign) parseState state;
@property (nonatomic, retain) Item *topItem;
@property (nonatomic, retain) Item *currentItem;

@end

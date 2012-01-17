//
//  Item.h
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Item : NSObject 
- (id) initWithParent:(Item *)parent;
@property (nonatomic, retain) Item *parent;

@property (nonatomic,copy) NSString *name;
@property (nonatomic,copy) NSString *adjective;
@property (nonatomic,readonly) NSString *fullName; // includes the adjective
@property (nonatomic,retain) NSMutableArray *dependencies;
@property (nonatomic,retain) Item *piledOn;

- (void) addDependency:(Item *)depends;

@end
//
//  AdventureParserDelegate.m
//  AdventureSolvr
//
//  Created by Jacob Farkas on 1/16/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#import "AdventureParserDelegate.h"

@implementation AdventureParserDelegate

- (id) init {
    if ((self = [super init])) {
        self.state = psBaseState;
    }
    return self;
}

- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName attributes:(NSDictionary *)attributeDict {

    self.depth++;
    elementName = [elementName lowercaseString];
    //for (int i=1; i < self.depth; i++) printf(" ");
    //printf("%s\n", [elementName UTF8String]);
    parseState nextState = self.state;
    
    if ([elementName isEqualToString:@"item"]) {
        if (self.state == psBaseState) {
            self.currentItem = [[Item alloc] init];
            self.topItem = self.currentItem;
        }
        nextState = psItemState;
    } 
    
    if (self.state > psBaseState) {
        if ([elementName isEqualToString:@"piled_on"]) {
            Item *nextItem = [[Item alloc] initWithParent:self.currentItem];
            self.currentItem.piledOn = nextItem;
            self.currentItem = nextItem;
            nextState = psItemState;
        }
        else if ([elementName isEqualToString:@"name"]) {
            nextState = psNameState;
        }
        else if ([elementName isEqualToString:@"kind"]) {
            Item *missingItem = [[Item alloc] initWithParent:self.currentItem];
            [self.currentItem addDependency:missingItem];
            self.currentItem = missingItem;
            nextState = psItemState;
        }
    }
    
    self.state = nextState;
}

- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string {
    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    
    if (self.state == psNameState) {
        self.currentItem.name = string;
    }
}

- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName {
    self.depth--;
    
    elementName = [elementName lowercaseString];
    parseState nextState = self.state;
    
    if (self.state > psBaseState) {
        if ([elementName isEqualToString:@"item"] ||
            [elementName isEqualToString:@"kind"]) {
            self.currentItem = self.currentItem.parent;
            nextState = psItemState;
        }
        else if ([elementName isEqualToString:@"name"]) {
            nextState = psItemState;
        }
    }
    
    self.state = nextState;
}
@end

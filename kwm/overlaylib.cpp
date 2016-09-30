//
//  overlaylib.cpp
//  kwm
//
//  Created by Jarrad Whitaker on 30/9/16.
//
//

#include "overlaylib.h"

void testSwiftFunction() {
	Class cl = objc_getClass("overlaylib.CBridge");
	objc_msgSend((id) cl, sel_getUid("printTest"));
}

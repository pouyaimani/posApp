#ifndef ASSETS_H_
#define ASSETS_H_

void *getImgAddr(const char *name);

#define ICON_IDLE_MAIN        getImgAddr("img_pNovin")

#define ICON_MENU             getImgAddr("img_menu")

#define ICON_SWIPE_CARD       getImgAddr("img_swipeCard")





#define SWIPE_CARD_TEXT     "لطفا کارت خود را بکشید"

#endif
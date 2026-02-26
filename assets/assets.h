#ifndef ASSETS_H_
#define ASSETS_H_

void *getImgAddr(const char *name);

#define ICON_IDLE_MAIN              getImgAddr("img_pNovin")

#define ICON_MENU                   getImgAddr("img_menu")

#define ICON_SWIPE_CARD             getImgAddr("img_swipeCard")

#define ICON_BAT_LEV_LOW            getImgAddr("img_bat_empty")
#define ICON_BAT_LEV_1              getImgAddr("img_bat_lev1")
#define ICON_BAT_LEV_2              getImgAddr("img_bat_lev2")
#define ICON_BAT_LEV_3              getImgAddr("img_bat_full")
#define ICON_BAT_CHARGING           getImgAddr("img_bat_charging")

#define ICON_SOUND_VOLUME_0         getImgAddr("img_sound_0")
#define ICON_SOUND_VOLUME_1         getImgAddr("img_sound_1")
#define ICON_SOUND_VOLUME_2         getImgAddr("img_sound_2")
#define ICON_SOUND_VOLUME_3         getImgAddr("img_sound_3")

#define ICON_WIFI_STRENGTH_0        getImgAddr("img_wifi_weak")
#define ICON_WIFI_STRENGTH_1        getImgAddr("img_wifi_1")
#define ICON_WIFI_STRENGTH_2        getImgAddr("img_wifi_2")
#define ICON_WIFI_STRENGTH_3        getImgAddr("img_wifi_3")
#define ICON_WIFI_DISCONNECT        getImgAddr("img_wifi_no")

#define ICON_CELL_STRENGTH_0        getImgAddr("img_cell_0")
#define ICON_CELL_STRENGTH_1        getImgAddr("img_cell_1")
#define ICON_CELL_STRENGTH_2        getImgAddr("img_cell_2")
#define ICON_CELL_STRENGTH_3        getImgAddr("img_cell_3")
#define ICON_CELL_DISCONNECT        getImgAddr("img_cell_diss")



#define SWIPE_CARD_TEXT     "لطفا کارت خود را بکشید"

#endif